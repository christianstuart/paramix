#include <gtest/gtest.h>

#include <vector>

#include "fft.h"
#include "float32.h"
#include "literals.h"
#include "logmag.h"
#include "onset.h"
#include "refine.h"
#include "window.h"

namespace paramix::analysis {

TEST(Window, MatchesJavascriptReferenceBits) {
    const WindowTable window = makeWindowTable();
    ASSERT_EQ(testdata::kWindowBitsSize, kWindowLength);
    for (int i = 0; i < kWindowLength; ++i) {
        EXPECT_FLOAT_EQ(window[static_cast<std::size_t>(i)], bitsFloat(testdata::kWindowBits[i]))
                << "index " << i;
    }
    EXPECT_EQ(window[0], 0.0f);
}

TEST(LogMagnitude, MatchesJavascriptReferenceBits) {
    for (int i = 0; i < testdata::kLogInputsSize; ++i) {
        const float value = testdata::kLogInputs[i];
        const std::uint32_t expected = testdata::kLogExpectedBits[i];
        EXPECT_EQ(floatBits(log10p1(value)), expected) << "input " << value;
    }
    EXPECT_EQ(log10p1(0.0f), 0.0f);
}

TEST(Fft, MagnitudesMatchJavascriptReferenceBits) {
    float input[kWindowLength];
    for (int i = 0; i < kWindowLength; ++i) {
        input[i] = bitsFloat(testdata::kFftInputBits[i]);
    }
    double re[kWindowLength];
    double im[kWindowLength];
    realFft512(input, re, im);
    float magnitude[kSpectrumBins];
    magnitudeSpectrum(re, im, magnitude);
    for (int k = 0; k < kSpectrumBins; ++k) {
        EXPECT_FLOAT_EQ(magnitude[k], bitsFloat(testdata::kFftExpectedBits[k]))
                << "bin " << k;
    }
}

TEST(Onset, FluxEnvelopeAndBassMatchJavascriptReferenceBits) {
    constexpr int kInputLength = 2048;
    std::vector<float> input(kInputLength);
    for (int i = 0; i < kInputLength; ++i) {
        input[static_cast<std::size_t>(i)] =
                bitsFloat(testdata::kOnsetInputBits[i]);
    }

    const int bursts = (kInputLength - kWindowLength) / kHopSize;
    BurstAnalyzer analyzer;
    for (int f = 0; f < bursts; ++f) {
        const int last = f * kHopSize + (kHopSize - 1);
        const int start = last - (kWindowLength - 1);
        float window[kWindowLength];
        for (int i = 0; i < kWindowLength; ++i) {
            const int index = start + i;
            window[i] = index >= 0 && index < kInputLength
                    ? input[static_cast<std::size_t>(index)]
                    : 0.0f;
        }
        analyzer.processBurst(window, f);
    }

    ASSERT_EQ(testdata::kOnsetExpectedBitsSize,
            static_cast<int>(analyzer.onsets().size()));
    ASSERT_EQ(testdata::kEnvelopeExpectedBitsSize,
            static_cast<int>(analyzer.envelope().size()));
    ASSERT_EQ(testdata::kBassExpectedBitsSize,
            static_cast<int>(analyzer.bass().size()));
    for (int i = 0; i < testdata::kOnsetExpectedBitsSize; ++i) {
        EXPECT_FLOAT_EQ(analyzer.onsets()[static_cast<std::size_t>(i)],
                bitsFloat(testdata::kOnsetExpectedBits[i]))
                << "onset " << i;
    }
    for (int i = 0; i < testdata::kEnvelopeExpectedBitsSize; ++i) {
        EXPECT_FLOAT_EQ(analyzer.envelope()[static_cast<std::size_t>(i)],
                bitsFloat(testdata::kEnvelopeExpectedBits[i]))
                << "envelope " << i;
    }
    for (int i = 0; i < testdata::kBassExpectedBitsSize; ++i) {
        EXPECT_FLOAT_EQ(analyzer.bass()[static_cast<std::size_t>(i)],
                bitsFloat(testdata::kBassExpectedBits[i]))
                << "bass " << i;
    }
}

TEST(Gate, PicksAndRefinementMatchJavascriptReferenceBits) {
    std::vector<float> envelope(testdata::kGateEnvelope,
            testdata::kGateEnvelope + testdata::kGateEnvelopeSize);
    std::vector<float> onsets(testdata::kGateOnsets,
            testdata::kGateOnsets + testdata::kGateOnsetsSize);

    const std::vector<PickedPeak> picks = pickPeaks(envelope);
    ASSERT_EQ(testdata::kGateFramesSize, static_cast<int>(picks.size()));
    for (int i = 0; i < testdata::kGateFramesSize; ++i) {
        EXPECT_EQ(picks[static_cast<std::size_t>(i)].frame,
                testdata::kGateFrames[i])
                << "frame " << i;
        EXPECT_EQ(picks[static_cast<std::size_t>(i)].shift,
                testdata::kGateShifts[i])
                << "shift " << i;
        EXPECT_EQ(floatBits(f32(picks[static_cast<std::size_t>(i)].rawPosition)),
                testdata::kGateRawBits[i])
                << "raw " << i;
        EXPECT_EQ(floatBits(picks[static_cast<std::size_t>(i)].flux),
                testdata::kGateFluxBits[i])
                << "flux " << i;
    }

    const std::vector<RefinedPeak> refined = refinePeaks(picks, onsets);
    ASSERT_EQ(testdata::kGateRefinedBitsSize, static_cast<int>(refined.size()));
    for (int i = 0; i < testdata::kGateRefinedBitsSize; ++i) {
        EXPECT_EQ(floatBits(f32(refined[static_cast<std::size_t>(i)].position)),
                testdata::kGateRefinedBits[i])
                << "position " << i;
    }
}

} // namespace paramix::analysis
