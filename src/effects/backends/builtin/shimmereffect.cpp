#include "effects/backends/builtin/shimmereffect.h"

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

// Reverb core.
constexpr CSAMPLE kCombAverage = 0.25f;
constexpr CSAMPLE kAllpassGain = 0.7f;
constexpr CSAMPLE kReverbOutGain = 0.5f;
constexpr double kCombFeedbackMin = 0.5;
constexpr double kCombFeedbackRange = 0.35;

// Loop gain bound: with decay at 1.0 the comb feedback is 0.85, so each comb
// has a gain of at most 1 / (1 - 0.85) = 6.67. The four combs are summed and
// scaled by 0.25, which leaves 6.67, the allpasses have unity magnitude and
// the output is scaled by 0.5, so the reverb output never exceeds 3.33. The
// shimmer return is scaled by 0.25 and shimmer is at most 1.0, so the total
// loop gain is at most 3.33 * 0.25 = 0.83 < 1.0.
constexpr CSAMPLE kShimmerReturnGain = 0.25f;

constexpr double kToneMinHz = 300;
constexpr double kToneMaxHz = 12000;

constexpr double kTwoPi = 6.283185307179586;

} // anonymous namespace

// static
QString ShimmerEffect::getId() {
    return QStringLiteral("org.paramix.effects.shimmer");
}

// static
EffectManifestPointer ShimmerEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());

    pManifest->setAddDryToWet(true);
    pManifest->setEffectRampsFromDry(true);

    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Shimmer"));
    pManifest->setShortName(QObject::tr("Shimmer"));
    pManifest->setAuthor("Paramix");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
            "Reverb with an octave-up shifter in the feedback path, "
            "which produces a rising, bright tail."));

    EffectManifestParameterPointer decay = pManifest->addParameter();
    decay->setId("decay");
    decay->setName(QObject::tr("Decay"));
    decay->setShortName(QObject::tr("Decay"));
    decay->setDescription(QObject::tr("Length of the reverb tail"));
    decay->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    decay->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    decay->setRange(0.0, 0.5, 1.0);

    EffectManifestParameterPointer level = pManifest->addParameter();
    level->setId("level");
    level->setName(QObject::tr("Level"));
    level->setShortName(QObject::tr("Level"));
    level->setDescription(QObject::tr("How much of the signal is sent into the reverb"));
    level->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    level->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    level->setDefaultLinkType(EffectManifestParameter::LinkType::Linked);
    level->setRange(0.0, 0.3, 1.0);

    EffectManifestParameterPointer shimmer = pManifest->addParameter();
    shimmer->setId("shimmer");
    shimmer->setName(QObject::tr("Shimmer"));
    shimmer->setShortName(QObject::tr("Shimmer"));
    shimmer->setDescription(QObject::tr(
            "Amount of octave-up signal fed back into the reverb"));
    shimmer->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    shimmer->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    shimmer->setRange(0.0, 0.5, 1.0);

    EffectManifestParameterPointer tone = pManifest->addParameter();
    tone->setId("tone");
    tone->setName(QObject::tr("Tone"));
    tone->setShortName(QObject::tr("Tone"));
    tone->setDescription(QObject::tr(
            "Lowpass cutoff of the feedback path\n"
            "Fully left: dark\n"
            "Fully right: bright"));
    tone->setValueScaler(EffectManifestParameter::ValueScaler::Linear);
    tone->setUnitsHint(EffectManifestParameter::UnitsHint::Unknown);
    tone->setRange(0.0, 0.6, 1.0);

    pManifest->setBeatFx(QStringLiteral("decay"),
            QStringLiteral("level"),
            QList<double>{0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0},
            EffectManifest::BeatFxStepUnit::Percent);

    return pManifest;
}

void ShimmerEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pDecayParameter = parameters.value("decay");
    m_pLevelParameter = parameters.value("level");
    m_pShimmerParameter = parameters.value("shimmer");
    m_pToneParameter = parameters.value("tone");
}

void ShimmerEffect::processChannel(
        ShimmerGroupState* pState,
        const CSAMPLE* pInput,
        CSAMPLE* pOutput,
        const mixxx::EngineParameters& engineParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);

    const int frames = engineParameters.framesPerBuffer();
    const double sampleRate = engineParameters.sampleRate();

    const double decay = std::clamp(m_pDecayParameter->value(), 0.0, 1.0);
    const double shimmer = std::clamp(m_pShimmerParameter->value(), 0.0, 1.0);
    const double tone = std::clamp(m_pToneParameter->value(), 0.0, 1.0);
    const auto levelCurrent = static_cast<CSAMPLE_GAIN>(m_pLevelParameter->value());

    const auto combFeedback = static_cast<CSAMPLE>(
            kCombFeedbackMin + kCombFeedbackRange * decay);
    const CSAMPLE shimmerReturn = kShimmerReturnGain * static_cast<CSAMPLE>(shimmer);

    const double cutoff = std::min(0.45 * sampleRate,
            kToneMinHz * std::pow(kToneMaxHz / kToneMinHz, tone));
    const auto toneCoefficient = static_cast<CSAMPLE>(
            1.0 - std::exp(-kTwoPi * cutoff / sampleRate));

    const double windowFrames = std::max(1.0,
            static_cast<double>(pState->pitchFrames -
                    ShimmerGroupState::kPitchBufferMargin));
    const double pitchIncrement = 1.0 / windowFrames;

    // Drop the tail of a previous run so it cannot reappear.
    if (enableState == EffectEnableState::Enabling) {
        pState->clear();
    }

    RampingValue<CSAMPLE_GAIN> level(pState->prev_level, levelCurrent, frames);

    for (int frame = 0; frame < frames; ++frame) {
        const int inputIndex = frame * 2;
        const auto levelRamped = level.getNth(frame);

        // Input plus the lowpassed, octave-up copy of the previous output.
        CSAMPLE reverbIn[2];
        for (int ch = 0; ch < 2; ++ch) {
            reverbIn[ch] = pInput[inputIndex + ch] * levelRamped +
                    pState->toneState[ch] * shimmerReturn;
        }

        CSAMPLE reverbOut[2];
        for (int ch = 0; ch < 2; ++ch) {
            CSAMPLE sum = 0.0f;
            for (int c = 0; c < ShimmerGroupState::kNumCombs; ++c) {
                int& index = pState->combIndexes[ch][c];
                const CSAMPLE delayed = pState->combBufs[ch][c][index];
                pState->combBufs[ch][c][index] = SampleUtil::clampSample(
                        reverbIn[ch] + delayed * combFeedback);
                if (++index >= pState->combLengths[ch][c]) {
                    index = 0;
                }
                sum += delayed;
            }

            CSAMPLE allpassIn = sum * kCombAverage;
            for (int a = 0; a < ShimmerGroupState::kNumAllpasses; ++a) {
                int& index = pState->allpassIndexes[ch][a];
                const CSAMPLE delayed = pState->allpassBufs[ch][a][index];
                const CSAMPLE written = allpassIn + kAllpassGain * delayed;
                pState->allpassBufs[ch][a][index] = written;
                allpassIn = delayed - kAllpassGain * written;
                if (++index >= pState->allpassLengths[ch][a]) {
                    index = 0;
                }
            }
            reverbOut[ch] = SampleUtil::clampSample(allpassIn * kReverbOutGain);
        }

        // Octave-up shifter: two heads moving at twice the write speed,
        // windowed with a raised cosine and offset by half a window.
        pState->pitchPhase += pitchIncrement;
        if (pState->pitchPhase >= 1.0) {
            pState->pitchPhase -= 1.0;
        }
        const double phase = pState->pitchPhase;
        const double phaseB = phase < 0.5 ? phase + 0.5 : phase - 0.5;
        const double delayA = std::max(1.0, windowFrames * (1.0 - phase));
        const double delayB = std::max(1.0, windowFrames * (1.0 - phaseB));
        const auto windowA = static_cast<CSAMPLE>(
                0.5 - 0.5 * std::cos(kTwoPi * phase));
        const auto windowB = static_cast<CSAMPLE>(1.0 - windowA);

        for (int ch = 0; ch < 2; ++ch) {
            pState->pitchBuf[pState->pitchWriteIndex * 2 + ch] = reverbOut[ch];
        }

        for (int ch = 0; ch < 2; ++ch) {
            const double positionA = pState->pitchWriteIndex - delayA;
            const int floorA = static_cast<int>(std::floor(positionA));
            const auto fracA = static_cast<CSAMPLE>(positionA - floorA);
            const int indexA = wrapFrame(floorA, pState->pitchFrames) * 2 + ch;
            const int indexA1 = wrapFrame(floorA + 1, pState->pitchFrames) * 2 + ch;

            const double positionB = pState->pitchWriteIndex - delayB;
            const int floorB = static_cast<int>(std::floor(positionB));
            const auto fracB = static_cast<CSAMPLE>(positionB - floorB);
            const int indexB = wrapFrame(floorB, pState->pitchFrames) * 2 + ch;
            const int indexB1 = wrapFrame(floorB + 1, pState->pitchFrames) * 2 + ch;

            const CSAMPLE shiftedA = pState->pitchBuf[indexA] * (1 - fracA) +
                    pState->pitchBuf[indexA1] * fracA;
            const CSAMPLE shiftedB = pState->pitchBuf[indexB] * (1 - fracB) +
                    pState->pitchBuf[indexB1] * fracB;
            const CSAMPLE shifted = shiftedA * windowA + shiftedB * windowB;

            CSAMPLE& filtered = pState->toneState[ch];
            filtered += (shifted - filtered) * toneCoefficient;
            // Snap so a silent feedback path cannot decay into denormals.
            if (std::fabs(filtered) < 1e-15f) {
                filtered = 0.0f;
            }
        }

        pOutput[inputIndex] = reverbOut[0];
        pOutput[inputIndex + 1] = reverbOut[1];

        if (++pState->pitchWriteIndex >= pState->pitchFrames) {
            pState->pitchWriteIndex = 0;
        }
    }

    if (enableState == EffectEnableState::Disabling) {
        SampleUtil::applyRampingGain(pOutput, 1.0, 0.0, engineParameters.samplesPerBuffer());
        pState->prev_level = 0.0f;
    } else {
        pState->prev_level = levelCurrent;
    }
}
