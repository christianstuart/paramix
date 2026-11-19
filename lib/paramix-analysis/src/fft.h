#pragma once

namespace paramix::analysis {

/// Forward 512-point real FFT, X[0..255] (the upper half is not needed by the
/// detector). Twiddles and intermediate storage are double precision, matching
/// the reference model; magnitudes are rounded back to float32 by the caller.
void realFft512(const float* input, double* re, double* im);

/// lin[k] = f32(f32(2) * f32(sqrt(f32(f32(re*re) + f32(im*im)))))
void magnitudeSpectrum(const double* re, const double* im, float* magnitude);

} // namespace paramix::analysis
