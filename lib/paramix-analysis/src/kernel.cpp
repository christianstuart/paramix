#include "kernel.h"

#include <cmath>

#include "float32.h"

namespace paramix::analysis {

double bessi0(double x) {
    const double ax = std::fabs(x);
    if (ax < 3.75) {
        const double y = (x / 3.75) * (x / 3.75);
        return 1.0 + y * (3.5156229 +
                y * (3.0899424 +
                        y * (1.2067492 +
                                y * (0.2659732 +
                                        y * (0.0360768 + y * 0.0045813)))));
    }
    const double y = 3.75 / ax;
    return (std::exp(ax) / std::sqrt(ax)) *
            (0.39894228 +
                    y * (0.01328592 +
                            y * (0.00225319 +
                                    y * (-0.00157565 +
                                            y * (0.00916281 +
                                                    y * (-0.02057706 +
                                                            y * (0.02635537 +
                                                                    y * (-0.01647633 +
                                                                            y * 0.00392377))))))));
}

double kernel(double t) {
    if (t == 0.0) {
        return 1.0;
    }
    const double x = 3.14159265358979323846 * t;
    const double u = t / 8.0;
    if (std::fabs(u) >= 1.0) {
        return 0.0;
    }
    const double window = bessi0(8.0 * std::sqrt(1.0 - u * u)) / bessi0(8.0);
    return window * (std::sin(x) / x);
}

const float* mode6Taps() {
    static const float taps[16] = {
            bitsFloat(0xb9d59b5fu),
            bitsFloat(0x3b179cc5u),
            bitsFloat(0xbbfab429u),
            bitsFloat(0x3ca00b78u),
            bitsFloat(0xbd2fc599u),
            bitsFloat(0x3db3651fu),
            bitsFloat(0xbe3e500du),
            bitsFloat(0x3f209bb8u),
            bitsFloat(0x3f209bb8u),
            bitsFloat(0xbe3e500du),
            bitsFloat(0x3db3651fu),
            bitsFloat(0xbd2fc599u),
            bitsFloat(0x3ca00b78u),
            bitsFloat(0xbbfab429u),
            bitsFloat(0x3b179cc5u),
            bitsFloat(0xb9d59b5fu)};
    return taps;
}

} // namespace paramix::analysis
