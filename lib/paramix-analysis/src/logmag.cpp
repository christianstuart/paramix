#include "logmag.h"

#include <cstdint>

#include "float32.h"

namespace paramix::analysis {

float log10p1(float value) {
    constexpr float c1 = 0.40526167f;
    constexpr float c2 = 0.10423073f;
    constexpr float c3 = 0.30103001f;

    const float t = f32(value + 1.0f);
    const std::uint32_t bits = floatBits(t);
    const std::uint32_t exponent = bits & 0x7f800000u;
    const float mantissa = bitsFloat((bits ^ exponent) | 0x3f800000u);
    const float y = f32(mantissa - 1.0f);
    const float p = f32(c1 - f32(y * c2));
    const float q = f32(p * y);
    const auto exponentValue = static_cast<int>((exponent >> 23) - 127);
    const float r = f32(f32(exponentValue) * c3);
    return f32(q + r);
}

float sumLanes4(const float* values, int count) {
    float lanes[4] = {values[0], values[1], values[2], values[3]};
    for (int i = 4; i < count; i += 4) {
        for (int j = 0; j < 4; ++j) {
            lanes[j] = f32(lanes[j] + values[i + j]);
        }
    }
    float sum = f32(0.0f + lanes[0]);
    sum = f32(sum + lanes[1]);
    sum = f32(sum + lanes[2]);
    sum = f32(sum + lanes[3]);
    return sum;
}

} // namespace paramix::analysis
