#pragma once

#include <vector>

#include "window.h"

namespace paramix::analysis {

/// Per-burst spectral front end: window, FFT, magnitude, log, fine and coarse
/// rectified flux, 43-frame envelope assembly and the bass envelope used by the
/// downbeat stage.
class BurstAnalyzer {
  public:
    BurstAnalyzer();

    /// Processes one burst. `samples` points at the 512-sample analysis window
    /// ending at the current 32-sample burst (zero padded before the start of
    /// the track). `burstIndex` is the absolute burst number.
    void processBurst(const float* samples, int burstIndex);

    const std::vector<float>& onsets() const {
        return m_onsets;
    }
    const std::vector<float>& envelope() const {
        return m_envelope;
    }
    /// Per-burst F(f32(log[1] + log[0]) + log[2]), the bass envelope.
    const std::vector<float>& bass() const {
        return m_bass;
    }

  private:
    WindowTable m_window;
    float m_fineHistory[kFluxBins] = {};
    float m_coarseHistory[kFluxBins] = {};
    float m_previousCoarse = 0.0f;
    std::vector<float> m_onsets;
    std::vector<float> m_envelope;
    std::vector<float> m_bass;
};

} // namespace paramix::analysis
