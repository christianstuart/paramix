#pragma once

#include <QMap>
#include <algorithm>

#include "effects/backends/effectprocessor.h"
#include "engine/engine.h"
#include "util/class.h"
#include "util/samplebuffer.h"

/// Reverb with an octave-up shifter in the feedback path.
///
/// The reverb core is a small Schroeder network (four parallel combs followed
/// by two allpasses per channel). The left and right comb lengths differ a
/// little so the tail is decorrelated. A copy of the network output is shifted
/// up one octave and lowpassed, then fed back into the network input.
class ShimmerGroupState : public EffectState {
  public:
    static constexpr int kNumCombs = 4;
    static constexpr int kNumAllpasses = 2;

    // Comb and allpass delays of the reverb core, in seconds.
    static constexpr double kCombSeconds[kNumCombs] = {0.0297, 0.0371, 0.0411, 0.0437};
    static constexpr double kAllpassSeconds[kNumAllpasses] = {0.0050, 0.0017};

    // Octave-up shifter window. The heads read at most one window behind the
    // write position.
    static constexpr double kWindowSeconds = 0.07;
    static constexpr int kPitchBufferMargin = 4;

    ShimmerGroupState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters) {
        audioParametersChanged(engineParameters);
        clear();
    }
    ~ShimmerGroupState() override = default;

    void audioParametersChanged(const mixxx::EngineParameters& engineParameters) {
        const double sampleRate = engineParameters.sampleRate();
        // Different lengths for the right channel decorrelate the tail.
        const int rightOffset = std::max(1, static_cast<int>(sampleRate) / 1500);
        for (int c = 0; c < kNumCombs; ++c) {
            const int lengthLeft = std::max(2,
                    static_cast<int>(kCombSeconds[c] * sampleRate));
            combLengths[0][c] = lengthLeft;
            combLengths[1][c] = lengthLeft + rightOffset;
            combBufs[0][c] = mixxx::SampleBuffer(combLengths[0][c]);
            combBufs[1][c] = mixxx::SampleBuffer(combLengths[1][c]);
        }
        for (int a = 0; a < kNumAllpasses; ++a) {
            const int lengthLeft = std::max(2,
                    static_cast<int>(kAllpassSeconds[a] * sampleRate));
            allpassLengths[0][a] = lengthLeft;
            allpassLengths[1][a] = lengthLeft + rightOffset;
            allpassBufs[0][a] = mixxx::SampleBuffer(allpassLengths[0][a]);
            allpassBufs[1][a] = mixxx::SampleBuffer(allpassLengths[1][a]);
        }
        pitchFrames = std::max(4,
                static_cast<int>(kWindowSeconds * sampleRate) + kPitchBufferMargin);
        pitchBuf = mixxx::SampleBuffer(pitchFrames * 2);
    };

    void clear() {
        for (int ch = 0; ch < 2; ++ch) {
            for (int c = 0; c < kNumCombs; ++c) {
                combBufs[ch][c].clear();
                combIndexes[ch][c] = 0;
            }
            for (int a = 0; a < kNumAllpasses; ++a) {
                allpassBufs[ch][a].clear();
                allpassIndexes[ch][a] = 0;
            }
            toneState[ch] = 0.0f;
        }
        pitchBuf.clear();
        pitchWriteIndex = 0;
        pitchPhase = 0.0;
        prev_level = 0.0f;
    };

    mixxx::SampleBuffer combBufs[2][kNumCombs];
    int combLengths[2][kNumCombs];
    int combIndexes[2][kNumCombs];

    mixxx::SampleBuffer allpassBufs[2][kNumAllpasses];
    int allpassLengths[2][kNumAllpasses];
    int allpassIndexes[2][kNumAllpasses];

    mixxx::SampleBuffer pitchBuf;
    int pitchFrames;
    int pitchWriteIndex;
    double pitchPhase;

    CSAMPLE toneState[2];
    CSAMPLE_GAIN prev_level;
};

class ShimmerEffect : public EffectProcessorImpl<ShimmerGroupState> {
  public:
    ShimmerEffect() = default;
    ~ShimmerEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            ShimmerGroupState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameterPointer m_pDecayParameter;
    EngineEffectParameterPointer m_pLevelParameter;
    EngineEffectParameterPointer m_pShimmerParameter;
    EngineEffectParameterPointer m_pToneParameter;

    DISALLOW_COPY_AND_ASSIGN(ShimmerEffect);
};
