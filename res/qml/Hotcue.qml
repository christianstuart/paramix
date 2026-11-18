import "Theme"
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Item {
    id: root

    property alias activate: hotcueActivateControl.value
    property alias clear: hotcueClearControl.value
    readonly property color color: {
        if (!root.isSet)
            return Theme.deckActiveColor;
        if (root.isLoop)
            return Theme.vuGreen;
        if (root.useStoredColor && root.storedColor.a > 0)
            return root.storedColor;
        return Theme.accent;
    }
    required property string group
    required property int hotcueNumber
    /// A hotcue that carries a stored loop (end position set) is drawn green,
    /// everything else that is set uses the stored colour when
    /// `useStoredColor` is on, and the accent otherwise.
    readonly property bool isLoop: hotcueTypeControl.value === 4 || hotcueEndPositionControl.value > 0
    readonly property bool isSet: hotcueStatusControl.value != 0
    readonly property color storedColor: {
        const rgb = Math.round(hotcueColorControl.value);
        if (rgb <= 0)
            return "transparent";
        return Qt.rgba(((rgb >> 16) & 0xff) / 255, ((rgb >> 8) & 0xff) / 255, (rgb & 0xff) / 255);
    }
    /// Read the cue colour stored on the track (`hotcue_<n>_color`, an RGB
    /// code) for the pad face. Off by default so the sampler keeps its
    /// accent/green look.
    property bool useStoredColor: false

    function setColor(newColor) {
        hotcueColorControl.value = (parseInt(newColor.r * 255) << 16) | (parseInt(newColor.g * 255) << 8) | parseInt(newColor.b * 255);
    }

    Mixxx.ControlProxy {
        id: hotcueTypeControl

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_type"
    }
    Mixxx.ControlProxy {
        id: hotcueEndPositionControl

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_endposition"
    }
    Mixxx.ControlProxy {
        id: hotcueColorControl

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_color"
    }
    Mixxx.ControlProxy {
        id: hotcueActivateControl

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_activate"
    }
    Mixxx.ControlProxy {
        id: hotcueStatusControl

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_status"
    }
    Mixxx.ControlProxy {
        id: hotcueClearControl

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_clear"
    }
}
