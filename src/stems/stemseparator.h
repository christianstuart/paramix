#pragma once

#include <functional>
#include <string>
#include <vector>

#include "util/types.h"

namespace mixxx::stems {

/// Receives finalised interleaved eight-channel audio in drums/bass/other/
/// vocals order. Spans arrive in order and never overlap; `offset` is the
/// absolute frame position and `frames` the number of frames in `pBuffer`.
/// Returning false cancels the separation.
using StemOutputCallback =
        std::function<bool(SINT offset, const CSAMPLE* pBuffer, SINT frames)>;

/// Reports separation progress as a fraction in [0, 1].
using StemProgressCallback = std::function<void(double progress)>;

/// Returns true to cancel. Polled between output blocks.
using StemCancelCallback = std::function<bool()>;

/// Whole-track stem separator.
///
/// Backends own the model session and their model-specific preprocessing.
/// Implementations are only called from a background separation worker, never
/// from the real-time audio thread.
class StemSeparator {
  public:
    virtual ~StemSeparator() = default;

    /// Sample rate the model expects and produces.
    virtual int sampleRate() const = 0;

    /// Number of audio channels the model expects.
    virtual int channels() const = 0;

    /// Source names in the order they appear in the output.
    virtual const std::vector<std::string>& sources() const = 0;

    /// The provider that actually initialised.
    virtual std::string device() const = 0;

    /// Describes the last failure from separate().
    virtual std::string lastError() const = 0;

    /// Separates a whole track.
    ///
    /// @param pPlanarStereo `[channels][frames]` at sampleRate()
    /// @param frames number of frames; zero is allowed
    /// @param writeOutput receives interleaved eight-channel blocks
    /// @param progress optional progress in [0, 1]
    /// @param cancel optional cancellation poll
    /// @return false if the runtime failed or the caller cancelled
    virtual bool separate(const float* pPlanarStereo,
            SINT frames,
            const StemOutputCallback& writeOutput,
            const StemProgressCallback& progress = StemProgressCallback(),
            const StemCancelCallback& cancel = StemCancelCallback()) = 0;
};

} // namespace mixxx::stems
