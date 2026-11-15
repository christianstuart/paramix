#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <vector>

#include "stems/stemresampler.h"
#include "util/types.h"

namespace mixxx::stems {
namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> makeSine(int sampleRate, SINT frames) {
    std::vector<float> samples(static_cast<std::size_t>(frames) * 2);
    for (SINT frame = 0; frame < frames; ++frame) {
        const double time = static_cast<double>(frame) / sampleRate;
        samples[static_cast<std::size_t>(frame) * 2] =
                static_cast<float>(0.4 * std::sin(2.0 * kPi * 440.0 * time));
        samples[static_cast<std::size_t>(frame) * 2 + 1] =
                static_cast<float>(0.2 * std::sin(2.0 * kPi * 660.0 * time));
    }
    return samples;
}

} // namespace

TEST(StemResamplerTest, SameRateCopies) {
    const auto input = makeSine(44100, 4410);
    std::vector<float> output;
    SINT outputFrames = 0;
    ASSERT_TRUE(resamplePlanar(input.data(), 2, 4410, 44100, 44100, &output, &outputFrames));
    EXPECT_EQ(4410, outputFrames);
    EXPECT_EQ(input, output);
}

TEST(StemResamplerTest, RejectsInvalidArguments) {
    std::vector<float> output;
    SINT outputFrames = 0;
    EXPECT_FALSE(resamplePlanar(nullptr, 2, 100, 48000, 44100, &output, &outputFrames));
    const auto input = makeSine(48000, 100);
    EXPECT_FALSE(resamplePlanar(input.data(), 2, 100, 0, 44100, &output, &outputFrames));
}

#ifdef __STEM_SEPARATION__
TEST(StemResamplerTest, ResamplesStereoBetweenRates) {
    constexpr SINT kInputFrames = 48000;
    const auto input = makeSine(48000, kInputFrames);
    std::vector<float> output;
    SINT outputFrames = 0;
    ASSERT_TRUE(resamplePlanar(input.data(),
            2,
            kInputFrames,
            48000,
            44100,
            &output,
            &outputFrames));
    EXPECT_LE(std::abs(outputFrames - 44100), 1);
    ASSERT_EQ(static_cast<std::size_t>(outputFrames) * 2, output.size());
    double energy = 0.0;
    for (const float sample : output) {
        ASSERT_TRUE(std::isfinite(sample));
        energy += sample * sample;
    }
    EXPECT_GT(energy, 0.0);

    std::vector<float> roundTrip;
    SINT roundTripFrames = 0;
    ASSERT_TRUE(resamplePlanar(output.data(),
            2,
            outputFrames,
            44100,
            48000,
            &roundTrip,
            &roundTripFrames));
    EXPECT_LE(std::abs(roundTripFrames - kInputFrames), 2);
}
#endif

} // namespace mixxx::stems
