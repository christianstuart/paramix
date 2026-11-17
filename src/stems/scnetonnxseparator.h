#pragma once

#include <memory>
#include <string>

#include "stems/stemseparator.h"

namespace mixxx::stems {

/// Configuration for the SCNet ONNX backend.
struct ScnetOnnxConfig {
    /// Path to the exported SCNet graph (scnet.onnx or scnet-large.onnx).
    std::string modelPath;
    /// "cpu" or "auto". GPU providers are a later phase.
    std::string device = "cpu";
    /// ONNX Runtime intra-op threads; zero uses a bounded default.
    int threads = 0;
    /// Segment step as a fraction of the 11 s segment. 0.5 halves the number
    /// of inference passes versus the reference 0.25, for a small SDR loss.
    float step = 0.5f;
    /// Linear fade at the segment edges as a fraction of the segment.
    float fade = 0.1f;
};

/// Whole-track separation on top of an exported SCNet graph.
///
/// The host owns the STFT/iSTFT and the segment overlap-add; the ONNX graph
/// takes a complex spectrogram `[1, 4, 2049, 476]` (left re/im, right re/im)
/// and returns `[1, 16, 2049, 476]` (four sources, two channels, re/im).
class ScnetOnnxSeparator final : public StemSeparator {
  public:
    /// Creates the runtime and opens the model. Returns null and fills
    /// `pError` when the build has no ONNX Runtime, the model cannot be read,
    /// or the graph does not match the SCNet contract.
    static std::unique_ptr<ScnetOnnxSeparator> create(
            const ScnetOnnxConfig& config, std::string* pError);

    ~ScnetOnnxSeparator() override;

    ScnetOnnxSeparator(const ScnetOnnxSeparator&) = delete;
    ScnetOnnxSeparator& operator=(const ScnetOnnxSeparator&) = delete;

    int sampleRate() const override;
    int channels() const override;
    const std::vector<std::string>& sources() const override;
    std::string device() const override;
    std::string lastError() const override;

    bool separate(const float* pPlanarStereo,
            SINT frames,
            const StemOutputCallback& writeOutput,
            const StemProgressCallback& progress = StemProgressCallback(),
            const StemCancelCallback& cancel = StemCancelCallback()) override;

  private:
    class Impl;

    explicit ScnetOnnxSeparator(std::unique_ptr<Impl> pImpl);

    std::unique_ptr<Impl> m_pImpl;
};

/// True when this build was compiled with ONNX Runtime support.
inline constexpr bool isScnetOnnxAvailable() {
#ifdef __STEM_SEPARATION__
    return true;
#else
    return false;
#endif
}

} // namespace mixxx::stems
