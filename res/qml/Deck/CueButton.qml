import ".." as Skin
import QtQuick 2.12
import "../Theme"

/// Cue button.
///
/// Pioneer-style: neutral raised face at rest, amber backlight while held or
/// previewing. Stays visually distinct from the green play key by colour,
// shape and label.
Skin.ControlButton {
    id: root

    property bool minimized: false

    activeColor: "#1A1005"
    key: "cue_default"
    text: qsTr("CUE")

    Accessible.description: qsTr("Left click previews from the cue point, right click moves the cue point here")
    Accessible.name: qsTr("Cue")

    background: Rectangle {
        border.color: root.highlight ? Qt.lighter(Theme.warning, 1.1) : (root.hovered ? Theme.borderStrong : Theme.border)
        border.width: 1
        color: root.highlight ? Theme.warning : (root.pressed ? Theme.surface3 : (root.hovered ? Theme.surfaceHover : Theme.surface2))
        radius: Theme.radiusSm

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 1
            color: root.highlight ? Qt.rgba(255, 255, 255, 0.3) : Theme.topHighlight
            height: 1
        }

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
    contentItem: Text {
        color: root.highlight ? "#1A1005" : (root.hovered ? Theme.textPrimary : Theme.textSecondary)
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: root.minimized ? Theme.fontSizeXs : Theme.fontSizeSm
        font.weight: Theme.fontWeightDemiBold
        horizontalAlignment: Text.AlignHCenter
        text: root.text
        verticalAlignment: Text.AlignVCenter
    }

    // The left click above keeps the momentary cue_default preview; the
    // right click drops the cue point at the current play position.
    Skin.ControlProxyButtonBehavior {
        id: cueSetBehavior

        group: root.group
        key: "cue_set"
        handlePointerInput: false
    }
    MouseArea {
        acceptedButtons: Qt.RightButton
        anchors.fill: parent

        onCanceled: {
            cueSetBehavior.releasePrimary();
        }
        onPressed: {
            cueSetBehavior.pressPrimary();
        }
        onReleased: {
            cueSetBehavior.releasePrimary();
        }
    }
}
