#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace paramix::analysis {

/// A detected transient, expressed in seconds from the start of the track.
///
/// The position is the value produced by the analysis front end (a refined
/// 32-sample burst position at the internal 22050 Hz analysis rate). The
/// weights are Traktor-compatible relative measures, not probabilities.
struct Transient {
    double positionSeconds = 0.0;
    float weight = 0.0f;
    float bassWeight = 0.0f;
};

/// Result of a completed analysis pass.
struct Result {
    bool ok = false;
    /// Raw, unsnapped BPM as detected by the tempo scan.
    float bpm = 0.0f;
    /// Phase of the first downbeat (the grid origin), in seconds.
    float anchorSeconds = 0.0f;
    /// Always 4 for the currently supported material.
    int beatsPerBar = 4;
    /// Zero-based index of the downbeat within the bar. The anchor is the
    /// downbeat, so this is 0 for a freshly analyzed grid.
    int downbeatPhase = 0;
    /// Heuristic confidence in the downbeat choice, in [0, 1].
    float confidence = 0.0f;
    /// Transient list, empty when Config::keepTransients is false.
    std::vector<Transient> transients;
};

/// Streaming facade for the Paramix analysis engine.
///
/// The engine accepts interleaved float frames at the source sample rate,
/// downmixes to mono, resamples to 22050 Hz internally and produces a raw
/// BPM plus a downbeat anchor. All stages except the final tempo/phase pass
/// are incremental.
class Engine {
  public:
    struct Config {
        int sampleRate = 44100;
        int channelCount = 2;
        /// Total number of frames at the source sample rate. Required: the
        /// analysis length and the finalize tail depend on it.
        std::int64_t totalFrames = 0;
        bool keepTransients = true;
    };

    explicit Engine(Config config);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void pushInterleaved(const float* frames, int frameCount);

    /// Runs the tempo and phase stages. Idempotent: later calls return the
    /// cached result.
    Result finish();

    void reset();

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace paramix::analysis
