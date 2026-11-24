#pragma once

#include <QObject>
#include <memory>

#include "analyzer/plugins/analyzerplugin.h"
#include "paramix/analysis/analysis.h"

namespace mixxx {

/// Opt-in beat detection plugin backed by the self-contained Paramix analysis
/// engine. It provides a native constant-tempo grid and additive downbeat
/// metadata instead of relying on BeatFactory's re-derivation and snapping.
class AnalyzerParamixBeats : public AnalyzerBeatsPlugin {
  public:
    static AnalyzerPluginInfo pluginInfo() {
        return AnalyzerPluginInfo(
                QStringLiteral("paramix-analysis-engine:1"),
                QStringLiteral("christianstuart"),
                QObject::tr("Paramix Analysis Engine"),
                true);
    }

    AnalyzerParamixBeats();
    ~AnalyzerParamixBeats() override;

    AnalyzerPluginInfo info() const override {
        return pluginInfo();
    }

    /// Version of the anchor stage. Grids stored by an older stage are
    /// re-analyzed because their phase could be up to half a beat off.
    static QString gridVersion() {
        return QStringLiteral("2");
    }

    void setBeatDetectionHints(
            const BeatDetectionHints& hints) override;

    bool keptExistingBeatGrid() const override {
        return !m_result.ok && m_result.bpmHintMismatch;
    }

    std::size_t transientCount() const override {
        return m_result.transients.size();
    }

    /// The engine needs the track length, so this plugin is initialized
    /// through the track-aware overload below.
    bool initialize(mixxx::audio::SampleRate sampleRate) override;
    bool initialize(mixxx::audio::SampleRate sampleRate,
            mixxx::audio::ChannelCount channelCount,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* pIn, SINT iLen) override;
    bool finalize() override;

    bool supportsBeatTracking() const override {
        return true;
    }
    bool hasNativeGrid() const override {
        return true;
    }

    mixxx::Bpm getBpm() const override;
    QVector<mixxx::audio::FramePos> getBeats() const override;
    mixxx::audio::FramePos getFirstBeat() const override;
    DownbeatInfo getDownbeatInfo() const override;
    float getConfidence() const override;

  private:
    std::unique_ptr<paramix::analysis::Engine> m_pEngine;
    paramix::analysis::Result m_result;
    AnalyzerBeatsPlugin::BeatDetectionHints m_hints;
    mixxx::audio::SampleRate m_sampleRate;
    int m_channelCount = 2;
    SINT m_totalFrames = 0;
    bool m_finalized = false;
};

} // namespace mixxx
