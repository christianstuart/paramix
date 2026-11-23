#pragma once

namespace paramix::analysis {

/// A Traktor BPM detection window. The analysis dialog offers a fixed set of
/// minimum/maximum pairs; the engine's search centre and half width are derived
/// from the selected pair.
struct BpmRange {
    int minBpm;
    int maxBpm;
    const char* label;
    /// Traktor's "Automatic" entry. It is a real preset, but the window it
    /// stands for cannot be inferred from a stored BPM, so the existing-BPM
    /// search ignores it.
    bool automatic;
};

/// The Traktor 4.1.1 BPM range presets, in dialog order. Preset 4 (88-175) is
/// the installation default. The table was read from the native preset array
/// (0x140f48ed0, ten 0x28-byte entries).
inline constexpr BpmRange kBpmRangePresets[] = {
        {95, 115, "95 - 115", false},
        {58, 115, "58 - 115", false},
        {68, 135, "68 - 135", false},
        {78, 155, "78 - 155", false},
        {88, 175, "88 - 175", false},
        {98, 195, "98 - 195", false},
        {108, 215, "108 - 215", false},
        {118, 235, "118 - 235", false},
        {128, 255, "128 - 255", false},
        {110, 220, "Automatic", true},
};

inline constexpr int kBpmRangePresetCount =
        static_cast<int>(sizeof(kBpmRangePresets) / sizeof(kBpmRangePresets[0]));

inline constexpr int kDefaultBpmRangePreset = 4;

inline constexpr BpmRange defaultBpmRange() {
    return kBpmRangePresets[kDefaultBpmRangePreset];
}

inline constexpr BpmRange bpmRangeForPreset(int preset) {
    return (preset >= 0 && preset < kBpmRangePresetCount)
            ? kBpmRangePresets[preset]
            : kBpmRangePresets[kDefaultBpmRangePreset];
}

} // namespace paramix::analysis
