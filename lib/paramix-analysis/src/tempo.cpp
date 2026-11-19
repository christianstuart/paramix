#include "tempo.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "float32.h"

namespace paramix::analysis {

namespace {

constexpr float kEpsilon = 1.1920928955078125e-7f;

/// The native pairwise SIMD sum: two four-lane accumulators over blocks of
/// eight, a horizontal add, then the sequential tail.
float sumF(const float* values, int count) {
    const int blocks = count & ~7;
    float a[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float b[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int k = 0; k < blocks; k += 8) {
        for (int l = 0; l < 4; ++l) {
            a[l] = f32(a[l] + values[k + l]);
            b[l] = f32(b[l] + values[k + 4 + l]);
        }
    }
    const float c0 = f32(a[0] + b[0]);
    const float c1 = f32(a[1] + b[1]);
    const float c2 = f32(a[2] + b[2]);
    const float c3 = f32(a[3] + b[3]);
    float total = f32(f32(c0 + c2) + f32(c1 + c3));
    for (int k = blocks; k < count; ++k) {
        total = f32(total + values[k]);
    }
    return total;
}

} // namespace

float bpmSearch(const std::vector<double>& positions,
        float centre,
        float halfWidth,
        int steps) {
    const int n = static_cast<int>(positions.size());
    if (n <= 0 || steps <= 0) {
        return 0.0f;
    }

    const float span = f32(centre - halfWidth);
    const float stepSize = steps == 1
            ? f32(f32(centre + halfWidth) - span)
            : f32(f32(f32(centre + halfWidth) - span) / f32(steps - 1));

    float bestScore = 0.0f;
    float bestBpm = 0.0f;
    std::vector<float> phases(static_cast<std::size_t>(n));
    std::vector<float> buffer(static_cast<std::size_t>(n));

    for (int i = 0; i < steps; ++i) {
        const float bpm = f32(f32(f32(i) * stepSize) + span);
        if (!(bpm > 0.0f)) {
            continue;
        }
        const float period = f32(f32(60.0f) / bpm);
        const float inverse = f32(f32(1.0f) / period);
        for (int k = 0; k < n; ++k) {
            const float wrapped = f32(
                    std::fmod(positions[static_cast<std::size_t>(k)],
                            static_cast<double>(period)));
            phases[static_cast<std::size_t>(k)] = f32(wrapped * inverse);
        }
        buffer = phases;
        std::sort(buffer.begin(), buffer.end());

        float denominator;
        float reciprocal;
        if (n == 1) {
            denominator = f32(1.0f);
            reciprocal = f32(0.0f);
        } else {
            denominator = f32(n - 1);
            reciprocal = f32(f32(1.0f) / denominator);
        }
        for (int k = 0; k < n; ++k) {
            buffer[static_cast<std::size_t>(k)] = f32(
                    buffer[static_cast<std::size_t>(k)] -
                    f32(f32(k) * reciprocal));
        }

        const float mean = f32(
                sumF(buffer.data(), n) / f32(n));
        float divisor = denominator;
        if (divisor <= kEpsilon) {
            divisor = kEpsilon;
        }

        float accumulate0[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float accumulate1[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        const int blocks = n & ~7;
        for (int k = 0; k < blocks; k += 8) {
            for (int j = 0; j < 4; ++j) {
                const float t0 = f32(buffer[static_cast<std::size_t>(k + j)] -
                        mean);
                accumulate0[j] = f32(accumulate0[j] + f32(t0 * t0));
                const float t1 = f32(
                        buffer[static_cast<std::size_t>(k + 4 + j)] - mean);
                accumulate1[j] = f32(accumulate1[j] + f32(t1 * t1));
            }
        }
        float c[4];
        for (int j = 0; j < 4; ++j) {
            c[j] = f32(accumulate0[j] + accumulate1[j]);
        }
        float score = f32(f32(c[0] + c[2]) + f32(c[1] + c[3]));
        for (int k = blocks; k < n; ++k) {
            const float t = f32(buffer[static_cast<std::size_t>(k)] - mean);
            score = f32(score + f32(t * t));
        }
        score = f32(score / divisor);
        if (score > bestScore) {
            bestScore = score;
            bestBpm = bpm;
        }
    }

    return bestBpm;
}

TempoResult detectBpm(const std::vector<double>& positions) {
    TempoResult result;
    constexpr float kCentre = 131.5f;
    constexpr float kHalfWidth = 43.5f;
    result.coarse = bpmSearch(positions, kCentre, kHalfWidth, 501);
    if (!(result.coarse > 0.0f)) {
        return result;
    }
    result.fine = bpmSearch(positions,
            result.coarse,
            f32(kHalfWidth * f32(0.05f)),
            101);
    if (!(result.fine > 0.0f)) {
        return result;
    }
    result.bpm = bpmSearch(positions,
            result.fine,
            f32(kHalfWidth * f32(0.002f)),
            101);
    return result;
}

} // namespace paramix::analysis
