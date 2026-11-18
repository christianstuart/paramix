#pragma once

namespace paramix::analysis {

/// Modified Bessel function I0 (Abramowitz & Stegun / Numerical Recipes),
/// evaluated in double precision exactly as in the reference model.
double bessi0(double x);

/// Kaiser-windowed sinc on the tap grid:
/// kernel(t) = sinc(pi*t) * I0(8*sqrt(1-(t/8)^2)) / I0(8), |t| < 8.
double kernel(double t);

/// The native mode-6 16-tap table (44.1 kHz -> 22.05 kHz), stored as the
/// measured float32 bit patterns.
const float* mode6Taps();

} // namespace paramix::analysis
