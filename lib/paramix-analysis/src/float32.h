#pragma once

#include <cmath>
#include <cstdint>

namespace paramix::analysis {

/// Rounds to IEEE-754 binary32. The reference implementation was written in
/// JavaScript with Math.fround after every arithmetic step; keeping the same
/// rounding here makes the port directly comparable to it.
constexpr float f32(float value) {
    return value;
}

constexpr float f32(double value) {
    return static_cast<float>(value);
}

constexpr float f32(int value) {
    return static_cast<float>(value);
}

constexpr float f32(long value) {
    return static_cast<float>(value);
}

constexpr float f32(long long value) {
    return static_cast<float>(value);
}

/// Bit-exact float32 bit pattern helpers.
inline std::uint32_t floatBits(float value) {
    union {
        float f;
        std::uint32_t u;
    } v{value};
    return v.u;
}

inline float bitsFloat(std::uint32_t value) {
    union {
        float f;
        std::uint32_t u;
    } v{};
    v.u = value;
    return v.f;
}

} // namespace paramix::analysis
