#pragma once

#include <array>

namespace paramix::analysis {

inline constexpr int kAnalysisRate = 22050;
inline constexpr int kWindowLength = 512;
inline constexpr int kHopSize = 32;
inline constexpr int kSpectrumBins = 256;
inline constexpr int kFluxBins = 80;
inline constexpr int kEnvelopeBatch = 43;

using WindowTable = std::array<float, kWindowLength>;

/// Half-amplitude Hann window, exactly as built by the reference model:
/// w[i] = f32(f32(f32(0.5) - f32(f32(0.5) * cos(f32(i * K)))) * f32(0.5)),
/// with K = f32(f32(6.2831855) / f32(511)).
WindowTable makeWindowTable();

} // namespace paramix::analysis
