#include "paramix/analysis/analysis.h"
#include "paramix/analysis/bpmrange.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "fields.h"
#include "float32.h"
#include "onset.h"
#include "phase.h"
#include "refine.h"
#include "resampler.h"
#include "tempo.h"
#include "window.h"

namespace paramix::analysis {

namespace {

TempoConfig tempoConfigForRange(int minBpm, int maxBpm) {
    TempoConfig config;
    config.centreBpm = f32(f32(minBpm + maxBpm) * f32(0.5f));
    config.halfWidthBpm = f32(f32(maxBpm - minBpm) * f32(0.5f));
    return config;
}

struct TempoChoice {
    TempoResult tempo;
    int minBpm = 88;
    int maxBpm = 175;
    bool hintMismatch = false;
};

/// Traktor's BPM window is user-selected. When the track already has a BPM we
/// prefer the configured window if it reproduces that BPM, otherwise try the
/// other Traktor presets. This reproduces imported grids analyzed with a
/// different window without changing normal fresh analysis.
///
/// The stored BPM may have been rounded by the importer (integer BPMs are
/// common), so a candidate counts as reproducing it when the difference is at
/// most half a BPM. That still rejects a manually halved or doubled BPM.
TempoChoice chooseTempo(const std::vector<double>& positions,
        const Engine::Config& config) {
    TempoChoice choice;
    choice.minBpm = config.bpmRangeMin;
    choice.maxBpm = config.bpmRangeMax;
    if (choice.minBpm <= 0 || choice.maxBpm <= choice.minBpm) {
        const BpmRange fallback = defaultBpmRange();
        choice.minBpm = fallback.minBpm;
        choice.maxBpm = fallback.maxBpm;
    }
    choice.tempo = detectBpm(positions,
            tempoConfigForRange(choice.minBpm, choice.maxBpm));

    const double hint = config.existingBpm;
    if (!config.matchExistingBpm || !(hint > 0.0)) {
        return choice;
    }

    constexpr double kToleranceBpm = 0.5;
    if (choice.tempo.bpm > 0.0f &&
            std::fabs(static_cast<double>(choice.tempo.bpm) - hint) <=
                    kToleranceBpm) {
        return choice;
    }

    bool found = false;
    double bestError = 0.0;
    TempoResult bestTempo;
    int bestMin = choice.minBpm;
    int bestMax = choice.maxBpm;
    for (int i = 0; i < kBpmRangePresetCount; ++i) {
        const BpmRange preset = kBpmRangePresets[i];
        if (preset.automatic) {
            // "Automatic" stands for a window Traktor picks itself, so a
            // stored BPM cannot tell us it was used. Including it would let
            // its wide window win the closeness comparison by luck.
            continue;
        }
        const TempoResult candidate = detectBpm(positions,
                tempoConfigForRange(preset.minBpm, preset.maxBpm));
        if (!(candidate.bpm > 0.0f)) {
            continue;
        }
        const double error = std::fabs(
                static_cast<double>(candidate.bpm) - hint);
        if (!found || error < bestError) {
            found = true;
            bestError = error;
            bestTempo = candidate;
            bestMin = preset.minBpm;
            bestMax = preset.maxBpm;
        }
    }

    if (found && bestError <= kToleranceBpm) {
        choice.tempo = bestTempo;
        choice.minBpm = bestMin;
        choice.maxBpm = bestMax;
        return choice;
    }

    choice.hintMismatch = true;
    return choice;
}

class BurstTracker final : public SampleProcessor {
  public:
    BurstTracker(int burstCount, BurstAnalyzer* analyzer)
            : m_burstCount(burstCount), m_analyzer(analyzer) {
    }

    void onSamples(const float* samples, int count) override {
        for (int i = 0; i < count; ++i) {
            m_ring[static_cast<std::size_t>(m_index & (kWindowLength - 1))] =
                    samples[i];
            if (m_nextBurst < m_burstCount &&
                    m_index == static_cast<std::int64_t>(m_nextBurst) * kHopSize +
                            (kHopSize - 1)) {
                processBurst();
            }
            ++m_index;
        }
    }

    void flush() override {
        // The resampler has already flushed all samples; every burst whose
        // window lies inside the declared length has been processed.
    }

    std::int64_t processedSamples() const {
        return m_index;
    }

  private:
    void processBurst() {
        const std::int64_t last = static_cast<std::int64_t>(m_nextBurst) * kHopSize +
                (kHopSize - 1);
        const std::int64_t start = last - (kWindowLength - 1);
        float window[kWindowLength];
        for (int i = 0; i < kWindowLength; ++i) {
            const std::int64_t index = start + i;
            window[i] = index < 0
                    ? 0.0f
                    : m_ring[static_cast<std::size_t>(index & (kWindowLength - 1))];
        }
        m_analyzer->processBurst(window, m_nextBurst);
        ++m_nextBurst;
    }

    float m_ring[kWindowLength] = {};
    std::int64_t m_index = 0;
    int m_burstCount = 0;
    int m_nextBurst = 0;
    BurstAnalyzer* m_analyzer;
};

} // namespace

struct Engine::Impl {
    explicit Impl(Config cfg)
            : config(cfg) {
        const std::int64_t totalOutput =
                resampledFrameCount(config.totalFrames, config.sampleRate);
        burstCount = totalOutput >= kWindowLength
                ? static_cast<int>((totalOutput - kWindowLength) / kHopSize)
                : 0;
        tracker = std::make_unique<BurstTracker>(burstCount, &analyzer);
        resampler = makeResampler(config.sampleRate, tracker.get());
        mono.reserve(8192);
    }

    void push(const float* frames, int frameCount) {
        if (frameCount <= 0 || frames == nullptr) {
            return;
        }
        const std::int64_t remaining = config.totalFrames - framesPushed;
        if (remaining <= 0) {
            return;
        }
        const int usable = static_cast<int>(
                std::min<std::int64_t>(frameCount, remaining));
        mono.resize(static_cast<std::size_t>(usable));

        if (config.channelCount <= 1) {
            for (int i = 0; i < usable; ++i) {
                mono[static_cast<std::size_t>(i)] = frames[i];
            }
        } else if (config.channelCount == 2) {
            for (int i = 0; i < usable; ++i) {
                const float left = frames[2 * i];
                const float right = frames[2 * i + 1];
                mono[static_cast<std::size_t>(i)] =
                        f32(f32(left + right) * f32(0.5f));
            }
        } else {
            for (int i = 0; i < usable; ++i) {
                float sum = 0.0f;
                for (int channel = 0; channel < config.channelCount; ++channel) {
                    sum = f32(sum + frames[i * config.channelCount + channel]);
                }
                mono[static_cast<std::size_t>(i)] =
                        f32(sum / f32(config.channelCount));
            }
        }
        framesPushed += usable;

        // The resampler consumes mono samples, so the interleaved frame count
        // maps one-to-one onto mono samples here.
        resampler->onSamples(mono.data(), usable);
    }

    Config config;
    int burstCount = 0;
    std::int64_t framesPushed = 0;
    BurstAnalyzer analyzer;
    std::unique_ptr<BurstTracker> tracker;
    std::unique_ptr<SampleProcessor> resampler;
    std::vector<float> mono;
    Result result;
    bool finished = false;
};

Engine::Engine(Config config)
        : m_impl(std::make_unique<Impl>(config)) {
}

Engine::~Engine() = default;

void Engine::pushInterleaved(const float* frames, int frameCount) {
    if (m_impl->finished) {
        return;
    }
    m_impl->push(frames, frameCount);
}

Result Engine::finish() {
    if (m_impl->finished) {
        return m_impl->result;
    }
    m_impl->finished = true;

    m_impl->resampler->flush();

    const std::vector<float>& envelope = m_impl->analyzer.envelope();
    const std::vector<float>& onsets = m_impl->analyzer.onsets();
    const std::vector<float>& bass = m_impl->analyzer.bass();
    if (envelope.empty() || onsets.empty()) {
        return m_impl->result;
    }

    const std::vector<PickedPeak> picked = pickPeaks(envelope);
    const std::vector<RefinedPeak> refined = refinePeaks(picked, onsets);
    const std::vector<TransientField> fields = computeFields(refined, bass);
    if (fields.empty()) {
        return m_impl->result;
    }

    std::vector<double> positions;
    std::vector<float> weights;
    std::vector<float> bassWeights;
    positions.reserve(fields.size());
    weights.reserve(fields.size());
    bassWeights.reserve(fields.size());
    for (const TransientField& field : fields) {
        positions.push_back(field.position);
        weights.push_back(field.weight);
        bassWeights.push_back(field.bassWeight);
    }

    const TempoChoice tempo = chooseTempo(positions, m_impl->config);
    if (tempo.hintMismatch && m_impl->config.preserveGridOnBpmMismatch) {
        // Keep the existing grid rather than overwriting it with a BPM that
        // does not match Traktor's stored value.
        m_impl->result = {};
        m_impl->result.bpmHintMismatch = true;
        return m_impl->result;
    }
    if (!(tempo.tempo.bpm > 0.0f)) {
        m_impl->result = {};
        m_impl->result.bpmHintMismatch = tempo.hintMismatch;
        return m_impl->result;
    }

    const PhaseResult phase = estimatePhase(
            positions, weights, bassWeights, tempo.tempo.bpm);
    if (!phase.ok) {
        return m_impl->result;
    }

    const float denominator = std::max(phase.bestScore, 1e-9f);
    const float margin = std::max(0.0f, (phase.bestScore - phase.secondScore) /
                    denominator);
    const float lengthFactor = std::min(
            1.0f, f32(static_cast<int>(positions.size())) / 100.0f);
    const float confidence = std::min(1.0f, margin * 4.0f) * lengthFactor;

    Result result;
    result.ok = true;
    result.bpm = tempo.tempo.bpm;
    result.anchorSeconds = phase.anchor;
    result.beatsPerBar = 4;
    result.downbeatPhase = 0;
    result.confidence = std::max(0.0f, confidence);
    result.bpmRangeMin = tempo.minBpm;
    result.bpmRangeMax = tempo.maxBpm;
    result.bpmHintMismatch = tempo.hintMismatch;
    if (m_impl->config.keepTransients) {
        result.transients.reserve(fields.size());
        for (const TransientField& field : fields) {
            Transient transient;
            transient.positionSeconds = field.position;
            transient.weight = field.weight;
            transient.bassWeight = field.bassWeight;
            result.transients.push_back(transient);
        }
    }

    m_impl->result = std::move(result);
    return m_impl->result;
}

void Engine::reset() {
    const Config config = m_impl->config;
    m_impl = std::make_unique<Impl>(config);
}

} // namespace paramix::analysis
