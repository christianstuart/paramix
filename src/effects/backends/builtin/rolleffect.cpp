#include "effects/backends/builtin/rolleffect.h"

#include <algorithm>
#include <cmath>

#include "effects/backends/effectmanifest.h"
#include "engine/effects/engineeffectparameter.h"
#include "util/rampingvalue.h"
#include "util/sample.h"

namespace {

// One beat if no tempo is known, like Echo.
constexpr double kDefaultBeatSeconds = 0.5;

// Seam crossfade length.
constexpr double kSeamCrossfadeSeconds = 0.005;

} // anonymous namespace

// static
QString RollEffect::getId() {
    return QStringLiteral("org.paramix.effects.roll");
}

// static
EffectManifestPointer RollEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());

    pManifest->setAddDryToWet(false);
    pManifest->setEffectRampsFromDry(false);

    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Roll"));
    pManifest->setShortName(QObject::tr("Roll"));
    pManifest->setAuthor("Paramix");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
            "Captures the most recent beats and loops that slice until the effect is switched off."));

    EffectManifestParameterPointer beats = pManifest->addParameter();
    beats->setId("beats");
    beats->setName(QObject::tr("Time"));
    beats->setShortName(QObject::tr("Time"));
    beats->setDescription(QObject::tr(
            "Length of the looped slice in beats\n"
            "1/2 second per beat if no tempo is detected"));
    beats->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    beats->setUnitsHint(EffectManifestParameter::UnitsHint::Beats);
    beats->setRange(0.0625, 1.0, 4.0);

    EffectManifestParameterPointer level = pManifest->addParameter();
    level->setId("level");
    level->setName(QObject::tr("Level"));
    level->setShortName(QObject::tr("Level"));
    level->setDescription(QObject::tr(
            "Crossfade between the original and the roll\n"
            "Fully left: only the original\n"
            "Fully right: only the roll"));
    level->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    level->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    level->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    level->setRange(0.0, 1.0, 1.0);

    pManifest->setBeatFx(QStringLiteral("beats"),
            QStringLiteral("level"),
            QList<double>{1 / 16.0, 1 / 8.0, 1 / 4.0, 1 / 2.0, 1.0, 2.0, 4.0});

    return pManifest;
}

void RollEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pBeatsParameter = parameters.value("beats");
    m_pLevelParameter = parameters.value("level");
}

void RollEffect::processChannel(
        RollGroupState* pState,
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
    const auto requestedFrames = static_cast<int>(beats * beatSeconds * sampleRate);
    const int sliceFrames = std::clamp(requestedFrames, 1, pState->ringFrames);

    const auto levelCurrent = static_cast<CSAMPLE_GAIN>(m_pLevelParameter->value());
    RampingValue<CSAMPLE_GAIN> level(pState->prev_level, levelCurrent, frames);

    // The loop end stays anchored where the capture stopped, so a shorter
    // value plays the last part of the captured slice.
    int loopLen = pState->rolling ? std::min(sliceFrames, pState->loopEnd) : 0;
    int loopStart = pState->loopEnd - loopLen;
    int crossfadeFrames = 0;
    if (loopLen > 0) {
        crossfadeFrames = std::min(static_cast<int>(kSeamCrossfadeSeconds * sampleRate),
                loopStart);
        crossfadeFrames = std::min(crossfadeFrames, loopLen / 2);
        pState->loopPhase %= loopLen;
    }

    for (int frame = 0; frame < frames; ++frame) {
        const int inputIndex = frame * channels;
        const auto levelRamped = level.getNth(frame);

        if (!pState->rolling) {
            for (int ch = 0; ch < channels; ++ch) {
                pState->ring[pState->writeFrame * channels + ch] = pInput[inputIndex + ch];
            }
            ++pState->writeFrame;
            if (pState->writeFrame >= sliceFrames) {
                // A full slice has been captured. Its end is fixed from here
                // on so the slice can be shortened without moving the end.
                pState->rolling = true;
                pState->loopEnd = pState->writeFrame;
                pState->loopPhase = 0;
            } else {
                // Nothing to loop yet, so play the input unchanged.
                for (int ch = 0; ch < channels; ++ch) {
                    pOutput[inputIndex + ch] = pInput[inputIndex + ch];
                }
                continue;
            }
            // Recomputed here because the capture ended mid buffer.
            loopLen = pState->loopEnd;
            loopStart = 0;
            crossfadeFrames = 0;
        }

        const int seamStart = loopLen - crossfadeFrames;
        for (int ch = 0; ch < channels; ++ch) {
            const CSAMPLE dry = pInput[inputIndex + ch];
            CSAMPLE rolled = pState->ring[(loopStart + pState->loopPhase) * channels + ch];
            if (crossfadeFrames > 0 && pState->loopPhase >= seamStart) {
                // Fade the end of the slice into the audio just before its
                // start to make the seam continuous.
                const int k = pState->loopPhase - seamStart;
                const auto f = static_cast<CSAMPLE>(k + 1) / crossfadeFrames;
                const CSAMPLE preRoll =
                        pState->ring[(loopStart - crossfadeFrames + k) * channels + ch];
                rolled = rolled * (1 - f) + preRoll * f;
            }
            pOutput[inputIndex + ch] = dry * (1 - levelRamped) + rolled * levelRamped;
        }
        ++pState->loopPhase;
    }

    if (enableState == EffectEnableState::Disabling) {
        // The engine crossfades to dry over this buffer. The next run starts
        // with a fresh capture.
        pState->rolling = false;
        pState->writeFrame = 0;
        pState->loopEnd = 0;
        pState->loopPhase = 0;
        pState->prev_level = 0.0f;
    } else {
        pState->prev_level = levelCurrent;
    }
}
