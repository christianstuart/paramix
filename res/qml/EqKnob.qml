import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

/// One compact EQ knob: no tile, just the knob with 3 px of padding.
Rectangle {
    id: root

    property alias knob: knob
    property string statusGroup: root.knob.group
    required property string statusKey

    border.width: 0
    color: "transparent"
    height: 32
    opacity: statusControl.value ? 0.45 : 1
    radius: 0
    width: 32

    // Right-click kills the band. It used to be double-click, but the knob
    // underneath also handles double-click (reset), so one gesture both reset
    // and killed the band.
    MouseArea {
        acceptedButtons: Qt.RightButton
        anchors.fill: parent

        onClicked: statusControl.value = statusControl.value ? 0 : 1
    }
    Skin.ControlKnob {
        id: knob

        anchors.centerIn: root
        height: 27
        width: 27
    }
    Mixxx.ControlProxy {
        id: statusControl

        group: root.statusGroup
        key: root.statusKey
    }
}
