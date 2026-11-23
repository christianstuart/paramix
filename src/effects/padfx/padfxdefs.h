#pragma once

#include <QString>
#include <array>

namespace mixxx {
namespace padfx {

constexpr int kNumBanks = 2;
constexpr int kNumPadsPerBank = 8;
constexpr int kNumStems = 4;
constexpr int kMaxPadParams = 8;
constexpr int kDeckTarget = 0;
constexpr int kModeMomentary = 0;
constexpr int kModeToggle = 1;

/// Pad FX are controls over the existing effect chains, not a separate effect
/// catalogue. These two effects stay pad-only because they shape the stem
/// gesture itself; every other effect comes from the normal effect list.
inline const QString kVinylBrakeId = QStringLiteral("org.paramix.effects.vinylbrake");
inline const QString kEchoOutId = QStringLiteral("org.paramix.effects.echoout");

struct PadConfig {
    /// Unique effect manifest ID. Empty means Off.
    QString effectId;
    /// 0 is the whole deck; 1..15 is a stem mask.
    int targetMask = kDeckTarget;
    /// kModeMomentary (hold) or kModeToggle (tap on, tap off).
    int mode = kModeMomentary;
    /// Normalized 0..1 values, ordered by the effect's parameter list. The
    /// pad-only effects keep their original UI order:
    ///   Vinyl Brake: time, curve, wind-up
    ///   Echo Out:    time, feedback, tail, reverb, reverb time
    std::array<double, kMaxPadParams> params{0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
};

struct BankConfig {
    std::array<PadConfig, kNumPadsPerBank> pads{};
};

struct PresetConfig {
    std::array<BankConfig, kNumBanks> banks{};
};

/// Old per-deck configuration used one-byte effect indices. This maps the
/// legacy index to the stable manifest ID used by the shared preset.
inline QString legacyEffectId(int effect) {
    switch (effect) {
    case 1:
        return kVinylBrakeId;
    case 2:
        return kEchoOutId;
    default:
        return QString();
    }
}

/// A manifest can be stored either by its id or by its unique id (id plus
/// backend). The pad-only checks always need the id half.
inline QString baseEffectId(const QString& effectId) {
    const int delimiter = effectId.lastIndexOf(QChar(' '));
    return delimiter < 0 ? effectId : effectId.left(delimiter);
}

inline bool isPadFxOnly(const QString& effectId) {
    const QString base = baseEffectId(effectId);
    return base == kVinylBrakeId || base == kEchoOutId;
}

/// Release effects (Vinyl Brake, Echo Out) latch by default; everything else
/// is held.
inline int defaultPadMode(const QString& effectId) {
    return isPadFxOnly(effectId) ? kModeToggle : kModeMomentary;
}

inline std::array<double, kMaxPadParams> defaultPadParams(const QString& effectId) {
    const QString base = baseEffectId(effectId);
    if (base == kEchoOutId) {
        // Echo time, feedback, tail, reverb, reverb time.
        return {0.5, 0.5, 0.47, 0.25, 0.5, 0.5, 0.5, 0.5};
    }
    if (base == kVinylBrakeId) {
        // Wind-down, curve, wind-up.
        return {0.5, 0.5, 0.47, 0.5, 0.5, 0.5, 0.5, 0.5};
    }
    return {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
}

} // namespace padfx
} // namespace mixxx
