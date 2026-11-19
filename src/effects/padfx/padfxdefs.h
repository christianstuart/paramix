#pragma once

#include <QString>
#include <array>

namespace mixxx {
namespace padfx {

constexpr int kNumBanks = 4;
constexpr int kNumPadsPerBank = 4;
constexpr int kNumStems = 4;

enum class PadEffect : int {
    Off = 0,
    VinylBrake = 1,
    EchoOut = 2,
};

enum class PadMode : int {
    Momentary = 0,
    Toggle = 1,
    OneShot = 2,
};

struct PadConfig {
    int effect = static_cast<int>(PadEffect::Off);
    int targetMask = 1;
    int mode = static_cast<int>(PadMode::Toggle);
    // Normalized 0..1 values. The meaning depends on the effect:
    //   Vinyl Brake: [0] time, [1] curve
    //   Echo Out:    [0] beats, [1] feedback, [2] tail beats
    std::array<double, 4> params{0.5, 0.5, 0.47, 0.0};
};

struct BankConfig {
    std::array<PadConfig, kNumPadsPerBank> pads{};
};

struct DeckConfig {
    int bank = 1;
    std::array<BankConfig, kNumBanks> banks{};
};

inline QString effectId(PadEffect effect) {
    switch (effect) {
    case PadEffect::VinylBrake:
        return QStringLiteral("org.paramix.effects.vinylbrake");
    case PadEffect::EchoOut:
        return QStringLiteral("org.paramix.effects.echoout");
    case PadEffect::Off:
    default:
        return QString();
    }
}

inline QString effectName(PadEffect effect) {
    switch (effect) {
    case PadEffect::VinylBrake:
        return QStringLiteral("Vinyl Brake");
    case PadEffect::EchoOut:
        return QStringLiteral("Echo Out");
    case PadEffect::Off:
    default:
        return QStringLiteral("Off");
    }
}

} // namespace padfx
} // namespace mixxx
