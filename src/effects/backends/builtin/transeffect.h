#pragma once

#include <QMap>

#include "effects/backends/effectprocessor.h"
#include "engine/engine.h"
#include "util/class.h"

class TransGroupState : public EffectState {
  public:
    TransGroupState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        Q_UNUSED(engineParameters);
        clear();
    }
    ~TransGroupState() override = default;

    void clear() {
        phaseFrame = 0;
        gain = 0.0f;
    };

    int phaseFrame;
    CSAMPLE_GAIN gain;
};

class TransEffect : public EffectProcessorImpl<TransGroupState> {
  public:
    TransEffect() = default;
    ~TransEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            TransGroupState* pState,
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
    EngineEffectParameterPointer m_pDepthParameter;
    EngineEffectParameterPointer m_pDutyParameter;

    DISALLOW_COPY_AND_ASSIGN(TransEffect);
};
