#include "waveform/stemdetail/stemdetailreducer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

// Constants from the shipping binary (float32 values stored in __TEXT).
constexpr float kTwoPi150 = 942.4778442382812f; // 0x103556afc
constexpr float kTwoPi671 = 4214.888671875f;    // 0x103556b14
constexpr float kTwoPi3000 = 18849.556640625f;  // 0x103556b10
constexpr float kDamping = 2.0f;                // 0x103556b20
constexpr float kEnvTauLow = 0.01f;             // 0x103556b30
constexpr float kEnvTauMid = 0.1f;              // 0x103556b34
constexpr float kEnvTauHighAttack = 0.005f;     // 0x103556b38
constexpr float kEnvTauHighRelease = 0.035f;    // 0x103556b3c
constexpr float kEnvTauFast = 0.0025f;          // 0x103556b00
constexpr float kEnvTauSlow = 0.025f;           // 0x1034e2a18
constexpr float kHalf = 0.5f;                   // 0x103496b34
constexpr float kOne = 1.0f;                    // 0x103496b14
constexpr float kMinusOne = -1.0f;              // 0x103496b38
constexpr float kMinusHalf = -0.5f;             // 0x103496b3c
constexpr float kLowWeight = 0.6f;              // 0x1034b3614
constexpr float kHighWeight = 1.2f;             // 0x103556430

inline float clampUnit(float v) {
    if (v > kOne) {
        return kOne;
    }
    if (v < kMinusOne) {
        return kMinusOne;
    }
    return v;
}

// Round-to-nearest-even, matching cvtss2si under MXCSR = 0x9f80.
inline int roundNearest(float v) {
    return static_cast<int>(std::nearbyint(v));
}

const StemDetailEntry kZeroEntry{0.0f, 0.0f, 0.0f, 0.0f};

} // namespace

StemDetailReducer::StemDetailReducer(const StemDetailConfig& config)
        : m_config(config) {
    m_current.assign(static_cast<std::size_t>(m_config.height), kZeroEntry);
    m_previous.assign(static_cast<std::size_t>(m_config.height), kZeroEntry);
    m_previousTail.assign(static_cast<std::size_t>(m_config.height), kZeroEntry);
    m_output.assign(static_cast<std::size_t>(m_config.height), kZeroEntry);
    reset();
}

void StemDetailReducer::reset() {
    const float fs = static_cast<float>(m_config.sampleRate);
    m_a[0] = kTwoPi150 / fs;
    m_a[1] = kTwoPi671 / fs;
    m_a[2] = kTwoPi3000 / fs;
    m_a[3] = kTwoPi671 / fs;
    std::memset(m_b, 0, sizeof(m_b));
    std::memset(m_c, 0, sizeof(m_c));
    m_env[0] = m_env[1] = m_env[2] = 0.0f;
    m_attack[0] = std::exp(-kOne / (fs * kEnvTauLow));
    m_attack[1] = std::exp(-kOne / (fs * kEnvTauHighAttack));
    m_attack[2] = std::exp(-kOne / (fs * kEnvTauFast));
    m_release[0] = std::exp(-kOne / (fs * kEnvTauMid));
    m_release[1] = std::exp(-kOne / (fs * kEnvTauHighRelease));
    m_release[2] = std::exp(-kOne / (fs * kEnvTauSlow));
    m_halfHeight = static_cast<float>(m_config.height - 2) * kHalf;
    m_lastMapped = static_cast<float>(m_config.height) * kHalf;
    std::fill(m_current.begin(), m_current.end(), kZeroEntry);
    std::fill(m_previous.begin(), m_previous.end(), kZeroEntry);
    std::fill(m_previousTail.begin(), m_previousTail.end(), kZeroEntry);
    std::fill(m_output.begin(), m_output.end(), kZeroEntry);
}

StemDetailSplitterState StemDetailReducer::splitterState() const {
    StemDetailSplitterState state;
    std::memcpy(state.b, m_b, sizeof(m_b));
    std::memcpy(state.c, m_c, sizeof(m_c));
    std::memcpy(state.env, m_env, sizeof(m_env));
    return state;
}

void StemDetailReducer::setSplitterState(const StemDetailSplitterState& state) {
    std::memcpy(m_b, state.b, sizeof(m_b));
    std::memcpy(m_c, state.c, sizeof(m_c));
    std::memcpy(m_env, state.env, sizeof(m_env));
}

StemDetailEntry StemDetailReducer::stepSample(float x) {
    // Four Chamberlin SVF lanes (a = 2*pi*fc/fs, damping 2).
    float y[4];
    float bn[4];
    for (int k = 0; k < 4; ++k) {
        y[k] = x - (kDamping * m_b[k] + m_c[k]);
        bn[k] = m_a[k] * y[k] + m_b[k];
    }
    for (int k = 0; k < 4; ++k) {
        const float cn = m_a[k] * bn[k] + m_c[k];
        m_b[k] = bn[k];
        m_c[k] = cn;
    }

    // Envelope followers: low from c'[0], mid from b'[3], high from y[2].
    const float m0 = std::fabs(m_c[0]);
    const float g0 = (m0 > m_env[0]) ? m_attack[0] : m_release[0];
    m_env[0] = (kOne - g0) * m0 + m_env[0] * g0;

    const float m1 = std::fabs(bn[3]);
    const float g1 = (m1 > m_env[1]) ? m_attack[1] : m_release[1];
    m_env[1] = (kOne - g1) * m1 + m_env[1] * g1;

    const float m2 = std::fabs(y[2]);
    const float g2 = (m2 > m_env[2]) ? m_attack[2] : m_release[2];
    m_env[2] = (kOne - g2) * m2 + m_env[2] * g2;

    // Shipped-code quirk: lane 3's b state is replaced by lane 1's b'.
    m_b[3] = bn[1];

    StemDetailEntry entry;
    entry.coverage = kOne;
    entry.low = kLowWeight * m_env[0];
    entry.mid = m_env[1];
    entry.high = kHighWeight * m_env[2];
    return entry;
}

const std::vector<StemDetailEntry>& StemDetailReducer::processColumn(
        const float* left, const float* right) {
    const int height = m_config.height;
    const int length = m_config.samplesPerColumn;
    const float mag = m_config.magnification;
    const float inset = static_cast<float>(m_config.verticalInset);
    const float scale = static_cast<float>(height - 2) * kHalf - inset;
    const float invLength = kOne / static_cast<float>(length);

    std::fill(m_current.begin(), m_current.end(), kZeroEntry);
    std::fill(m_previous.begin(), m_previous.end(), kZeroEntry);

    for (int i = 0; i < length; ++i) {
        const float x = (left[i] + right[i]) * kHalf * mag;
        const StemDetailEntry comp = stepSample(x);

        // Row span between the previous and current mapped positions.
        const float v = clampUnit(x) * scale + m_halfHeight;
        const float loV = (m_lastMapped < v) ? m_lastMapped : v;
        const float hiV = (m_lastMapped < v) ? v : m_lastMapped;
        int lo = roundNearest(loV + kMinusHalf);
        int hi = roundNearest(hiV + kMinusHalf);
        if (lo < 0) {
            lo = 0;
        }
        if (lo > height - 1) {
            lo = height - 1;
        }
        if (hi > height - 1) {
            hi = height - 1;
        }
        if (hi < 0) {
            hi = 0;
        }

        const float t = static_cast<float>(i) * invLength;
        const float wCur = kOne - t;
        if (lo <= hi) {
            for (int r = lo; r <= hi; ++r) {
                m_current[r].coverage += comp.coverage * wCur;
                m_current[r].low += comp.low * wCur;
                m_current[r].mid += comp.mid * wCur;
                m_current[r].high += comp.high * wCur;
                m_previous[r].coverage += comp.coverage * t;
                m_previous[r].low += comp.low * t;
                m_previous[r].mid += comp.mid * t;
                m_previous[r].high += comp.high * t;
            }
        }
        m_lastMapped = v;
    }

    // Final Entry = current column + previous column's interpolation tail,
    // with every split scaled by 1/length.
    for (int r = 0; r < height; ++r) {
        m_output[r].coverage = (m_current[r].coverage + m_previousTail[r].coverage) * invLength;
        m_output[r].low = (m_current[r].low + m_previousTail[r].low) * invLength;
        m_output[r].mid = (m_current[r].mid + m_previousTail[r].mid) * invLength;
        m_output[r].high = (m_current[r].high + m_previousTail[r].high) * invLength;
    }
    m_previousTail = m_previous;
    return m_output;
}
