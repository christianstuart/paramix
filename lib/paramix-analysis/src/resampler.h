#pragma once

#include <cstdint>
#include <memory>

namespace paramix::analysis {

/// Streaming consumer of mono float samples at the internal analysis rate.
class SampleProcessor {
  public:
    virtual ~SampleProcessor() = default;
    virtual void onSamples(const float* samples, int count) = 0;
    /// Signals end of input. Implementations must drain their internal delay
    /// lines and forward the remaining samples before calling onSamples on
    /// their own sink.
    virtual void flush() = 0;
};

/// Creates a mono resampler from `inputRate` to 22050 Hz. The returned object
/// owns the whole stage chain and forwards output to `sink`.
std::unique_ptr<SampleProcessor> makeResampler(int inputRate, SampleProcessor* sink);

/// Number of output frames produced for `inputFrames` input frames.
std::int64_t resampledFrameCount(std::int64_t inputFrames, int inputRate);

} // namespace paramix::analysis
