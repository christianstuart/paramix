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

/// Gap-search phase guess over the sorted unit phases.
float phaseGuess(const std::vector<double>& positions, float bpm);

/// Full phase stage: phase guess, circular mean, bass-salience downbeat
/// candidates and the final circular mean around the winning candidate.
PhaseResult estimatePhase(const std::vector<double>& positions,
        const std::vector<float>& bassWeights,
        float bpm);

} // namespace paramix::analysis
