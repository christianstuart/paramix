#pragma once

#include <vector>

namespace paramix::analysis {

/// One peak emitted by the coarse 7-point gate.
struct PickedPeak {
    int frame = 0;
    int shift = 0;
    float flux = 0.0f;
    int batch = 0;
    bool finalize = false;
    double rawPosition = 0.0;
};

/// One transient after the +/-12-burst refinement pass. Finalize-pass peaks are
/// not refined by the engine.
struct RefinedPeak {
    double position = 0.0;
    float flux = 0.0f;
    bool finalize = false;
    int frame = 0;
    int subIndex = 0;
};

/// Applies the 43-frame batch threshold, the strict 7-point gate, the 0/1/2
/// slope shift and the finalize pass over the partial last batch.
std::vector<PickedPeak> pickPeaks(const std::vector<float>& envelope);

/// Refines full-batch peaks against the fine onset array.
std::vector<RefinedPeak> refinePeaks(
        const std::vector<PickedPeak>& peaks, const std::vector<float>& onsets);

} // namespace paramix::analysis
