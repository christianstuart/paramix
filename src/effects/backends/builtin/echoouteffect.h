#pragma once

#include "effects/backends/effectprocessor.h"
#include "engine/engine.h"
#include "util/class.h"
#include "util/samplebuffer.h"

/// Echo Out is a pad-only echo that removes the dry signal while active.
///
/// Unlike the stock Echo effect this processor keeps feeding its delay line
/// while inactive, so the first repeat is already in the buffer when the pad is
/// pressed. The pad control layer gates the wet output with the `active`
/// parameter. A pad can therefore leave the stem silent after the tail decays.
class EchoOutGroupState : public EffectState {
  public:
    explicit EchoOutGroupState(const mixxx::EngineParameters& engineParameters);
    ~EchoOutGroupState() override = default;

    void audioParametersChanged(const mixxx::EngineParameters& engineParameters);
    void clear();

    mixxx::SampleBuffer ring;
    int ringFrames;
    int channelCount;
    qint64 writeFrame;
    double delayFrames;
    double dryMix;
    bool active;
};

class EchoOutEffect : public EffectProcessorImpl<EchoOutGroupState> {
  public:
    EchoOutEffect() = default;
    ~EchoOutEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            EchoOutGroupState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameterPointer m_pActiveParameter;
    EngineEffectParameterPointer m_pBeatsParameter;
    EngineEffectParameterPointer m_pFeedbackParameter;

    DISALLOW_COPY_AND_ASSIGN(EchoOutEffect);
};
