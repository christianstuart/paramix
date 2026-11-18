#include "effects/backends/builtin/vinylbrakeeffect.h"

#include <algorithm>
#include <cmath>

#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "util/math.h"
#include "util/sample.h"

namespace {
constexpr double kMaxBrakeSeconds = 2.1;
constexpr double kDryWetRampSeconds = 0.008;

inline int wrapFrame(qint64 frame, int ringFrames) {
    int wrapped = static_cast<int>(frame % ringFrames);
    return wrapped < 0 ? wrapped + ringFrames : wrapped;
}
} // namespace

VinylBrakeGroupState::VinylBrakeGroupState(
        const mixxx::EngineParameters& engineParameters)
        : EffectState(engineParameters) {
    audioParametersChanged(engineParameters);
    clear();
}

void VinylBrakeGroupState::audioParametersChanged(
        const mixxx::EngineParameters& engineParameters) {
    channelCount = engineParameters.channelCount();
    ringFrames = static_cast<int>(kMaxBrakeSeconds * engineParameters.sampleRate());
    ring = mixxx::SampleBuffer(static_cast<std::size_t>(ringFrames) * channelCount);
}

void VinylBrakeGroupState::clear() {
    ring.clear();
    writeFrame = 0;
    readFrame = 0.0;
    rate = 1.0;
    phaseFrames = 0.0;
    dryMix = 1.0;
    releasePhase = 0.0;
    releaseStartRate = 1.0;
    active = false;
    releasing = false;
}

// static
QString VinylBrakeEffect::getId() {
    return QStringLiteral("org.paramix.effects.vinylbrake");
}

// static
EffectManifestPointer VinylBrakeEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Vinyl Brake"));
    pManifest->setShortName(QObject::tr("Brake"));
    pManifest->setAuthor("The Paramix Team");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
            "Slows a stem to a stop like a turntable losing power"));
    pManifest->setEffectRampsFromDry(true);
    pManifest->setAddDryToWet(false);
    pManifest->setPadFxOnly(true);

    EffectManifestParameterPointer active = pManifest->addParameter();
    active->setId("active");
    active->setName(QObject::tr("Active"));
    active->setShortName(QObject::tr("Active"));
    active->setDescription(QObject::tr("Engage the brake"));
    active->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
    active->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    active->setRange(0, 0, 1);

    EffectManifestParameterPointer brakeTime = pManifest->addParameter();
    brakeTime->setId("brake_time");
    brakeTime->setName(QObject::tr("Brake Time"));
    brakeTime->setShortName(QObject::tr("Time"));
    brakeTime->setDescription(QObject::tr("Seconds until the stem reaches a stop"));
    brakeTime->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    brakeTime->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    brakeTime->setRange(0.1, 1.0, 2.0);

    EffectManifestParameterPointer windUp = pManifest->addParameter();
    windUp->setId("wind_up_time");
    windUp->setName(QObject::tr("Wind-up Time"));
    windUp->setShortName(QObject::tr("Wind-up"));
    windUp->setDescription(QObject::tr(
            "Seconds to spin the stem back up when the brake is released"));
    windUp->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    windUp->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    windUp->setRange(0.05, 0.3, 1.0);

    EffectManifestParameterPointer curve = pManifest->addParameter();
    curve->setId("curve");
    curve->setName(QObject::tr("Curve"));
    curve->setShortName(QObject::tr("Curve"));
    curve->setDescription(QObject::tr(
            "Shape of the slowdown. Lower values fall linearly, higher values "
            "hold speed for longer and then drop quickly."));
    curve->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    curve->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    curve->setRange(0.0, 0.5, 1.0);

    return pManifest;
}

void VinylBrakeEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pActiveParameter = parameters.value("active");
    m_pBrakeTimeParameter = parameters.value("brake_time");
    m_pCurveParameter = parameters.value("curve");
    m_pWindUpParameter = parameters.value("wind_up_time");
}

void VinylBrakeEffect::processChannel(
        VinylBrakeGroupState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);

    const bool active = enableState != EffectEnableState::Disabled &&
            enableState != EffectEnableState::Disabling &&
            m_pActiveParameter && m_pActiveParameter->toBool();
    const double sampleRate = engineParameters.sampleRate();
    const int channels = std::max(1, static_cast<int>(engineParameters.channelCount()));
    const double brakeSeconds = m_pBrakeTimeParameter
            ? std::clamp(m_pBrakeTimeParameter->value(), 0.1, 2.0)
            : 1.0;
    const double curve = m_pCurveParameter
            ? std::clamp(m_pCurveParameter->value(), 0.0, 1.0)
            : 0.5;
    const double rateStepDenominator = std::max(1.0, brakeSeconds * sampleRate);
    const double dryWetStep = 1.0 / std::max(1.0, kDryWetRampSeconds * sampleRate);

    if (active && !pState->active) {
        // Start from unity speed at the current input position. Because the
        // read pointer starts at the write pointer there is no audible jump.
        pState->readFrame = static_cast<double>(pState->writeFrame);
        pState->rate = 1.0;
        pState->phaseFrames = 0.0;
        pState->releasing = false;
    } else if (!active && pState->active && !pState->releasing) {
        // Releasing a brake spins the stem back up from the stopped position
        // before the effect crossfades to dry. This is audible when the pad
        // layer clears the native stem mute before dropping the gate.
        pState->releasing = true;
        pState->releasePhase = 0.0;
        pState->releaseStartRate = pState->rate;
    }
    pState->active = active;

    const bool wetActive = active || pState->releasing;
    const double targetDryMix = wetActive ? 0.0 : 1.0;
    const int frames = engineParameters.framesPerBuffer();

    for (int frame = 0; frame < frames; ++frame) {
        const int inputOffset = frame * channels;

        // Always keep the ring fed. This costs one copy per frame and avoids
        // stale audio if a brake is re-triggered.
        const int writeIndex = wrapFrame(pState->writeFrame, pState->ringFrames) * channels;
        for (int ch = 0; ch < channels; ++ch) {
            pState->ring[writeIndex + ch] = pInput[inputOffset + ch];
        }

        if (pState->releasing) {
            const double windUpSeconds = m_pWindUpParameter
                    ? std::clamp(m_pWindUpParameter->value(), 0.05, 1.0)
                    : 0.3;
            pState->releasePhase = std::min(1.0,
                    pState->releasePhase + 1.0 / (windUpSeconds * sampleRate));
            const double t = pState->releasePhase;
            pState->rate = pState->releaseStartRate +
                    (1.0 - pState->releaseStartRate) * t;
            pState->readFrame += pState->rate;
            if (t >= 1.0) {
                pState->releasing = false;
            }
        } else if (active) {
            const double phase = std::min(1.0,
                    pState->phaseFrames / rateStepDenominator);
            pState->rate = std::pow(std::max(0.0, 1.0 - phase), 1.0 + curve * 2.0);
            pState->phaseFrames += 1.0;
            pState->readFrame += pState->rate;

            const double maxRead = static_cast<double>(pState->writeFrame);
            const double minRead = maxRead - pState->ringFrames + 2.0;
            if (pState->readFrame > maxRead) {
                pState->readFrame = maxRead;
            } else if (pState->readFrame < minRead) {
                pState->readFrame = minRead;
            }
        } else {
            pState->readFrame = static_cast<double>(pState->writeFrame);
            pState->rate = 1.0;
        }

        if (pState->dryMix < targetDryMix) {
            pState->dryMix = std::min(targetDryMix, pState->dryMix + dryWetStep);
        } else if (pState->dryMix > targetDryMix) {
            pState->dryMix = std::max(targetDryMix, pState->dryMix - dryWetStep);
        }

        // Linear interpolation between the two nearest ring frames.
        const double readFloor = std::floor(pState->readFrame);
        const double fraction = pState->readFrame - readFloor;
        const int frameA = wrapFrame(static_cast<qint64>(readFloor), pState->ringFrames);
        const int frameB = wrapFrame(static_cast<qint64>(readFloor) + 1, pState->ringFrames);
        const int indexA = frameA * channels;
        const int indexB = frameB * channels;

        // Near-zero speed would hold a DC sample; fade the wet path out as the
        // rate approaches zero so the stop is silent rather than a click.
        const double wetGain = wetActive
                ? std::clamp(pState->rate / 0.08, 0.0, 1.0)
                : 0.0;

        for (int ch = 0; ch < channels; ++ch) {
            const CSAMPLE wet = static_cast<CSAMPLE>(
                    (pState->ring[indexA + ch] * (1.0 - fraction) +
                            pState->ring[indexB + ch] * fraction) *
                    wetGain);
            const CSAMPLE dry = pInput[inputOffset + ch];
            pOutput[inputOffset + ch] = static_cast<CSAMPLE>(
                    dry * pState->dryMix + wet * (1.0 - pState->dryMix));
        }

        ++pState->writeFrame;
    }
}
