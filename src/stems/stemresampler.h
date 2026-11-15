#pragma once

#include <vector>

#include "util/types.h"

namespace mixxx::stems {

/// Resamples planar [channels][frames] audio from `inputRate` to `outputRate`.
/// The samples are band-limited with a windowed-sinc filter when this build
/// has ONNX Runtime (which supplies the vendored resampler); builds without it
/// return false, because separation itself is unavailable.
bool resamplePlanar(const float* pInput,
        int channels,
        SINT frames,
        int inputRate,
        int outputRate,
        std::vector<float>* pOutput,
        SINT* pOutputFrames);

} // namespace mixxx::stems
