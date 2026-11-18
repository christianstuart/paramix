#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "float32.h"
#include "literals.h"
#include "resampler.h"
#include "window.h"

namespace paramix::analysis {

namespace {

class Collector final : public SampleProcessor {
  public:
    void onSamples(const float* samples, int count) override {
        output.insert(output.end(), samples, samples + count);
    }
    void flush() override {
    }

    std::vector<float> output;
};

std::vector<float> resample(const std::vector<float>& input, int rate) {
    Collector collector;
    auto resampler = makeResampler(rate, &collector);
    constexpr int kChunk = 37;
    for (std::size_t pos = 0; pos < input.size(); pos += kChunk) {
        const int count = static_cast<int>(
                std::min<std::size_t>(kChunk, input.size() - pos));
        resampler->onSamples(input.data() + pos, count);
    }
    resampler->flush();
    return collector.output;
}

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

} // namespace

TEST(Resampler, CopyAtAnalysisRate) {
    std::vector<float> input(100);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = f32(static_cast<double>(i) / 97.0);
    }
    const std::vector<float> output = resample(input, 22050);
    ASSERT_EQ(input.size(), output.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        EXPECT_EQ(floatBits(input[i]), floatBits(output[i])) << "sample " << i;
    }
}

TEST(Resampler, Mode6MatchesNativeTapReferenceBits) {
    std::vector<float> input(testdata::kResamplerInputBitsSize);
    for (int i = 0; i < testdata::kResamplerInputBitsSize; ++i) {
        input[static_cast<std::size_t>(i)] =
                bitsFloat(testdata::kResamplerInputBits[i]);
    }
    const std::vector<float> output = resample(input, 44100);
    ASSERT_EQ(testdata::kResamplerExpectedBitsSize,
            static_cast<int>(output.size()));
    for (int i = 0; i < testdata::kResamplerExpectedBitsSize; ++i) {
        EXPECT_EQ(floatBits(output[static_cast<std::size_t>(i)]),
                testdata::kResamplerExpectedBits[i])
                << "output " << i;
    }
}

TEST(Resampler, ProducesTheDeclaredFrameCountForEveryRate) {
    constexpr int kInputFrames = 10000;
    std::vector<float> input(kInputFrames);
    for (int i = 0; i < kInputFrames; ++i) {
        input[static_cast<std::size_t>(i)] =
                f32(std::sin(2.0 * 3.14159265358979323846 * 440.0 * i / 48000.0));
    }
    for (int rate : kRates) {
        SCOPED_TRACE(rate);
        const std::vector<float> output = resample(input, rate);
        EXPECT_EQ(resampledFrameCount(kInputFrames, rate),
                static_cast<std::int64_t>(output.size()));
    }
}

TEST(Resampler, PreservesDcGainForEveryRate) {
    constexpr int kInputFrames = 8192;
    const std::vector<float> input(kInputFrames, 1.0f);
    for (int rate : kRates) {
        SCOPED_TRACE(rate);
        const std::vector<float> output = resample(input, rate);
        const int margin = std::min<int>(1000, static_cast<int>(output.size()) / 4);
        for (int i = margin; i < static_cast<int>(output.size()) - margin; ++i) {
            EXPECT_NEAR(output[static_cast<std::size_t>(i)], 1.0f, 1e-3)
                    << "sample " << i;
        }
    }
}

TEST(Resampler, ImpulseLandsAtTheResampledPosition) {
    constexpr int kInputFrames = 8192;
    constexpr int kImpulse = 3000;
    std::vector<float> input(kInputFrames, 0.0f);
    input[kImpulse] = 1.0f;
    for (int rate : kRates) {
        SCOPED_TRACE(rate);
        const std::vector<float> output = resample(input, rate);
        std::size_t peak = 0;
        float peakValue = 0.0f;
        for (std::size_t i = 0; i < output.size(); ++i) {
            if (std::fabs(output[i]) > peakValue) {
                peakValue = std::fabs(output[i]);
                peak = i;
            }
        }
        const double expected = static_cast<double>(kImpulse) * kAnalysisRate / rate;
        // The exact half-band cascade carries a small deterministic delay; the
        // general polyphase path is centered. Allow 25 output samples.
        EXPECT_NEAR(static_cast<double>(peak), expected, 25.0) << "rate " << rate;
        EXPECT_GT(peakValue, 1e-3f) << "rate " << rate;
    }
}

TEST(Resampler, IsDeterministic) {
    std::vector<float> input(4096);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = f32(std::sin(static_cast<double>(i) * 0.017));
    }
    for (int rate : kRates) {
        SCOPED_TRACE(rate);
        const std::vector<float> first = resample(input, rate);
        const std::vector<float> second = resample(input, rate);
        ASSERT_EQ(first.size(), second.size());
        for (std::size_t i = 0; i < first.size(); ++i) {
            ASSERT_EQ(floatBits(first[i]), floatBits(second[i])) << "sample " << i;
        }
    }
}

} // namespace paramix::analysis
