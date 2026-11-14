import ".." as Skin
import "../Theme"

/// Cue button. It lights up in the warning token while held so it stays
/// distinguishable from the accent-coloured play button.
Skin.ControlButton {
    id: root

    property bool minimized: false

    activeColor: Theme.onAccent
    key: "cue_default"
    text: qsTr("Cue")

    Accessible.name: qsTr("Cue")

    background: Rectangle {
        border.color: root.highlight ? Theme.warning : (root.hovered ? Theme.borderStrong : Theme.border)
        border.width: 1
        color: root.highlight ? Theme.warning : (root.pressed ? Theme.surface3 : (root.hovered ? Theme.surface2 : Theme.surface1))
        radius: Theme.radiusLg

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
}
