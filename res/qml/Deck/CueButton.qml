import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

/// Cue button.
///
/// Pioneer-style: neutral raised face at rest, amber backlight while held or
/// previewing, and a small amber lamp that mirrors Mixxx's classic
/// `cue_indicator`. Stays visually distinct from the green play key by colour,
/// shape and label.
Skin.ControlButton {
    id: root

    property bool minimized: false

    Accessible.description: qsTr("Left click previews from the cue point, right click jumps to the cue point and stops, Shift+click moves the cue point here")
    Accessible.name: qsTr("Cue")
    activeColor: "#1A1005"
    key: "cue_default"
    text: qsTr("CUE")

    background: Rectangle {
        border.color: root.highlight ? Qt.lighter(Theme.warning, 1.1) : (root.hovered ? Theme.borderStrong : Theme.border)
        border.width: 1
        color: root.highlight ? Theme.warning : (root.pressed ? Theme.surface3 : (root.hovered ? Theme.surfaceHover : Theme.surface2))
        radius: Theme.radiusSm

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

        // Cue-stored lamp: `cue_indicator` is Mixxx's classic cue light, so it
        // blinks while a CUE press would move the cue point instead of leaving
        // a static lamp on the face.
        Rectangle {
            anchors.margins: 3
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.warning
            height: 4
            radius: 2
            visible: cueIndicator.value != 0
            width: 4
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

    Mixxx.ControlProxy {
        id: cueIndicator

        group: root.group
        key: "cue_indicator"
    }

    // The plain left click above keeps the momentary cue_default preview, the
    // classic right click is cue_gotoandstop, and Shift+click stores the cue.
    Skin.ControlProxyButtonBehavior {
        id: cueGotoBehavior

        group: root.group
        handlePointerInput: false
        key: "cue_gotoandstop"
    }
    Skin.ControlProxyButtonBehavior {
        id: cueSetBehavior

        group: root.group
        handlePointerInput: false
        key: "cue_set"
    }
    MouseArea {
        acceptedButtons: Qt.RightButton
        anchors.fill: parent

        onCanceled: {
            cueGotoBehavior.releasePrimary();
        }
        onPressed: {
            cueGotoBehavior.pressPrimary();
        }
        onReleased: {
            cueGotoBehavior.releasePrimary();
        }
    }
    // Shift is the keyboard gesture for cue_set (Shift+D in en_US.kbd.cfg), so
    // it mirrors that here. Without Shift the handler ignores the press and the
    // button below still gets the momentary cue_default preview.
    Item {
        anchors.fill: parent

        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.ShiftModifier

            onPressedChanged: {
                if (pressed) {
                    cueSetBehavior.pressPrimary();
                } else {
                    cueSetBehavior.releasePrimary();
                }
            }
        }
    }
}
