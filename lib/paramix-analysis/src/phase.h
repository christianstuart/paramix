#pragma once

#include <vector>

namespace paramix::analysis {

struct PhaseResult {
    bool ok = false;
    /// Final downbeat anchor in seconds.
    float anchor = 0.0f;
    float phase0 = 0.0f;
    int candidate = 0;
    float bestScore = 0.0f;
    float secondScore = 0.0f;
};

/// Weighted circular mean with the squared triangular window of width
/// 1/64 period. `positions` are seconds.
float circularMean(const std::vector<float>& positions, float bpm, float phase);

/// The beat phase that maximizes transient support: every transient
/// contributes according to its weight and its distance to the nearest grid
/// line, with a linear taper over a 30 ms window. This is what Traktor's grid
/// anchor follows; averaging the transient positions instead lets dense
/// off-beat hi-hats and vocals pull the grid away from the beat.
float combPhase(const std::vector<float>& positions,
        const std::vector<float>& weights,
        float bpm);

/// Full phase stage: comb phase, circular-mean refinement, bass-salience
/// downbeat candidates and the final circular mean around the winning
/// candidate.
PhaseResult estimatePhase(const std::vector<double>& positions,
        const std::vector<float>& weights,
        const std::vector<float>& bassWeights,
        float bpm);

} // namespace paramix::analysis
