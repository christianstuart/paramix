#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "float32.h"
#include "literals.h"
#include "phase.h"
#include "tempo.h"

namespace paramix::analysis {

TEST(Tempo, DetectsAPerfectHalfSecondGrid) {
    std::vector<double> positions;
    for (int i = 0; i < 101; ++i) {
        positions.push_back(0.25 + i * 0.5);
    }
    const TempoResult result = detectBpm(positions);
    EXPECT_NEAR(result.bpm, 120.0f, 1e-3f);
    EXPECT_GT(result.coarse, 0.0f);
    EXPECT_GT(result.fine, 0.0f);
}

TEST(Tempo, RejectsAnEmptyTransientList) {
    const std::vector<double> positions;
    const TempoResult result = detectBpm(positions);
    EXPECT_EQ(result.bpm, 0.0f);
}

TEST(Tempo, MatchesJavascriptReferenceBitsOnTheClickFixture) {
    std::vector<double> positions(testdata::kTempoPositions,
            testdata::kTempoPositions + testdata::kTempoPositionsSize);
    const TempoResult result = detectBpm(positions);
    EXPECT_EQ(floatBits(result.bpm), testdata::kTempoExpectedBpmBits[0]);
    EXPECT_NEAR(result.bpm, 120.0f, 0.01f);
}

TEST(Phase, CircularMeanFindsTheCommonPhase) {
    std::vector<float> positions;
    for (int i = 0; i < 20; ++i) {
        positions.push_back(f32(0.25 + i * 0.5));
    }
    const float mean = circularMean(positions, 120.0f, 0.25f);
    EXPECT_NEAR(mean, 0.25f, 1e-4f);
}

TEST(Phase, AnchorMatchesJavascriptReferenceBitsOnTheClickFixture) {
    std::vector<double> positions(testdata::kTempoPositions,
            testdata::kTempoPositions + testdata::kTempoPositionsSize);
    std::vector<float> bass(testdata::kTempoBassWeights,
            testdata::kTempoBassWeights + testdata::kTempoBassWeightsSize);
    ASSERT_EQ(positions.size(), bass.size());

    const TempoResult tempo = detectBpm(positions);
    const PhaseResult phase = estimatePhase(positions, bass, tempo.bpm);
    ASSERT_TRUE(phase.ok);
    EXPECT_EQ(floatBits(phase.anchor), testdata::kTempoExpectedAnchorBits[0]);
    EXPECT_NEAR(phase.anchor, 0.25f, 0.005f);
    EXPECT_GE(phase.bestScore, phase.secondScore);
}

TEST(Phase, ChoosesTheAccentedEveryFourthBeat) {
    std::vector<double> positions;
    std::vector<float> bass;
    for (int i = 0; i < 32; ++i) {
        positions.push_back(0.25 + i * 0.5);
        bass.push_back(i % 4 == 0 ? 10.0f : 0.1f);
    }
    const PhaseResult phase = estimatePhase(positions, bass, 120.0f);
    ASSERT_TRUE(phase.ok);
    EXPECT_NEAR(phase.anchor, 0.25f, 1e-3f);
    EXPECT_NEAR(phase.phase0, 0.25f, 1e-3f);
}

} // namespace paramix::analysis
