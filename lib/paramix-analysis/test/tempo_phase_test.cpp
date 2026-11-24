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

TEST(Tempo, ReturnsTheWindowCentreForASingleTransient) {
    const std::vector<double> positions{12.345};
    const TempoConfig config{131.5f, 43.5f};
    const TempoResult result = detectBpm(positions, config);
    EXPECT_EQ(result.bpm, 131.5f);
}

TEST(Tempo, HonoursAnExplicitTraktorWindow) {
    std::vector<double> positions;
    for (int i = 0; i < 101; ++i) {
        positions.push_back(0.25 + i * (60.0 / 100.0));
    }
    const TempoResult narrow = detectBpm(positions, TempoConfig{100.0f, 20.0f});
    const TempoResult wide = detectBpm(positions, TempoConfig{131.5f, 43.5f});
    EXPECT_NEAR(narrow.bpm, 100.0f, 1e-3f);
    EXPECT_NEAR(wide.bpm, 100.0f, 1e-3f);
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

TEST(Phase, AnchorMatchesTheClickFixturePhase) {
    std::vector<double> positions(testdata::kTempoPositions,
            testdata::kTempoPositions + testdata::kTempoPositionsSize);
    std::vector<float> weights(positions.size(), 1.0f);
    std::vector<float> bass(testdata::kTempoBassWeights,
            testdata::kTempoBassWeights + testdata::kTempoBassWeightsSize);
    ASSERT_EQ(positions.size(), bass.size());

    const TempoResult tempo = detectBpm(positions);
    const PhaseResult phase = estimatePhase(positions, weights, bass, tempo.bpm);
    ASSERT_TRUE(phase.ok);
    EXPECT_NEAR(phase.anchor, 0.25f, 0.004f);
    EXPECT_GE(phase.bestScore, phase.secondScore);
}

TEST(Phase, IgnoresDenseOffBeatTransients) {
    // Hi-hats every 16th, kick and snare only on the beat. Averaging the
    // transient positions lands on the 16th grid; the weighted comb must stay
    // on the beat.
    std::vector<double> positions;
    std::vector<float> weights;
    std::vector<float> bass;
    for (int i = 0; i < 64; ++i) {
        const double position = 0.25 + i * 0.125;
        positions.push_back(position);
        weights.push_back(i % 4 == 0 ? 0.9f : 0.05f);
        bass.push_back(i % 4 == 0 ? 8.0f : 0.2f);
    }
    const PhaseResult phase = estimatePhase(positions, weights, bass, 120.0f);
    ASSERT_TRUE(phase.ok);
    EXPECT_NEAR(phase.anchor, 0.25f, 0.004f);
    EXPECT_NEAR(phase.phase0, 0.25f, 0.004f);
}

TEST(Phase, ChoosesTheAccentedEveryFourthBeat) {
    std::vector<double> positions;
    std::vector<float> weights;
    std::vector<float> bass;
    for (int i = 0; i < 32; ++i) {
        positions.push_back(0.25 + i * 0.5);
        weights.push_back(i % 4 == 0 ? 0.9f : 0.3f);
        bass.push_back(i % 4 == 0 ? 10.0f : 0.1f);
    }
    const PhaseResult phase = estimatePhase(positions, weights, bass, 120.0f);
    ASSERT_TRUE(phase.ok);
    EXPECT_NEAR(phase.anchor, 0.25f, 1e-3f);
    EXPECT_NEAR(phase.phase0, 0.25f, 1e-3f);
}

} // namespace paramix::analysis
