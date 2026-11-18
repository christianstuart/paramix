#pragma once

#include <vector>

#include "waveform/stemdetail/stemdetailentry.h"

/// Filter and envelope state of the detail reducer (spec §28).
///
/// This is a pure function of the sample index and independent of
/// samplesPerColumn, which is what makes windowed rebuilding possible.
struct StemDetailSplitterState {
    float b[4] = {};
    float c[4] = {};
    float env[3] = {};
};

/// Streaming port of Traktor's WaveformDataGenerator detail reducer.
///
/// Reproduces WaveformSplitter + computeInterpolationSplits +
/// computeColumnFromInterpolationSplits bit-for-bit in float32. All constants
/// and recurrences are derived from the shipping x86_64 slice; see
/// spec/stem-waveform-spec.md §28 and the addresses listed in ADR-0006.
class StemDetailReducer {
  public:
    explicit StemDetailReducer(const StemDetailConfig& config);

    /// Clear filter/envelope state and the column history, as reset() does.
    void reset();

    /// Process one column. left/right must point to at least
    /// config().samplesPerColumn samples. Returns height entries.
    const std::vector<StemDetailEntry>& processColumn(
            const float* left, const float* right);

    const StemDetailConfig& config() const {
        return m_config;
    }

    /// Validation / resume hooks: snapshot and restore the channel-filter and
    /// envelope state (not the column interpolation tail).
    StemDetailSplitterState splitterState() const;
    void setSplitterState(const StemDetailSplitterState& state);

    /// Advance the splitter by one sample and return its 4-float component
    /// vector. Does not touch the row accumulator.
    StemDetailEntry stepSample(float x);

  private:
    StemDetailConfig m_config;
    float m_a[4] = {};
    float m_b[4] = {};
    float m_c[4] = {};
    float m_env[3] = {};
    float m_attack[3] = {};
    float m_release[3] = {};
    float m_halfHeight = 0.0f;
    float m_lastMapped = 0.0f;
    std::vector<StemDetailEntry> m_current;
    std::vector<StemDetailEntry> m_previous;
    std::vector<StemDetailEntry> m_previousTail;
    std::vector<StemDetailEntry> m_output;
};
