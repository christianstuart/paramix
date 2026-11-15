import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Rectangle {
    id: root

    property alias knob: knob
    property string statusGroup: root.knob.group
    required property string statusKey

    border.color: Theme.border
    border.width: 1
    color: Theme.surface1
    height: 44
    opacity: statusControl.value ? 0.45 : 1
    radius: Theme.radiusSm
    width: 44

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.top: parent.top
        anchors.topMargin: 1
        color: Theme.topHighlight
        height: 1
    }

    TapHandler {
        onDoubleTapped: {
            statusControl.value = !statusControl.value;
        }
    }
    Skin.ControlKnob {
        id: knob

        anchors.centerIn: root
        height: 36
        width: 36
    }
    Mixxx.ControlProxy {
        id: statusControl

        group: root.statusGroup
        key: root.statusKey
    }
}
