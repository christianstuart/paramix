#pragma once

#include <vector>

namespace paramix::analysis {

/// One variance-minimizing tempo scan. Returns 0 when no candidate has a
/// positive score.
float bpmSearch(const std::vector<double>& positions,
        float centre,
        float halfWidth,
        int steps);

struct TempoResult {
    float coarse = 0.0f;
    float fine = 0.0f;
    float bpm = 0.0f;
};

/// The native three-pass chain: 131.5 +/- 43.5 (501 steps), then the two
/// refinement passes of 101 steps each.
TempoResult detectBpm(const std::vector<double>& positions);

} // namespace paramix::analysis
