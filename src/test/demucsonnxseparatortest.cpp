#include <gtest/gtest.h>
#include <sndfile.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "stems/demucsonnxseparator.h"
#include "stems/stemresampler.h"
#include "stems/stemseparatorfactory.h"
#include "util/types.h"

namespace mixxx::stems {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr SINT kFrameCount = 2048;
constexpr int kOutputChannels = 8;

DemucsOnnxConfig modelConfig() {
    DemucsOnnxConfig config;
    const char* modelPath = std::getenv("PARAMIX_TEST_DEMUCS_ONNX");
    if (modelPath != nullptr) {
        config.modelPath = modelPath;
    }
    config.device = "cpu";
    config.batch = 1;
    config.overlap = 0.25f;
    return config;
}

std::vector<float> makeInput(SINT frames = kFrameCount) {
    std::vector<float> input(static_cast<std::size_t>(frames) * 2);
    for (SINT frame = 0; frame < frames; ++frame) {
        const double time = static_cast<double>(frame) / 44100.0;
        input[static_cast<std::size_t>(frame) * 2] =
                static_cast<float>(0.4 * std::sin(2.0 * kPi * 440.0 * time));
        input[static_cast<std::size_t>(frame) * 2 + 1] =
                static_cast<float>(0.4 * std::sin(2.0 * kPi * 660.0 * time));
    }
    return input;
}

#ifdef __STEM_SEPARATION__
std::unique_ptr<DemucsOnnxSeparator> createTestSeparator() {
    std::string error;
    auto separator = createDemucsOnnxSeparator(modelConfig(), &error);
    EXPECT_NE(nullptr, separator) << error;
    return separator;
}
#endif

class DemucsOnnxSeparatorTest : public testing::Test {};

TEST_F(DemucsOnnxSeparatorTest, ReportsUnavailableBuild) {
    if (!isDemucsOnnxAvailable()) {
        std::string error;
        EXPECT_EQ(nullptr, createDemucsOnnxSeparator({}, &error));
        EXPECT_FALSE(error.empty());
    }
}

#ifdef __STEM_SEPARATION__
TEST_F(DemucsOnnxSeparatorTest, RejectsEmptyModelPath) {
    DemucsOnnxConfig config;
    std::string error;
    EXPECT_EQ(nullptr, createDemucsOnnxSeparator(config, &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(DemucsOnnxSeparatorTest, RejectsUnknownDevice) {
    DemucsOnnxConfig config;
    config.modelPath = "test-model.onnx";
    config.device = "bogus";
    std::string error;
    EXPECT_EQ(nullptr, createDemucsOnnxSeparator(config, &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(DemucsOnnxSeparatorTest, CoversWholeRealAudioFile) {
    const char* audioPath = std::getenv("PARAMIX_TEST_AUDIO_FILE");
    if (audioPath == nullptr) {
        GTEST_SKIP() << "set PARAMIX_TEST_AUDIO_FILE";
    }
    if (!isDemucsOnnxAvailable()) {
        GTEST_SKIP() << "ONNX Runtime is not available";
    }
    SF_INFO info{};
    SNDFILE* pFile = sf_open(audioPath, SFM_READ, &info);
    ASSERT_NE(nullptr, pFile) << sf_strerror(nullptr);
    ASSERT_EQ(2, info.channels);
    ASSERT_EQ(44100, info.samplerate);
    const SINT frames = static_cast<SINT>(info.frames);
    ASSERT_GT(frames, 0);
    std::vector<float> interleaved(static_cast<std::size_t>(frames) * 2);
    ASSERT_EQ(static_cast<sf_count_t>(frames),
            sf_readf_float(pFile, interleaved.data(), frames));
    ASSERT_EQ(0, sf_close(pFile));

    std::vector<float> planar(static_cast<std::size_t>(frames) * 2);
    for (SINT i = 0; i < frames; ++i) {
        planar[i] = interleaved[static_cast<std::size_t>(i) * 2];
        planar[frames + i] = interleaved[static_cast<std::size_t>(i) * 2 + 1];
    }

    auto pSeparator = createTestSeparator();
    ASSERT_NE(nullptr, pSeparator);
    std::vector<CSAMPLE> stems(
            static_cast<std::size_t>(frames) * kOutputChannels, 0.0f);
    SINT coveredEnd = 0;
    SINT coveredFrames = 0;
    ASSERT_TRUE(pSeparator->separate(planar.data(),
            frames,
            [&](SINT offset, const CSAMPLE* pBuffer, SINT count) {
                if (offset < 0 || count <= 0 || offset + count > frames) {
                    return false;
                }
                std::copy(pBuffer,
                        pBuffer + static_cast<std::size_t>(count) * kOutputChannels,
                        stems.begin() + static_cast<std::size_t>(offset) * kOutputChannels);
                coveredEnd = std::max(coveredEnd, offset + count);
                coveredFrames += count;
                return true;
            }));
    EXPECT_EQ(frames, coveredEnd);
    EXPECT_EQ(frames, coveredFrames);

    for (int stem = 0; stem < 4; ++stem) {
        SINT lastAudible = 0;
        for (SINT i = 0; i < frames; ++i) {
            CSAMPLE peak = 0.0f;
            for (int channel = 0; channel < 2; ++channel) {
                peak = std::max(peak,
                        std::abs(stems[static_cast<std::size_t>(i) * kOutputChannels +
                                stem * 2 + channel]));
            }
            if (peak > 1e-4f) {
                lastAudible = i;
            }
        }
        EXPECT_GT(static_cast<double>(lastAudible) / 44100.0, 150.0)
                << "stem " << stem << " ends early";
    }
}

TEST_F(DemucsOnnxSeparatorTest, SeparatesTinyModelAndReportsProgress) {
    const char* modelPath = std::getenv("PARAMIX_TEST_DEMUCS_ONNX");
    if (modelPath == nullptr || *modelPath == '\0') {
        GTEST_SKIP() << "set PARAMIX_TEST_DEMUCS_ONNX";
    }
    auto separator = createTestSeparator();
    ASSERT_NE(nullptr, separator);
    EXPECT_EQ(44100, separator->sampleRate());
    EXPECT_EQ(2, separator->channels());
    EXPECT_EQ(4u, separator->sources().size());
    EXPECT_EQ("cpu", separator->device());

    const auto input = makeInput();
    std::vector<CSAMPLE> output;
    std::vector<std::pair<SINT, SINT>> blocks;
    std::vector<double> progress;
    const bool completed = separator->separate(input.data(), kFrameCount, [&output, &blocks](SINT offset, const CSAMPLE* buffer, SINT frames) {
                blocks.emplace_back(offset, frames);
                output.insert(output.end(), buffer,
                        buffer + static_cast<std::size_t>(frames) *
                                kOutputChannels);
                return true; }, [&progress](double value) { progress.push_back(value); });
    ASSERT_TRUE(completed) << separator->lastError();
    ASSERT_FALSE(blocks.empty());
    SINT totalFrames = 0;
    for (const auto& block : blocks) {
        EXPECT_EQ(totalFrames, block.first);
        totalFrames += block.second;
    }
    EXPECT_EQ(kFrameCount, totalFrames);
    ASSERT_EQ(static_cast<std::size_t>(kFrameCount) * kOutputChannels,
            output.size());
    double squareSum = 0.0;
    for (const CSAMPLE sample : output) {
        ASSERT_TRUE(std::isfinite(sample));
        squareSum += static_cast<double>(sample) * sample;
    }
    EXPECT_GT(std::sqrt(squareSum / output.size()), 0.0);

    ASSERT_FALSE(progress.empty());
    for (std::size_t i = 0; i < progress.size(); ++i) {
        EXPECT_GE(progress[i], 0.0);
        EXPECT_LE(progress[i], 1.0);
        if (i > 0) {
            EXPECT_LE(progress[i - 1], progress[i]);
        }
    }
    EXPECT_GE(progress.back(), 0.9);
}

TEST_F(DemucsOnnxSeparatorTest, ResamplesSourceRateAroundSeparation) {
    const char* modelPath = std::getenv("PARAMIX_TEST_DEMUCS_ONNX");
    if (modelPath == nullptr || *modelPath == '\0') {
        GTEST_SKIP() << "set PARAMIX_TEST_DEMUCS_ONNX";
    }
    if (!isDemucsOnnxAvailable()) {
        GTEST_SKIP() << "ONNX Runtime is not available";
    }
    // The generated test graph expands fixed shapes and is not a separator;
    // this test needs real weights to check level and alignment.
    std::error_code sizeError;
    const auto modelBytes = std::filesystem::file_size(modelPath, sizeError);
    if (sizeError || modelBytes < 1024 * 1024) {
        GTEST_SKIP() << "use a real separation model, not the tiny test graph";
    }
    auto separator = createTestSeparator();
    ASSERT_NE(nullptr, separator);
    const int modelRate = separator->sampleRate();
    constexpr int kSourceRate = 96000;
    constexpr SINT kSourceFrames = 192000; // 2 s at 96 kHz
    ASSERT_NE(modelRate, kSourceRate);

    std::vector<float> interleaved(static_cast<std::size_t>(kSourceFrames) * 2);
    for (SINT frame = 0; frame < kSourceFrames; ++frame) {
        const double time = static_cast<double>(frame) / kSourceRate;
        interleaved[static_cast<std::size_t>(frame) * 2] = static_cast<float>(
                0.4 * std::sin(2.0 * kPi * 220.0 * time) +
                0.1 * std::sin(2.0 * kPi * 880.0 * time));
        interleaved[static_cast<std::size_t>(frame) * 2 + 1] =
                static_cast<float>(0.3 * std::sin(2.0 * kPi * 330.0 * time));
    }
    std::vector<float> sourcePlanar(static_cast<std::size_t>(kSourceFrames) * 2);
    for (SINT frame = 0; frame < kSourceFrames; ++frame) {
        sourcePlanar[frame] = interleaved[static_cast<std::size_t>(frame) * 2];
        sourcePlanar[kSourceFrames + frame] =
                interleaved[static_cast<std::size_t>(frame) * 2 + 1];
    }

    std::vector<float> modelPlanar;
    SINT modelFrames = 0;
    ASSERT_TRUE(resamplePlanar(sourcePlanar.data(),
            2,
            kSourceFrames,
            kSourceRate,
            modelRate,
            &modelPlanar,
            &modelFrames));
    EXPECT_LE(std::abs(modelFrames - kSourceFrames * modelRate / kSourceRate), 1);

    std::vector<CSAMPLE> stems(
            static_cast<std::size_t>(modelFrames) * kOutputChannels, 0.0f);
    ASSERT_TRUE(separator->separate(modelPlanar.data(),
            modelFrames,
            [&stems, modelFrames](SINT offset, const CSAMPLE* pBuffer, SINT count) {
                if (offset < 0 || count <= 0 || offset + count > modelFrames) {
                    return false;
                }
                std::copy(pBuffer,
                        pBuffer + static_cast<std::size_t>(count) * kOutputChannels,
                        stems.begin() +
                                static_cast<std::size_t>(offset) * kOutputChannels);
                return true;
            }))
            << separator->lastError();

    std::vector<float> modelStems(
            static_cast<std::size_t>(modelFrames) * kOutputChannels);
    for (SINT frame = 0; frame < modelFrames; ++frame) {
        for (int channel = 0; channel < kOutputChannels; ++channel) {
            modelStems[static_cast<std::size_t>(channel) * modelFrames + frame] =
                    stems[static_cast<std::size_t>(frame) * kOutputChannels +
                            channel];
        }
    }
    std::vector<float> sourceStems;
    SINT sourceOutputFrames = 0;
    ASSERT_TRUE(resamplePlanar(modelStems.data(),
            kOutputChannels,
            modelFrames,
            modelRate,
            kSourceRate,
            &sourceStems,
            &sourceOutputFrames));
    EXPECT_EQ(kSourceFrames, sourceOutputFrames);

    // The cache must land on exactly the source frame count so the beatgrid and
    // cue positions stay aligned.
    std::vector<CSAMPLE> finalStems(
            static_cast<std::size_t>(kSourceFrames) * kOutputChannels, 0.0f);
    for (SINT frame = 0; frame < kSourceFrames; ++frame) {
        for (int channel = 0; channel < kOutputChannels; ++channel) {
            finalStems[static_cast<std::size_t>(frame) * kOutputChannels +
                    channel] =
                    sourceStems[static_cast<std::size_t>(channel) *
                                    sourceOutputFrames +
                            frame];
        }
    }

    double inputDot = 0.0;
    double inputEnergy = 0.0;
    double stemEnergy = 0.0;
    SINT silentFrames = 0;
    for (SINT frame = 0; frame < kSourceFrames; ++frame) {
        bool anySound = false;
        for (int channel = 0; channel < 2; ++channel) {
            CSAMPLE stemSum = 0.0f;
            for (int stem = 0; stem < 4; ++stem) {
                stemSum += finalStems[static_cast<std::size_t>(frame) *
                                kOutputChannels +
                        stem * 2 + channel];
            }
            ASSERT_TRUE(std::isfinite(stemSum));
            const float input =
                    interleaved[static_cast<std::size_t>(frame) * 2 + channel];
            inputDot += input * stemSum;
            inputEnergy += input * input;
            stemEnergy += stemSum * stemSum;
            anySound = anySound || std::abs(stemSum) > 1e-4f;
        }
        if (!anySound) {
            ++silentFrames;
        }
    }
    EXPECT_GT(stemEnergy, 0.0);
    // Demucs conserves the mix: the four stems must sum back onto the input,
    // which only holds when both resampling steps are time-aligned.
    EXPECT_GT(inputDot / std::sqrt(inputEnergy * stemEnergy), 0.9)
            << "the resampled stems are not aligned with the source";
    EXPECT_LE(std::abs(std::sqrt(stemEnergy / inputEnergy) - 1.0), 0.1)
            << "the resampling changed the stem level";
    EXPECT_EQ(0, silentFrames) << "the resampled output contains digital silence";
}

TEST_F(DemucsOnnxSeparatorTest, AutoDeviceCreatesASeparator) {
    const char* modelPath = std::getenv("PARAMIX_TEST_DEMUCS_ONNX");
    if (modelPath == nullptr || *modelPath == '\0') {
        GTEST_SKIP() << "set PARAMIX_TEST_DEMUCS_ONNX";
    }
    if (!hasGpuInferenceProvider()) {
        GTEST_SKIP() << "this ONNX Runtime build has no GPU execution provider";
    }
    DemucsOnnxConfig config = modelConfig();
    config.device = "auto";
    std::string error;
    auto separator = createDemucsOnnxSeparator(config, &error);
    ASSERT_NE(nullptr, separator) << error;
    // `auto` may still end on the CPU if no accelerator accepts the graph;
    // what must hold is that asking for a GPU never fails session creation.
    EXPECT_FALSE(separator->device().empty());
    std::cout << "auto selected provider: " << separator->device() << std::endl;
}

TEST_F(DemucsOnnxSeparatorTest, CancelsAfterFirstOutputBlock) {
    const char* modelPath = std::getenv("PARAMIX_TEST_DEMUCS_ONNX");
    if (modelPath == nullptr || *modelPath == '\0') {
        GTEST_SKIP() << "set PARAMIX_TEST_DEMUCS_ONNX";
    }
    auto separator = createTestSeparator();
    ASSERT_NE(nullptr, separator);
    // A 2 s input fits in one Demucs window, so cancellation is never polled
    // before the job completes. Use enough audio for two windows.
    constexpr SINT kCancelFrames = 44100 * 10;
    const auto input = makeInput(kCancelFrames);
    int blocksWritten = 0;
    const bool completed = separator->separate(input.data(), kCancelFrames, [&blocksWritten](SINT, const CSAMPLE*, SINT) {
                ++blocksWritten;
                return true; }, {}, [&blocksWritten]() { return blocksWritten > 0; });
    EXPECT_FALSE(completed);
    EXPECT_GT(blocksWritten, 0);
    EXPECT_FALSE(separator->lastError().empty());
}

TEST_F(DemucsOnnxSeparatorTest, ZeroFramesWritesNothing) {
    const char* modelPath = std::getenv("PARAMIX_TEST_DEMUCS_ONNX");
    if (modelPath == nullptr || *modelPath == '\0') {
        GTEST_SKIP() << "set PARAMIX_TEST_DEMUCS_ONNX";
    }
    auto separator = createTestSeparator();
    ASSERT_NE(nullptr, separator);
    int writes = 0;
    EXPECT_TRUE(separator->separate(nullptr, 0, [&writes](SINT, const CSAMPLE*, SINT) {
        ++writes;
        return true;
    }));
    EXPECT_EQ(0, writes);
}

TEST_F(DemucsOnnxSeparatorTest, RejectsNullInputWithFrames) {
    const char* modelPath = std::getenv("PARAMIX_TEST_DEMUCS_ONNX");
    if (modelPath == nullptr || *modelPath == '\0') {
        GTEST_SKIP() << "set PARAMIX_TEST_DEMUCS_ONNX";
    }
    auto separator = createTestSeparator();
    ASSERT_NE(nullptr, separator);
    EXPECT_FALSE(separator->separate(nullptr, 1, [](SINT, const CSAMPLE*, SINT) { return true; }));
    EXPECT_FALSE(separator->lastError().empty());
}
#endif

} // namespace
} // namespace mixxx::stems
