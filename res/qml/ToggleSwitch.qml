import QtQuick 2.15
import "Theme"

/// Compact, token-driven switch used by the Paramix customizer.
Item {
    id: root

    property bool checked: false

    signal toggled

    implicitHeight: 22
    implicitWidth: 38
    opacity: enabled ? 1 : 0.5

    Accessible.checkable: true
    Accessible.checked: root.checked
    Accessible.role: Accessible.CheckBox

    Rectangle {
        anchors.fill: parent
        border.color: root.checked ? Theme.accentHover : Theme.borderStrong
        border.width: 1
        color: root.checked ? Theme.accent : Theme.surface3
        radius: height / 2

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
    Rectangle {
        id: knob

        height: 16
        radius: height / 2
        width: height
        x: root.checked ? root.width - width - 3 : 3
        y: (root.height - height) / 2
        color: root.checked ? Theme.textOnAccent : Theme.textSecondary

        Behavior on x {
            NumberAnimation {
                duration: Theme.motionFast
                easing.type: Easing.OutCubic
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
    TapHandler {
        onTapped: root.toggled()
    }
}
