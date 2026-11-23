#pragma once

#include <QString>
#include <cstddef>

#include "audio/frame.h"
#include "track/beats.h"
#include "track/bpm.h"
#include "track/keys.h"
#include "util/types.h"

namespace mixxx {

class AnalyzerPluginInfo {
  public:
    AnalyzerPluginInfo(const QString& id,
            const QString& author,
            const QString& name,
            bool isConstantTempoSupported)
            : m_id(id),
              m_author(author),
              m_name(name),
              m_isConstantTempoSupported(isConstantTempoSupported) {
    }

    const QString& id() const {
        return m_id;
    }

    const QString& author() const {
        return m_author;
    }

    const QString& name() const {
        return m_name;
    }

    bool isConstantTempoSupported() const {
        return m_isConstantTempoSupported;
    }

  private:
    QString m_id;
    QString m_author;
    QString m_name;
    bool m_isConstantTempoSupported;
};

class AnalyzerPlugin {
  public:
    virtual ~AnalyzerPlugin() = default;

    virtual QString id() const {
        return info().id();
    }
    virtual QString author() const {
        return info().author();
    }
    virtual QString name() const {
        return info().name();
    }
    virtual AnalyzerPluginInfo info() const = 0;

    virtual bool initialize(mixxx::audio::SampleRate sampleRate) = 0;
    virtual bool processSamples(const CSAMPLE* pIn, SINT iLen) = 0;
    virtual bool finalize() = 0;
};

class AnalyzerBeatsPlugin : public AnalyzerPlugin {
  public:
    ~AnalyzerBeatsPlugin() override = default;

    /// Track-specific detection hints. Only plugins that care need to read
    /// them; the defaults preserve the existing behavior.
    struct BeatDetectionHints {
        int bpmRangeMin = 88;
        int bpmRangeMax = 175;
        double existingBpm = 0.0;
        bool hasGrid = false;
        bool matchExistingBpm = true;
    };

    // Keep the sample-rate-only overload visible; the track-aware overload
    // below would otherwise hide it inside this class.
    using AnalyzerPlugin::initialize;

    /// Called by AnalyzerBeats before initialize() when the track is known.
    virtual void setBeatDetectionHints(const BeatDetectionHints& hints) {
        (void)hints;
    }

    /// True when the last run deliberately kept the track's existing beat grid
    /// instead of producing a new one, so a failed finalize() is not an error.
    virtual bool keptExistingBeatGrid() const {
        return false;
    }

    /// Number of transients the last run detected, for diagnostics only.
    virtual std::size_t transientCount() const {
        return 0;
    }

    /// Track-aware initialization. Plugins that need the total frame count or
    /// the channel count override this; the default keeps the existing
    /// sample-rate-only contract for all other plugins.
    virtual bool initialize(mixxx::audio::SampleRate sampleRate,
            mixxx::audio::ChannelCount channelCount,
            SINT frameLength) {
        (void)channelCount;
        (void)frameLength;
        return initialize(sampleRate);
    }

    virtual bool supportsBeatTracking() const = 0;
    virtual mixxx::Bpm getBpm() const {
        return {};
    }
    virtual QVector<mixxx::audio::FramePos> getBeats() const {
        return {};
    }

    /// Native-grid plugins provide the grid origin and downbeat metadata
    /// directly and must bypass BeatFactory's re-derivation and BPM snapping.
    virtual bool hasNativeGrid() const {
        return false;
    }
    virtual mixxx::audio::FramePos getFirstBeat() const {
        return mixxx::audio::kStartFramePos;
    }
    virtual DownbeatInfo getDownbeatInfo() const {
        return {};
    }
    virtual float getConfidence() const {
        return 0.0f;
    }
};

class AnalyzerKeyPlugin : public AnalyzerPlugin {
  public:
    ~AnalyzerKeyPlugin() override = default;

    virtual KeyChangeList getKeyChanges() const = 0;
};

} // namespace mixxx
