import Mixxx 1.0 as Mixxx
import QtQuick
import "../LateNightTheme"

/// Skin-facing VU meter. The meter itself is a C++ scene-graph item
/// (`Mixxx.VuMeter`): one shared 30 Hz ticker decays the peak and the bars are
/// drawn as textured quads. This wrapper only maps the LateNight theme assets
/// and the legacy property names onto it, so the call sites stay unchanged.
Mixxx.VuMeter {
    id: root

    property int backgroundVariant: 0
    readonly property string variantName: backgroundVariant === 0 ? "dark" : "light"
    property bool micAux: false

    clip: true
    clipActiveSource: root.micAux ? LateNightTheme.assetMicAuxVuClippingActive : LateNightTheme.lateNightAsset("style", "vu_deck_clipping_active.png")
    clipBackgroundSource: root.micAux ? LateNightTheme.assetMicAuxVuClippingBackground : LateNightTheme.mixerVuClipBackground(root.variantName)
    clipHeight: root.micAux ? 9 : 11
    implicitHeight: root.micAux ? 54 : 96
    implicitWidth: 8
    levelActiveSource: root.micAux ? LateNightTheme.assetMicAuxVuLevelActive : LateNightTheme.lateNightAsset("style", "vu_deck_level_active.png")
    levelBackgroundSource: root.micAux ? LateNightTheme.assetMicAuxVuLevelBackground : LateNightTheme.mixerVuLevelBackground(root.variantName)
    levelHeight: root.micAux ? 41 : 81
    peakFallTime: root.group === "[Main]" ? 50 : 10
    peakHoldSize: root.group === "[Main]" ? 5 : 4
}
