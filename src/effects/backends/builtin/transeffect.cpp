#include "effects/backends/builtin/transeffect.h"

#include <algorithm>
#include <cmath>

#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "util/sample.h"

namespace {

// One beat if no tempo is known, like Echo.
constexpr double kDefaultBeatSeconds = 0.5;

// Time the gate takes to reach the other state.
constexpr double kEdgeSmoothingSeconds = 0.002;

} // anonymous namespace

// static
QString TransEffect::getId() {
    return QStringLiteral("org.paramix.effects.trans");
}

// static
EffectManifestPointer TransEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());

    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Trans"));
    pManifest->setShortName(QObject::tr("Trans"));
    pManifest->setAuthor("Paramix");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
            "Beat-synced gate that chops the signal."));

    EffectManifestParameterPointer beats = pManifest->addParameter();
    beats->setId("beats");
    beats->setName(QObject::tr("Time"));
    beats->setShortName(QObject::tr("Time"));
    beats->setDescription(QObject::tr(
            "Length of one gate cycle in beats\n"
            "1/2 second per beat if no tempo is detected"));
    beats->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    beats->setUnitsHint(EffectManifestParameter::UnitsHint::Beats);
    beats->setRange(0.0625, 0.5, 4.0);

    EffectManifestParameterPointer depth = pManifest->addParameter();
    depth->setId("depth");
    depth->setName(QObject::tr("Depth"));
    depth->setShortName(QObject::tr("Depth"));
    depth->setDescription(QObject::tr(
            "How much the gate closes\n"
            "Fully left: no effect\n"
            "Fully right: silent while closed"));
    depth->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    depth->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    depth->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    depth->setRange(0.0, 1.0, 1.0);

    EffectManifestParameterPointer duty = pManifest->addParameter();
    duty->setId("duty");
    duty->setName(QObject::tr("Duty"));
    duty->setShortName(QObject::tr("Duty"));
    duty->setDescription(QObject::tr("Fraction of each cycle the gate is open"));
    duty->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    duty->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    duty->setRange(0.1, 0.5, 0.9);

    pManifest->setBeatFx(QStringLiteral("beats"),
            QStringLiteral("depth"),
            QList<double>{1 / 16.0, 1 / 8.0, 1 / 4.0, 1 / 2.0, 1.0, 2.0, 4.0});

    return pManifest;
}

void TransEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeatsParameter = parameters.value("beats");
    m_pDepthParameter = parameters.value("depth");
    m_pDutyParameter = parameters.value("duty");
}

void TransEffect::processChannel(
        TransGroupState* pState,
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
    const double beats = std::max(0.0625, m_pBeatsParameter->value());
    const int periodFrames = std::max(1,
            static_cast<int>(beats * beatSeconds * sampleRate));
    const double depth = std::clamp(m_pDepthParameter->value(), 0.0, 1.0);
    const double duty = std::clamp(m_pDutyParameter->value(), 0.1, 0.9);
    const int openFrames = std::clamp(static_cast<int>(duty * periodFrames),
            1,
            std::max(1, periodFrames - 1));
    const auto smoothing = static_cast<CSAMPLE_GAIN>(
            1.0 - std::exp(-1.0 / (kEdgeSmoothingSeconds * sampleRate)));

    // Follow the beat grid when a period fits in one beat, so the gate stays
    // aligned. Longer periods free-run from the phase taken on enabling.
    const bool hasBeat = groupFeatures.beat_length.has_value() &&
            groupFeatures.beat_fraction_buffer_end.has_value();
    const int beatFrames = hasBeat
            ? static_cast<int>(groupFeatures.beat_length->seconds * sampleRate)
            : 0;
    if (hasBeat &&
            (enableState == EffectEnableState::Enabling || periodFrames <= beatFrames)) {
        // beat_fraction_buffer_end is the position at the end of the buffer.
        const double beatPosition = *groupFeatures.beat_fraction_buffer_end * beatFrames;
        const int phaseAtEnd = static_cast<int>(std::fmod(beatPosition, periodFrames));
        pState->phaseFrame = (phaseAtEnd - frames) % periodFrames;
        if (pState->phaseFrame < 0) {
            pState->phaseFrame += periodFrames;
        }
    } else {
        pState->phaseFrame %= periodFrames;
    }

    if (enableState == EffectEnableState::Enabling) {
        pState->gain = 0.0f;
    }

    for (int frame = 0; frame < frames; ++frame) {
        const CSAMPLE_GAIN target = pState->phaseFrame < openFrames
                ? 1.0f
                : static_cast<CSAMPLE_GAIN>(1.0 - depth);
        pState->gain += (target - pState->gain) * smoothing;
        // Snap so a fully closed gate does not decay into denormals.
        if (std::fabs(pState->gain - target) < 1e-6f) {
            pState->gain = target;
        }

        const int inputIndex = frame * channels;
        for (int ch = 0; ch < channels; ++ch) {
            pOutput[inputIndex + ch] = pInput[inputIndex + ch] * pState->gain;
        }

        if (++pState->phaseFrame >= periodFrames) {
            pState->phaseFrame = 0;
        }
    }
}
