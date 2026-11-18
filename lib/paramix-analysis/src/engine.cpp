#include "paramix/analysis/analysis.h"

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
    std::vector<float> bassWeights;
    positions.reserve(fields.size());
    bassWeights.reserve(fields.size());
    for (const TransientField& field : fields) {
        positions.push_back(field.position);
        bassWeights.push_back(field.bassWeight);
    }

    const TempoResult tempo = detectBpm(positions);
    if (!(tempo.bpm > 0.0f)) {
        return m_impl->result;
    }

    const PhaseResult phase = estimatePhase(positions, bassWeights, tempo.bpm);
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
    result.bpm = tempo.bpm;
    result.anchorSeconds = phase.anchor;
    result.beatsPerBar = 4;
    result.downbeatPhase = 0;
    result.confidence = std::max(0.0f, confidence);
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
