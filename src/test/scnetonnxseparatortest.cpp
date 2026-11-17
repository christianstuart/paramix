#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "stems/scnetonnxseparator.h"
#include "util/types.h"

namespace mixxx::stems {
namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> makeInput(SINT frames) {
    std::vector<float> input(static_cast<std::size_t>(frames) * 2);
    for (SINT frame = 0; frame < frames; ++frame) {
        const double time = static_cast<double>(frame) / 44100.0;
        input[static_cast<std::size_t>(frame)] =
                static_cast<float>(0.3 * std::sin(2.0 * kPi * 440.0 * time));
        input[static_cast<std::size_t>(frames) + frame] =
                static_cast<float>(0.3 * std::sin(2.0 * kPi * 660.0 * time));
    }
    return input;
}

class ScnetOnnxSeparatorTest : public testing::Test {};

TEST_F(ScnetOnnxSeparatorTest, ReportsUnavailableBuild) {
#ifndef __STEM_SEPARATION__
    EXPECT_FALSE(isScnetOnnxAvailable());
#endif
}

#ifdef __STEM_SEPARATION__
TEST_F(ScnetOnnxSeparatorTest, RejectsEmptyModelPath) {
    ScnetOnnxConfig config;
    std::string error;
    EXPECT_EQ(nullptr, ScnetOnnxSeparator::create(config, &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(ScnetOnnxSeparatorTest, RejectsUnknownGraph) {
    ScnetOnnxConfig config;
    config.modelPath = "does-not-exist.onnx";
    config.device = "cpu";
    std::string error;
    EXPECT_EQ(nullptr, ScnetOnnxSeparator::create(config, &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(ScnetOnnxSeparatorTest, SeparatesShortClipWithRealModel) {
    const char* modelPath = std::getenv("PARAMIX_TEST_SCNET_ONNX");
    if (modelPath == nullptr) {
        GTEST_SKIP() << "set PARAMIX_TEST_SCNET_ONNX to run the real SCNet graph";
    }
    if (!isScnetOnnxAvailable()) {
        GTEST_SKIP() << "ONNX Runtime is not available";
    }

    ScnetOnnxConfig config;
    config.modelPath = modelPath;
    config.device = "auto";
    config.threads = 2;
    std::string error;
    auto separator = ScnetOnnxSeparator::create(config, &error);
    ASSERT_NE(nullptr, separator) << error;
    EXPECT_EQ(44100, separator->sampleRate());
    EXPECT_EQ(2, separator->channels());
    ASSERT_EQ(4, separator->sources().size());
    EXPECT_EQ("drums", separator->sources()[0]);
    EXPECT_EQ("bass", separator->sources()[1]);
    EXPECT_EQ("other", separator->sources()[2]);
    EXPECT_EQ("vocals", separator->sources()[3]);
    if (std::getenv("PARAMIX_TEST_REQUIRE_GPU") != nullptr) {
        EXPECT_NE("cpu", separator->device());
    }

    // Longer than two 11 s segments, so the overlap-add and border handling
    // a full track goes through are exercised, not only a single segment.
    constexpr SINT kFrames = 44100 * 25;
    const std::vector<float> input = makeInput(kFrames);
    std::vector<CSAMPLE> output;
    bool callbackCalled = false;
    const bool separated = separator->separate(input.data(),
            kFrames,
            [&](SINT offset, const CSAMPLE* pBuffer, SINT frames) {
                EXPECT_EQ(0, offset);
                EXPECT_EQ(kFrames, frames);
                output.assign(pBuffer,
                        pBuffer + static_cast<std::size_t>(frames) * 8);
                callbackCalled = true;
                return true;
            });
    ASSERT_TRUE(separated) << separator->lastError();
    ASSERT_TRUE(callbackCalled);
    ASSERT_EQ(static_cast<std::size_t>(kFrames) * 8, output.size());

    double energy = 0.0;
    for (CSAMPLE sample : output) {
        EXPECT_TRUE(std::isfinite(sample));
        energy += static_cast<double>(sample) * sample;
    }
    EXPECT_GT(energy, 0.0);

    // Smoke check that the host STFT/iSTFT preserves the mixture: the four
    // stems should sum back to something correlated with the input.
    double inputEnergy = 0.0;
    double sumEnergy = 0.0;
    double dot = 0.0;
    for (SINT frame = 0; frame < kFrames; ++frame) {
        for (int channel = 0; channel < 2; ++channel) {
            double sum = 0.0;
            for (int source = 0; source < 4; ++source) {
                sum += output[static_cast<std::size_t>(frame) * 8 +
                        static_cast<std::size_t>(source) * 2 + channel];
            }
            const double inputSample =
                    input[static_cast<std::size_t>(channel) * kFrames + frame];
            inputEnergy += inputSample * inputSample;
            sumEnergy += sum * sum;
            dot += inputSample * sum;
        }
    }
    EXPECT_GT(inputEnergy, 0.0);
    EXPECT_GT(sumEnergy, 0.0);
    const double correlation = dot / std::sqrt(inputEnergy * sumEnergy);
    EXPECT_GT(correlation, 0.9);
    // Correlation is blind to gain: the stems also have to sum back to the
    // level of the input, not a scaled copy of it.
    EXPECT_NEAR(1.0, std::sqrt(sumEnergy / inputEnergy), 0.25);
}
#endif

} // namespace
} // namespace mixxx::stems
