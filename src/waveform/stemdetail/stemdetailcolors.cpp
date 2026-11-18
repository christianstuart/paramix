#include "waveform/stemdetail/stemdetailcolors.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr float kOne = 1.0f;
constexpr float kEps = 0.01f; // 0x103496b20

inline float channel(QRgb color, int index) {
    switch (index) {
    case 0:
        return qRed(color) / 255.0f;
    case 1:
        return qGreen(color) / 255.0f;
    default:
        return qBlue(color) / 255.0f;
    }
}

inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// The shipping code rounds with cvtsd2si (round-half-to-even) and then clamps:
//   cmpl $0xff; cmovge 255     (>= 255 -> 255)
//   testl;      cmovle 0       (<= 0   -> 0)
inline uint8_t toByte(float v) {
    const int i = static_cast<int>(std::nearbyint(v * 255.0f));
    if (i > 254) {
        return 255;
    }
    if (i < 1) {
        return 0;
    }
    return static_cast<uint8_t>(i);
}

} // namespace

float stemDetailLut(int level) {
    // pow(n/255, 1/3.5) is evaluated once; the renderer asks for it tens of
    // thousands of times per frame.
    static const std::array<float, 256> table = [] {
        std::array<float, 256> t{};
        t[0] = 0.0f;
        for (int n = 1; n <= 255; ++n) {
            t[static_cast<std::size_t>(n)] =
                    std::pow(static_cast<float>(n) / 255.0f, 1.0f / 3.5f);
        }
        return t;
    }();
    if (level <= 0) {
        return 0.0f;
    }
    if (level >= 255) {
        return 1.0f;
    }
    return table[static_cast<std::size_t>(level)];
}

float stemDetailBrightness(float coverage, float gain) {
    const float lv = coverage * gain * 255.0f;
    const int level = static_cast<int>(std::nearbyint(lv - 0.5f));
    if (level < 0) {
        return 0.0f;
    }
    if (level >= 255) {
        return 1.0f;
    }
    const float frac = lv - static_cast<float>(level);
    return (kOne - frac) * stemDetailLut(level) + frac * stemDetailLut(level + 1);
}

QRgb stemDetailEntryColor(const StemDetailEntry& entry,
        const StemDetailColorParams& params) {
    // updateWaveformCachePicture @ 0x10087b774..0x10087b853
    const float s = kEps + entry.low + entry.mid + entry.high;
    float w1 = 0.0f;
    float w2 = 0.0f;
    float w3 = 0.0f;
    if (s > 0.0f) {
        w1 = entry.low * params.gain / s;
        w2 = entry.mid * params.gain / s;
        w3 = (entry.high * params.gain / s) * params.highScale;
    }
    const float alpha = clamp01(stemDetailBrightness(entry.coverage, params.gain));

    // createColorFunctionByMode::$_0 @ 0x100adcdf0 (colour mode 3, identity):
    // R = low, G = mid, B = high after normalisation.
    const float rgb[3] = {w1, w2, w3};

    // Caller blend @ 0x10087b875: c*alpha*scalar + base*(1-alpha), scalar = 1.
    const float baseAlpha = qAlpha(params.base) / 255.0f;
    const float base[3] = {channel(params.base, 0),
            channel(params.base, 1),
            channel(params.base, 2)};
    return qRgb(toByte(rgb[0] * alpha * baseAlpha + base[0] * (1.0f - alpha)),
            toByte(rgb[1] * alpha * baseAlpha + base[1] * (1.0f - alpha)),
            toByte(rgb[2] * alpha * baseAlpha + base[2] * (1.0f - alpha)));
}
