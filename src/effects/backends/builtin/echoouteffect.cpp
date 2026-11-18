#include "effects/backends/builtin/echoouteffect.h"

#include <algorithm>
#include <cmath>

#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "util/math.h"
#include "util/sample.h"

namespace {
constexpr double kMaxDelaySeconds = 2.1;
constexpr double kDryWetRampSeconds = 0.0005;

inline int wrapFrame(qint64 frame, int ringFrames) {
    int wrapped = static_cast<int>(frame % ringFrames);
    return wrapped < 0 ? wrapped + ringFrames : wrapped;
}
} // namespace

EchoOutGroupState::EchoOutGroupState(
        const mixxx::EngineParameters& engineParameters)
        : EffectState(engineParameters) {
    audioParametersChanged(engineParameters);
    clear();
}

void EchoOutGroupState::audioParametersChanged(
        const mixxx::EngineParameters& engineParameters) {
    channelCount = static_cast<int>(engineParameters.channelCount());
    ringFrames = static_cast<int>(kMaxDelaySeconds * engineParameters.sampleRate());
    ring = mixxx::SampleBuffer(static_cast<std::size_t>(ringFrames) * channelCount);
}

void EchoOutGroupState::clear() {
    ring.clear();
    writeFrame = 0;
    delayFrames = 0.0;
    dryMix = 1.0;
    active = false;
}

// static
QString EchoOutEffect::getId() {
    return QStringLiteral("org.paramix.effects.echoout");
}

// static
EffectManifestPointer EchoOutEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Echo Out"));
    pManifest->setShortName(QObject::tr("Echo Out"));
    pManifest->setAuthor("The Paramix Team");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
            "Cuts the dry signal and repeats a stem into silence"));
    pManifest->setEffectRampsFromDry(true);
    pManifest->setAddDryToWet(false);
    pManifest->setPadFxOnly(true);

    EffectManifestParameterPointer active = pManifest->addParameter();
    active->setId("active");
    active->setName(QObject::tr("Active"));
    active->setShortName(QObject::tr("Active"));
    active->setDescription(QObject::tr("Send the stem into the echo"));
    active->setValueScaler(EffectManifestParameter::ValueScaler::Toggle);
    active->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    active->setRange(0, 0, 1);

    EffectManifestParameterPointer beats = pManifest->addParameter();
    beats->setId("beats");
    beats->setName(QObject::tr("Time"));
    beats->setShortName(QObject::tr("Time"));
    beats->setDescription(QObject::tr(
            "Delay length in beats when a beatgrid is available"));
    beats->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    beats->setUnitsHint(EffectManifestParameter::UnitsHint::Beats);
    beats->setRange(0.125, 1.0, 2.0);

    EffectManifestParameterPointer feedback = pManifest->addParameter();
    feedback->setId("feedback");
    feedback->setName(QObject::tr("Feedback"));
    feedback->setShortName(QObject::tr("Feedback"));
    feedback->setDescription(QObject::tr("How much each repeat fades"));
    feedback->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    feedback->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    feedback->setRange(0.0, 0.7, 0.95);

    return pManifest;
}

void EchoOutEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pActiveParameter = parameters.value("active");
    m_pBeatsParameter = parameters.value("beats");
    m_pFeedbackParameter = parameters.value("feedback");
}

void EchoOutEffect::processChannel(
        EchoOutGroupState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const bool active = enableState != EffectEnableState::Disabled &&
            enableState != EffectEnableState::Disabling &&
            m_pActiveParameter && m_pActiveParameter->toBool();

    const double sampleRate = engineParameters.sampleRate();
    const int channels = std::max(1, static_cast<int>(engineParameters.channelCount()));
    const int frames = engineParameters.framesPerBuffer();

    double delaySeconds;
    if (groupFeatures.beat_length.has_value()) {
        delaySeconds = (m_pBeatsParameter ? m_pBeatsParameter->value() : 1.0) *
                groupFeatures.beat_length->seconds;
    } else {
        delaySeconds = (m_pBeatsParameter ? m_pBeatsParameter->value() : 1.0) * 0.5;
    }
    const double maxDelayFrames = static_cast<double>(pState->ringFrames - 2);
    const double targetDelayFrames = std::clamp(delaySeconds * sampleRate,
            2.0,
            maxDelayFrames);

    const double feedback = m_pFeedbackParameter
            ? std::clamp(m_pFeedbackParameter->value(), 0.0, 0.95)
            : 0.7;
    const double dryWetStep = 1.0 / std::max(1.0, kDryWetRampSeconds * sampleRate);
    const double targetDryMix = active ? 0.0 : 1.0;

    if (active && !pState->active) {
        // The delay buffer is continuously pre-filled while the pad is idle.
        // Start reading at the intended delay immediately instead of sweeping
        // the read pointer from zero, which would sound like a pitch slide.
        pState->delayFrames = targetDelayFrames;
    }
    pState->active = active;

    for (int frame = 0; frame < frames; ++frame) {
        // Smooth delay changes so changing the beat value while active does not
        // produce a click.
        pState->delayFrames += (targetDelayFrames - pState->delayFrames) * 0.001;
        const double delayFrames = std::max(1.0, pState->delayFrames);
        const bool hasHistory = pState->writeFrame >=
                static_cast<qint64>(delayFrames) + 2;
        const double readPosition = static_cast<double>(pState->writeFrame) -
                delayFrames;
        const double readFloor = std::floor(readPosition);
        const double fraction = readPosition - readFloor;
        const int frameA = wrapFrame(static_cast<qint64>(readFloor), pState->ringFrames);
        const int frameB = wrapFrame(static_cast<qint64>(readFloor) + 1, pState->ringFrames);
        const int indexA = frameA * channels;
        const int indexB = frameB * channels;
        const int inputOffset = frame * channels;
        const int writeIndex = wrapFrame(pState->writeFrame, pState->ringFrames) * channels;

        if (pState->dryMix < targetDryMix) {
            pState->dryMix = std::min(targetDryMix, pState->dryMix + dryWetStep);
        } else if (pState->dryMix > targetDryMix) {
            pState->dryMix = std::max(targetDryMix, pState->dryMix - dryWetStep);
        }

        for (int ch = 0; ch < channels; ++ch) {
            const CSAMPLE delayed = hasHistory
                    ? static_cast<CSAMPLE>(
                              pState->ring[indexA + ch] * (1.0 - fraction) +
                              pState->ring[indexB + ch] * fraction)
                    : 0.0f;
            const CSAMPLE dry = pInput[inputOffset + ch];
            // Pre-roll with a dry signal while inactive so the first repeat is
            // immediately available when the pad engages.
            pState->ring[writeIndex + ch] = active
                    ? static_cast<CSAMPLE>(dry + delayed * feedback)
                    : dry;
            pOutput[inputOffset + ch] = static_cast<CSAMPLE>(
                    dry * pState->dryMix + delayed * (1.0 - pState->dryMix));
        }

        ++pState->writeFrame;
    }
}
