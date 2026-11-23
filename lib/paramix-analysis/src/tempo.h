#pragma once

#include <vector>

namespace paramix::analysis {

struct TempoConfig {
    float centreBpm = 131.5f;
    float halfWidthBpm = 43.5f;
};

/// One variance-minimizing tempo scan. Returns the window centre when no
/// candidate has a positive score, matching the native engine.
float bpmSearch(const std::vector<double>& positions,
        float centre,
        float halfWidth,
        int steps);

struct TempoResult {
    float coarse = 0.0f;
    float fine = 0.0f;
    float bpm = 0.0f;
};

/// The native three-pass chain: centre +/- half width (501 steps), then the two
/// refinement passes of 101 steps each.
TempoResult detectBpm(const std::vector<double>& positions, TempoConfig config);

TempoResult detectBpm(const std::vector<double>& positions);

} // namespace paramix::analysis
