#pragma once

#include <QMap>

#include "effects/backends/effectprocessor.h"
#include "engine/engine.h"
#include "util/class.h"
#include "util/samplebuffer.h"

class RollGroupState : public EffectState {
  public:
    // 4 beats at 40 BPM is 6 seconds.
    static constexpr int kMaxRollSeconds = 6;

    RollGroupState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
        clear();
    }
    ~RollGroupState() override = default;

    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        channelCount = engineParameters.channelCount();
        ringFrames = static_cast<int>(kMaxRollSeconds * engineParameters.sampleRate());
        if (ringFrames < 4) {
            ringFrames = 4;
        }
        ring = mixxx::SampleBuffer(ringFrames * channelCount);
    };

    void clear() {
        ring.clear();
        writeFrame = 0;
        loopEnd = 0;
        loopPhase = 0;
        rolling = false;
        prev_level = 0.0f;
    };

    mixxx::SampleBuffer ring;
    int channelCount;
    int ringFrames;
    // Frames captured since enabling and the frame where the capture stopped.
    int writeFrame;
    int loopEnd;
    int loopPhase;
    bool rolling;
    CSAMPLE_GAIN prev_level;
};

class RollEffect : public EffectProcessorImpl<RollGroupState> {
  public:
    RollEffect() = default;
    ~RollEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            RollGroupState* pState,
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

    DISALLOW_COPY_AND_ASSIGN(RollEffect);
};
