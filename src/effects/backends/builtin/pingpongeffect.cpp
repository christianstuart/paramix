#include "effects/backends/builtin/pingpongeffect.h"

#include <algorithm>
#include <cmath>

#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {

inline int wrapFrame(int frame, int ringFrames) {
    int wrapped = frame % ringFrames;
    return wrapped < 0 ? wrapped + ringFrames : wrapped;
}

// One beat if no tempo is known, like Echo.
constexpr double kDefaultBeatSeconds = 0.5;

} // anonymous namespace

// static
QString PingPongEffect::getId() {
    return QStringLiteral("org.paramix.effects.pingpong");
}

// static
EffectManifestPointer PingPongEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());

    pManifest->setAddDryToWet(true);
    pManifest->setEffectRampsFromDry(true);

    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Ping Pong"));
    pManifest->setShortName(QObject::tr("Ping Pong"));
    pManifest->setAuthor("Paramix");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
            "Beat-synced delay whose repeats alternate between the left and right sides."));

    EffectManifestParameterPointer beats = pManifest->addParameter();
    beats->setId("beats");
    beats->setName(QObject::tr("Time"));
    beats->setShortName(QObject::tr("Time"));
    beats->setDescription(QObject::tr(
            "Delay time in beats\n"
            "1/2 second per beat if no tempo is detected"));
    beats->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    beats->setUnitsHint(EffectManifestParameter::UnitsHint::Beats);
    beats->setRange(0.125, 0.5, 4.0);

    EffectManifestParameterPointer level = pManifest->addParameter();
    level->setId("level");
    level->setName(QObject::tr("Level"));
    level->setShortName(QObject::tr("Level"));
    level->setDescription(QObject::tr("Volume of the repeats added to the dry signal"));
    level->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    level->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    level->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    level->setRange(0.0, 0.5, 1.0);

    EffectManifestParameterPointer feedback = pManifest->addParameter();
    feedback->setId("feedback");
    feedback->setName(QObject::tr("Feedback"));
    feedback->setShortName(QObject::tr("Feedback"));
    feedback->setDescription(QObject::tr("Amount the repeats fade each time they loop"));
    feedback->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    feedback->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    feedback->setRange(0.0, 0.5, 0.9);

    EffectManifestParameterPointer width = pManifest->addParameter();
    width->setId("width");
    width->setName(QObject::tr("Width"));
    width->setShortName(QObject::tr("Width"));
    width->setDescription(QObject::tr(
            "How much the repeats alternate between the left and right sides\n"
            "Fully left: centered, like a plain delay\n"
            "Fully right: full alternation"));
    width->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    width->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    width->setRange(0.0, 1.0, 1.0);

    pManifest->setBeatFx(QStringLiteral("beats"),
            QStringLiteral("level"),
            QList<double>{1 / 8.0, 1 / 4.0, 1 / 2.0, 3 / 4.0, 1.0, 2.0, 4.0});

    return pManifest;
}

void PingPongEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeatsParameter = parameters.value("beats");
    m_pLevelParameter = parameters.value("level");
    m_pFeedbackParameter = parameters.value("feedback");
    m_pWidthParameter = parameters.value("width");
}

void PingPongEffect::processChannel(
        PingPongGroupState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const int channels = std::max(1, static_cast<int>(engineParameters.channelCount()));
    const int frames = engineParameters.framesPerBuffer();
    const double sampleRate = engineParameters.sampleRate();

    const double beatSeconds = groupFeatures.beat_length.has_value()
            ? groupFeatures.beat_length->seconds
            : kDefaultBeatSeconds;
    const double beats = std::max(0.125, m_pBeatsParameter->value());
    const double maxDelayFrames = static_cast<double>(pState->ringFrames - 2);
    const double targetDelayFrames = std::clamp(beats * beatSeconds * sampleRate,
            2.0,
            maxDelayFrames);

    const auto levelCurrent = static_cast<CSAMPLE_GAIN>(m_pLevelParameter->value());
    const auto feedbackCurrent = static_cast<CSAMPLE_GAIN>(m_pFeedbackParameter->value());
    const auto widthCurrent = static_cast<CSAMPLE_GAIN>(m_pWidthParameter->value());

    if (enableState == EffectEnableState::Enabling || pState->delayFrames < 2.0) {
        pState->delayFrames = targetDelayFrames;
    }

    RampingValue<CSAMPLE_GAIN> level(pState->prev_level, levelCurrent, frames);
    RampingValue<CSAMPLE_GAIN> feedback(pState->prev_feedback, feedbackCurrent, frames);
    RampingValue<CSAMPLE_GAIN> width(pState->prev_width, widthCurrent, frames);

    for (int frame = 0; frame < frames; ++frame) {
        pState->delayFrames += (targetDelayFrames - pState->delayFrames) * 0.001;

        const int delayFloor = static_cast<int>(std::floor(pState->delayFrames));
        const auto frac = static_cast<CSAMPLE>(pState->delayFrames - delayFloor);
        const int readA = wrapFrame(pState->writeIndex - delayFloor, pState->ringFrames);
        const int readB = wrapFrame(readA - 1, pState->ringFrames);

        const auto levelRamped = level.getNth(frame);
        const auto feedbackRamped = feedback.getNth(frame);
        const auto widthRamped = std::clamp(width.getNth(frame), 0.0f, 1.0f);

        const int inputIndex = frame * channels;
        const int writeIndex = pState->writeIndex * channels;

        CSAMPLE mono = 0.0f;
        for (int ch = 0; ch < channels; ++ch) {
            mono += pInput[inputIndex + ch];
        }
        mono /= channels;

        for (int ch = 0; ch < channels; ++ch) {
            const int other = (ch + 1) % channels;
            const CSAMPLE delayed = pState->delay_buf[readA * channels + ch] * (1 - frac) +
                    pState->delay_buf[readB * channels + ch] * frac;
            const CSAMPLE delayedOther = pState->delay_buf[readA * channels + other] * (1 - frac) +
                    pState->delay_buf[readB * channels + other] * frac;
            const CSAMPLE dry = pInput[inputIndex + ch];

            // At zero width the input and the feedback stay in their own
            // channel. At full width the input is centered and the feedback
            // crosses over, so each repeat changes sides.
            const CSAMPLE sent = dry * (1 - widthRamped) + mono * widthRamped;
            const CSAMPLE fedBack = delayed * (1 - widthRamped) + delayedOther * widthRamped;

            pOutput[inputIndex + ch] = delayed * levelRamped;
            pState->delay_buf[writeIndex + ch] = SampleUtil::clampSample(
                    sent + fedBack * feedbackRamped);
        }

        if (++pState->writeIndex >= pState->ringFrames) {
            pState->writeIndex = 0;
        }
    }

    if (enableState == EffectEnableState::Disabling) {
        SampleUtil::applyRampingGain(pOutput, 1.0, 0.0, engineParameters.samplesPerBuffer());
        pState->delay_buf.clear();
        pState->prev_level = 0.0f;
    } else {
        pState->prev_level = levelCurrent;
    }
    pState->prev_feedback = feedbackCurrent;
    pState->prev_width = widthCurrent;
}
