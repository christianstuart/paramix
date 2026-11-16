import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Rectangle {
    id: root

    required property string group
    property alias knob: knob

    color: Theme.knobBackgroundColor
    height: 30
    opacity: statusControl.value ? 1 : 0.4
    radius: 5
    width: 30

    Skin.ControlKnob {
        id: knob

        anchors.horizontalCenter: root.horizontalCenter
        anchors.top: root.top
        group: root.group
        height: 25
        key: "super1"
        width: 25
    }
    // Right-click toggles the effect. Double-click belongs to the knob (reset),
    // and nesting it here made one gesture do both.
    MouseArea {
        acceptedButtons: Qt.RightButton
        anchors.fill: parent

        onClicked: statusControl.value = statusControl.value ? 0 : 1
    }
    Mixxx.ControlProxy {
        id: statusControl

        group: root.group
        key: "enabled"
    }
}
