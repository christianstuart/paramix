#include "fft.h"

#include <cmath>
#include <utility>

#include "float32.h"
#include "window.h"

namespace paramix::analysis {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

void realFft512(const float* input, double* re, double* im) {
    constexpr int n = kWindowLength;
    for (int i = 0; i < n; ++i) {
        re[i] = static_cast<double>(input[i]);
        im[i] = 0.0;
    }

    // Iterative radix-2 decimation in time, identical butterfly order to the
    // reference implementation.
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * kPi / static_cast<double>(len);
        for (int i = 0; i < n; i += len) {
            for (int k = 0; k < len / 2; ++k) {
                const double wr = std::cos(ang * static_cast<double>(k));
                const double wi = std::sin(ang * static_cast<double>(k));
                const double ur = re[i + k];
                const double ui = im[i + k];
                const double vr = re[i + k + len / 2] * wr -
                        im[i + k + len / 2] * wi;
                const double vi = re[i + k + len / 2] * wi +
                        im[i + k + len / 2] * wr;
                re[i + k] = ur + vr;
                im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr;
                im[i + k + len / 2] = ui - vi;
            }
        }
    }
}

void magnitudeSpectrum(const double* re, const double* im, float* magnitude) {
    for (int k = 0; k < kSpectrumBins; ++k) {
        const float rr = f32(re[k] * re[k]);
        const float ii = f32(im[k] * im[k]);
        const float sum = f32(rr + ii);
        const float root = f32(std::sqrt(sum));
        magnitude[k] = f32(f32(2.0f) * root);
    }
}

} // namespace paramix::analysis
