#pragma once

#include "effects/backends/effectprocessor.h"
#include "engine/engine.h"
#include "util/class.h"
#include "util/samplebuffer.h"

/// Vinyl Brake is an audio-domain tape stop for one stem.
///
/// The effect keeps its own short ring buffer and reads it at a rate that ramps
/// from 1.0 to 0.0. The deck transport and beatgrid are untouched, so the other
/// stems keep playing normally. When the pad is released the effect crossfades
/// back to the live input.
class VinylBrakeGroupState : public EffectState {
  public:
    explicit VinylBrakeGroupState(const mixxx::EngineParameters& engineParameters);
    ~VinylBrakeGroupState() override = default;

    void audioParametersChanged(const mixxx::EngineParameters& engineParameters);
    void clear();

    mixxx::SampleBuffer ring;
    int ringFrames;
    int channelCount;
    qint64 writeFrame;
    double readFrame;
    double rate;
    double phaseFrames;
    double dryMix;
    double releasePhase;
    double releaseStartRate;
    bool active;
    bool releasing;
};

class VinylBrakeEffect : public EffectProcessorImpl<VinylBrakeGroupState> {
  public:
    VinylBrakeEffect() = default;
    ~VinylBrakeEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            VinylBrakeGroupState* pState,
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
    EngineEffectParameterPointer m_pBrakeTimeParameter;
    EngineEffectParameterPointer m_pCurveParameter;
    EngineEffectParameterPointer m_pWindUpParameter;

    DISALLOW_COPY_AND_ASSIGN(VinylBrakeEffect);
};
