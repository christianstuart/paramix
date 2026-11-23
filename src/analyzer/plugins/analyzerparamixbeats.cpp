#include "analyzer/plugins/analyzerparamixbeats.h"

#include <algorithm>
#include <cmath>

namespace mixxx {

namespace {
constexpr int kDefaultBeatsPerBar = 4;
} // namespace

AnalyzerParamixBeats::AnalyzerParamixBeats() = default;

AnalyzerParamixBeats::~AnalyzerParamixBeats() = default;

void AnalyzerParamixBeats::setBeatDetectionHints(
        const BeatDetectionHints& hints) {
    m_hints = hints;
}

bool AnalyzerParamixBeats::initialize(mixxx::audio::SampleRate sampleRate) {
    // The analysis engine needs the total track length, which the
    // sample-rate-only entry point cannot provide.
    (void)sampleRate;
    return false;
}

bool AnalyzerParamixBeats::initialize(mixxx::audio::SampleRate sampleRate,
        mixxx::audio::ChannelCount channelCount,
        SINT frameLength) {
    if (!sampleRate.isValid() || frameLength <= 0 || channelCount.value() == 0) {
        return false;
    }

    m_sampleRate = sampleRate;
    m_channelCount = channelCount.value();
    m_totalFrames = frameLength;
    m_finalized = false;
    m_result = {};

    paramix::analysis::Engine::Config config;
    config.sampleRate = static_cast<int>(sampleRate.value());
    config.channelCount = m_channelCount;
    config.totalFrames = frameLength;
    config.keepTransients = true;
    config.bpmRangeMin = m_hints.bpmRangeMin;
    config.bpmRangeMax = m_hints.bpmRangeMax;
    config.existingBpm = m_hints.existingBpm;
    config.matchExistingBpm = m_hints.matchExistingBpm;
    config.preserveGridOnBpmMismatch = m_hints.hasGrid &&
            m_hints.existingBpm > 0.0 && m_hints.matchExistingBpm;
    m_pEngine = std::make_unique<paramix::analysis::Engine>(config);
    return true;
}

bool AnalyzerParamixBeats::processSamples(const CSAMPLE* pIn, SINT iLen) {
    if (!m_pEngine || pIn == nullptr || iLen <= 0 || m_finalized) {
        return false;
    }
    if (iLen % m_channelCount != 0) {
        return false;
    }
    const int frameCount = static_cast<int>(iLen / m_channelCount);
    m_pEngine->pushInterleaved(pIn, frameCount);
    return true;
}

bool AnalyzerParamixBeats::finalize() {
    if (!m_pEngine) {
        return false;
    }
    if (!m_finalized) {
        m_result = m_pEngine->finish();
        m_finalized = true;
    }
    return m_result.ok;
}

mixxx::Bpm AnalyzerParamixBeats::getBpm() const {
    if (!m_result.ok) {
        return {};
    }
    return mixxx::Bpm(m_result.bpm);
}

mixxx::audio::FramePos AnalyzerParamixBeats::getFirstBeat() const {
    if (!m_result.ok || !m_sampleRate.isValid()) {
        return mixxx::audio::kStartFramePos;
    }
    const double frame = std::round(static_cast<double>(m_result.anchorSeconds) *
            m_sampleRate.toDouble());
    return mixxx::audio::FramePos(std::max(0.0, frame));
}

QVector<mixxx::audio::FramePos> AnalyzerParamixBeats::getBeats() const {
    QVector<mixxx::audio::FramePos> beats;
    if (!m_result.ok || !(m_result.bpm > 0.0f) || m_totalFrames <= 0) {
        return beats;
    }
    const double beatFrames = static_cast<double>(60.0f / m_result.bpm) *
            m_sampleRate.toDouble();
    if (!(beatFrames > 0.0)) {
        return beats;
    }
    double position = static_cast<double>(getFirstBeat().value());
    while (position <= static_cast<double>(m_totalFrames)) {
        beats.append(mixxx::audio::FramePos(std::round(position)));
        position += beatFrames;
    }
    return beats;
}

DownbeatInfo AnalyzerParamixBeats::getDownbeatInfo() const {
    DownbeatInfo info;
    if (!m_result.ok) {
        return info;
    }
    info.valid = true;
    info.beatsPerBar = m_result.beatsPerBar > 0 ? m_result.beatsPerBar
                                                : kDefaultBeatsPerBar;
    info.phase = m_result.downbeatPhase;
    info.confidence = m_result.confidence;
    info.source = pluginInfo().id();
    return info;
}

float AnalyzerParamixBeats::getConfidence() const {
    return m_result.ok ? m_result.confidence : 0.0f;
}

} // namespace mixxx
