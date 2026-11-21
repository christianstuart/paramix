#pragma once

#include <QMap>

#include "effects/backends/effectprocessor.h"
#include "engine/engine.h"
#include "util/class.h"
#include "util/samplebuffer.h"

class BeatDelayGroupState : public EffectState {
  public:
    // 4 beats at 40 BPM is 6 seconds.
    static constexpr int kMaxDelaySeconds = 6;

    BeatDelayGroupState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
        clear();
    }
    ~BeatDelayGroupState() override = default;

    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        channelCount = engineParameters.channelCount();
        ringFrames = static_cast<int>(kMaxDelaySeconds * engineParameters.sampleRate());
        if (ringFrames < 4) {
            ringFrames = 4;
        }
        delay_buf = mixxx::SampleBuffer(ringFrames * channelCount);
    };

    void clear() {
        delay_buf.clear();
        writeIndex = 0;
        delayFrames = 0.0;
        prev_level = 0.0f;
        prev_feedback = 0.0f;
    };

    mixxx::SampleBuffer delay_buf;
    int channelCount;
    int ringFrames;
    int writeIndex;
    double delayFrames;
    CSAMPLE_GAIN prev_level;
    CSAMPLE_GAIN prev_feedback;
};

class BeatDelayEffect : public EffectProcessorImpl<BeatDelayGroupState> {
  public:
    BeatDelayEffect() = default;
    ~BeatDelayEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            BeatDelayGroupState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameterPointer m_pBeatsParameter;
    EngineEffectParameterPointer m_pLevelParameter;
    EngineEffectParameterPointer m_pFeedbackParameter;

    DISALLOW_COPY_AND_ASSIGN(BeatDelayEffect);
};
