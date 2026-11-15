import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Skin.ControlProxyButtonBehavior {
    id: root

    anchors.fill: parent

    required property int hotcueNumber
    property color fallbackColor: "white"
    readonly property bool isSet: statusProxy.value > 0
    /// Set cues take the accent, cues that carry a stored loop are green.
    readonly property bool isLoop: typeProxy.value === 4 || endPositionProxy.value > 0
    readonly property color hotcueColor: !root.isSet
            ? root.fallbackColor
            : root.isLoop ? Theme.vuGreen : Theme.accent

    signal popupRequested(real mouseX, real mouseY)
    signal cleared()

    key: "hotcue_" + root.hotcueNumber + "_activate"
    displayKey: "hotcue_" + root.hotcueNumber + "_status"

    Mixxx.ControlProxy {
        id: typeProxy

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_type"
    }

    Mixxx.ControlProxy {
        id: endPositionProxy

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_endposition"
    }

    Mixxx.ControlProxy {
        id: statusProxy

        group: root.group
        key: "hotcue_" + root.hotcueNumber + "_status"

        onValueChanged: function(newValue) {
            if (newValue === 0) {
                root.cleared();
            }
        }
    }

    onSecondaryPressed: function(displayValue, mouseX, mouseY) {
        if (displayValue > 0) {
            root.popupRequested(mouseX, mouseY);
        }
    }
}
