import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property int hotcueNumber
    required property string group
    property alias activate: hotcueActivateControl.value
    property alias clear: hotcueClearControl.value
    readonly property bool isSet: hotcueStatusControl.value != 0
    /// A hotcue that carries a stored loop (end position set) is drawn green,
    /// everything else that is set uses the accent. The per-cue colour stored
    /// on the track is still written by the cue popup, it just no longer drives
    /// the pad face.
    readonly property bool isLoop: hotcueTypeControl.value === 4
            || hotcueEndPositionControl.value > 0
    readonly property color color: {
        if (!root.isSet)
            return Theme.deckActiveColor;
        if (root.isLoop)
            return Theme.vuGreen;
        return Theme.accent;
    }

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
