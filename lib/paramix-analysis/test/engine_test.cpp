#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "float32.h"
#include "paramix/analysis/analysis.h"
#include "paramix/analysis/bpmrange.h"

namespace paramix::analysis {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kFixtureBpm = 140.0;
constexpr double kFixturePhase = 0.25;
constexpr double kFixtureSeconds = 30.0;
constexpr int kRates[] = {
        8000,
        11025,
        16000,
        22050,
        24000,
        32000,
        44100,
        48000,
        88200,
        96000,
        176400,
        192000,
};

std::vector<float> makeClickTrack(int rate,
        double bpm = kFixtureBpm,
        double seconds = kFixtureSeconds) {
    const int frames = static_cast<int>(std::floor(seconds * rate));
    std::vector<float> samples(static_cast<std::size_t>(frames), 0.0f);
    const double period = 60.0 / bpm;
    int beat = 0;
    for (double time = kFixturePhase; time < seconds - 0.1;
            time += period, ++beat) {
        const int start = static_cast<int>(std::floor(time * rate));
        const int clickLength = static_cast<int>(std::floor(rate * 0.006));
        for (int i = 0; i < clickLength && start + i < frames; ++i) {
            const double envelope = std::exp(-i / (rate * 0.0015));
            const double value = envelope *
                    std::sin(2.0 * kPi * 2400.0 * i / rate);
            samples[static_cast<std::size_t>(start + i)] = f32(
                    samples[static_cast<std::size_t>(start + i)] + f32(value));
        }
        if (beat % 4 == 0) {
            const int lowLength = static_cast<int>(std::floor(rate * 0.03));
            for (int i = 0; i < lowLength && start + i < frames; ++i) {
                const double envelope = std::exp(-i / (rate * 0.008));
                const double value = 0.9 * envelope *
                        std::sin(2.0 * kPi * 55.0 * i / rate);
                samples[static_cast<std::size_t>(start + i)] = f32(
                        samples[static_cast<std::size_t>(start + i)] +
                        f32(value));
            }
        }
    }
    return samples;
}

Result analyze(const std::vector<float>& mono,
        int rate,
        int channels,
        bool keepTransients = true,
        Engine::Config config = Engine::Config{}) {
    std::vector<float> interleaved;
    const float* data = mono.data();
    if (channels == 2) {
        interleaved.resize(mono.size() * 2);
        for (std::size_t i = 0; i < mono.size(); ++i) {
            interleaved[2 * i] = mono[i];
            interleaved[2 * i + 1] = mono[i];
        }
        data = interleaved.data();
    }

    config.sampleRate = rate;
    config.channelCount = channels;
    config.totalFrames = static_cast<std::int64_t>(mono.size());
    config.keepTransients = keepTransients;
    Engine engine(config);
    constexpr int kChunkFrames = 4096;
    const int frames = static_cast<int>(mono.size());
    for (int pos = 0; pos < frames; pos += kChunkFrames) {
        const int count = std::min(kChunkFrames, frames - pos);
        engine.pushInterleaved(data + static_cast<std::size_t>(pos) * channels,
                count);
    }
    return engine.finish();
}

void expectSameResult(const Result& left, const Result& right) {
    ASSERT_EQ(left.ok, right.ok);
    ASSERT_EQ(floatBits(left.bpm), floatBits(right.bpm));
    ASSERT_EQ(floatBits(left.anchorSeconds), floatBits(right.anchorSeconds));
    ASSERT_EQ(left.beatsPerBar, right.beatsPerBar);
    ASSERT_EQ(left.downbeatPhase, right.downbeatPhase);
    ASSERT_EQ(floatBits(left.confidence), floatBits(right.confidence));
    ASSERT_EQ(left.transients.size(), right.transients.size());
    for (std::size_t i = 0; i < left.transients.size(); ++i) {
        ASSERT_EQ(left.transients[i].positionSeconds,
                right.transients[i].positionSeconds)
                << "position " << i;
        ASSERT_EQ(floatBits(left.transients[i].weight),
                floatBits(right.transients[i].weight))
                << "weight " << i;
        ASSERT_EQ(floatBits(left.transients[i].bassWeight),
                floatBits(right.transients[i].bassWeight))
                << "bassWeight " << i;
    }
}

} // namespace

TEST(Engine, DetectsBpmAndDownbeatForEveryRateBucket) {
    for (int rate : kRates) {
        SCOPED_TRACE(rate);
        const std::vector<float> samples = makeClickTrack(rate);
        const Result result = analyze(samples, rate, 1);
        ASSERT_TRUE(result.ok) << "rate " << rate;
        EXPECT_NEAR(result.bpm, kFixtureBpm, 2e-3) << "rate " << rate;
        EXPECT_NEAR(result.anchorSeconds, kFixturePhase, 0.003)
                << "rate " << rate;
        EXPECT_EQ(result.beatsPerBar, 4);
        EXPECT_EQ(result.downbeatPhase, 0);
        EXPECT_GT(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);
        EXPECT_GT(result.transients.size(), 10U);
    }
}

TEST(Engine, StereoAndMonoProduceTheSameResult) {
    const std::vector<float> samples = makeClickTrack(44100);
    const Result mono = analyze(samples, 44100, 1);
    const Result stereo = analyze(samples, 44100, 2);
    expectSameResult(mono, stereo);
}

TEST(Engine, IsDeterministicOverThreeRuns) {
    const std::vector<float> samples = makeClickTrack(22050);
    const Result first = analyze(samples, 22050, 1);
    const Result second = analyze(samples, 22050, 1);
    const Result third = analyze(samples, 22050, 1);
    expectSameResult(first, second);
    expectSameResult(first, third);
}

TEST(Engine, EmptyInputIsRejected) {
    Engine::Config config;
    config.sampleRate = 44100;
    config.channelCount = 2;
    config.totalFrames = 0;
    Engine engine(config);
    const Result result = engine.finish();
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.bpm, 0.0f);
}

TEST(Engine, CanSkipTransientRetention) {
    const std::vector<float> samples = makeClickTrack(22050);
    const Result result = analyze(samples, 22050, 1, false);
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.transients.empty());
    EXPECT_NEAR(result.bpm, kFixtureBpm, 2e-3);
}

TEST(Engine, FinishIsIdempotent) {
    const std::vector<float> samples = makeClickTrack(22050);
    Engine::Config config;
    config.sampleRate = 22050;
    config.channelCount = 1;
    config.totalFrames = static_cast<std::int64_t>(samples.size());
    Engine engine(config);
    engine.pushInterleaved(samples.data(), static_cast<int>(samples.size()));
    const Result first = engine.finish();
    const Result second = engine.finish();
    expectSameResult(first, second);
}

TEST(Engine, UsesTheSelectedTraktorBpmRange) {
    const std::vector<float> samples = makeClickTrack(22050, 75.0, 20.0);
    Engine::Config config;
    config.bpmRangeMin = 58;
    config.bpmRangeMax = 115;
    const Result result = analyze(samples, 22050, 1, true, config);
    ASSERT_TRUE(result.ok);
    EXPECT_NEAR(result.bpm, 75.0, 0.01);
    EXPECT_EQ(result.bpmRangeMin, 58);
    EXPECT_EQ(result.bpmRangeMax, 115);
}

TEST(Engine, ExistingBpmSelectsAMatchingTraktorRange) {
    const std::vector<float> samples = makeClickTrack(22050, 75.0, 20.0);
    Engine::Config config;
    config.bpmRangeMin = 88;
    config.bpmRangeMax = 175;
    config.existingBpm = 75.0;
    config.matchExistingBpm = true;
    const Result result = analyze(samples, 22050, 1, true, config);
    ASSERT_TRUE(result.ok);
    EXPECT_NEAR(result.bpm, 75.0, 0.01);
    EXPECT_FALSE(result.bpmRangeMin == 88 && result.bpmRangeMax == 175);
    EXPECT_FALSE(result.bpmHintMismatch);
}

TEST(Engine, UnmatchedExistingBpmKeepsTheExistingGrid) {
    const std::vector<float> samples = makeClickTrack(22050, 140.0, 20.0);
    Engine::Config config;
    config.bpmRangeMin = 88;
    config.bpmRangeMax = 175;
    config.existingBpm = 60.0;
    config.matchExistingBpm = true;
    config.preserveGridOnBpmMismatch = true;
    const Result result = analyze(samples, 22050, 1, true, config);
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.bpmHintMismatch);
}

TEST(Engine, ExistingBpmMatchingNeverSelectsTheAutomaticPreset) {
    const std::vector<float> samples = makeClickTrack(22050, 140.0, 30.0);
    Engine::Config config;
    config.bpmRangeMin = 88;
    config.bpmRangeMax = 175;
    config.existingBpm = 140.0;
    config.matchExistingBpm = true;
    const Result result = analyze(samples, 22050, 1, true, config);
    ASSERT_TRUE(result.ok);
    for (int i = 0; i < kBpmRangePresetCount; ++i) {
        if (kBpmRangePresets[i].automatic) {
            EXPECT_FALSE(result.bpmRangeMin == kBpmRangePresets[i].minBpm &&
                    result.bpmRangeMax == kBpmRangePresets[i].maxBpm);
        }
    }
}

TEST(Engine, ExistingBpmMatchingCanBeDisabled) {
    const std::vector<float> samples = makeClickTrack(22050, 75.0, 20.0);
    Engine::Config config;
    config.bpmRangeMin = 58;
    config.bpmRangeMax = 115;
    config.existingBpm = 75.0;
    config.matchExistingBpm = false;
    const Result result = analyze(samples, 22050, 1, true, config);
    ASSERT_TRUE(result.ok);
    EXPECT_NEAR(result.bpm, 75.0, 0.01);
    EXPECT_EQ(result.bpmRangeMin, 58);
    EXPECT_EQ(result.bpmRangeMax, 115);
}

TEST(BpmRange, MatchesTheNativePresetTable) {
    EXPECT_EQ(10, kBpmRangePresetCount);
    const int expectedMin[kBpmRangePresetCount] = {
            95, 58, 68, 78, 88, 98, 108, 118, 128, 110};
    const int expectedMax[kBpmRangePresetCount] = {
            115, 115, 135, 155, 175, 195, 215, 235, 255, 220};
    for (int i = 0; i < kBpmRangePresetCount; ++i) {
        EXPECT_EQ(expectedMin[i], kBpmRangePresets[i].minBpm);
        EXPECT_EQ(expectedMax[i], kBpmRangePresets[i].maxBpm);
        EXPECT_EQ(i == kBpmRangePresetCount - 1, kBpmRangePresets[i].automatic);
    }
    EXPECT_EQ(88, defaultBpmRange().minBpm);
    EXPECT_EQ(175, defaultBpmRange().maxBpm);
    EXPECT_EQ(78, bpmRangeForPreset(3).minBpm);
    EXPECT_EQ(155, bpmRangeForPreset(3).maxBpm);
    EXPECT_EQ(88, bpmRangeForPreset(-1).minBpm);
    EXPECT_EQ(175, bpmRangeForPreset(kBpmRangePresetCount).maxBpm);
}

} // namespace paramix::analysis
