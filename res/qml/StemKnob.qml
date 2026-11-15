import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

/// Compact stem control card: colour key, mute toggle, volume knob,
/// QuickFX enable toggle, super knob and chain preset selector.
///
/// The stem colour key doubles as the mute button: it is lit while the stem is
/// audible and hollow when muted, so state is visible without a separate pill.
/// A small square LED next to the label reports QuickFX enable.
Rectangle {
    id: root

    property alias knob: volume

    required property string stemGroup
    required property string label
    required property color stemColor

    readonly property string fxGroup: `[QuickEffectRack1_${stemGroup}]`
    readonly property bool muted: statusControl.value != 0

    border.color: Theme.border
    border.width: 1
    color: Theme.surface1
    height: 72
    opacity: root.muted ? 0.76 : 1
    radius: Theme.radiusSm
    width: 72

    Behavior on opacity {
        NumberAnimation {
            duration: Theme.motionFast
        }
    }

    Mixxx.ControlProxy {
        id: statusControl

        group: root.stemGroup
        key: "mute"
    }

    Mixxx.ControlProxy {
        id: fxControl

        group: root.fxGroup
        key: "enabled"
    }

    // Stem colour key and mute toggle. Identity is always paired with the
    // text label; colour is never the only signal.
    Rectangle {
        id: colorChip

        anchors.bottom: colorSelector.top
        anchors.bottomMargin: Theme.space1
        anchors.left: parent.left
        anchors.leftMargin: Theme.space1
        anchors.top: parent.top
        anchors.topMargin: Theme.space1
        border.color: root.muted ? Theme.borderStrong : root.stemColor
        border.width: 1
        color: root.muted ? "transparent" : root.stemColor
        radius: Theme.radiusXs
        width: 8

        Accessible.checkable: true
        Accessible.checked: root.muted
        Accessible.name: qsTr("Mute %1").arg(root.label)
        Accessible.role: Accessible.CheckBox

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

        TapHandler {
            onTapped: statusControl.value = !statusControl.value
        }
        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }
    }

    Row {
        id: header

        anchors.left: colorChip.right
        anchors.leftMargin: Theme.space1
        anchors.right: parent.right
        anchors.rightMargin: Theme.space1
        anchors.top: parent.top
        anchors.topMargin: Theme.space1
        height: 12
        spacing: Theme.space1

        Text {
            id: stemLabel

            anchors.verticalCenter: parent.verticalCenter
            color: root.muted ? Theme.textMuted : Theme.textPrimary
            elide: Text.ElideRight
            font.capitalization: Font.MixedCase
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            height: parent.height
            text: root.label
            verticalAlignment: Text.AlignVCenter
            width: Math.max(0, parent.width - fxButton.width - parent.spacing)
        }

        Rectangle {
            id: fxButton

            anchors.verticalCenter: parent.verticalCenter
            border.color: fxControl.value ? Theme.accent : Theme.borderStrong
            border.width: 1
            color: fxControl.value ? Theme.accent : "transparent"
            height: 8
            radius: Theme.radiusXs
            width: 8

            Accessible.checkable: true
            Accessible.checked: fxControl.value != 0
            Accessible.name: qsTr("%1 QuickFX enabled").arg(root.label)
            Accessible.role: Accessible.CheckBox

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

            TapHandler {
                onTapped: fxControl.value = !fxControl.value
            }
            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
        }
    }

    Skin.ControlKnob {
        id: volume

        anchors.left: colorChip.right
        anchors.leftMargin: Theme.space1
        anchors.top: header.bottom
        anchors.topMargin: Theme.space1
        color: Theme.accent
        group: root.stemGroup
        height: 28
        key: "volume"
        width: 28

        Accessible.name: qsTr("%1 volume").arg(root.label)

        arcStart: 0
    }

    Skin.ControlMiniKnob {
        id: effectSuperKnob

        anchors.bottom: colorSelector.top
        anchors.bottomMargin: Theme.space1
        anchors.left: volume.right
        anchors.leftMargin: Theme.space1
        anchors.right: root.right
        anchors.rightMargin: Theme.space1
        anchors.top: header.bottom
        anchors.topMargin: Theme.space1
        arcStart: Knob.ArcStart.Minimum
        color: fxControl.value ? Theme.accent : Theme.textMuted
        group: root.fxGroup
        key: "super1"

        Accessible.name: qsTr("%1 QuickFX").arg(root.label)
    }

    Mixxx.ControlProxy {
        id: fxSelect

        group: root.fxGroup
        key: "loaded_chain_preset"
    }

    Skin.ComboBox {
        id: colorSelector

        anchors.bottom: root.bottom
        anchors.bottomMargin: Theme.space1
        anchors.left: root.left
        anchors.leftMargin: Theme.space1
        anchors.right: root.right
        anchors.rightMargin: Theme.space1
        clip: true
        currentIndex: fxSelect.value == -1 ? 0 : fxSelect.value
        font.pixelSize: Theme.fontSizeXs
        height: 16
        indicator.width: 0
        model: Mixxx.EffectsManager.quickChainPresetModel
        opacity: fxControl.value ? 1 : 0.6
        popupWidth: 150
        spacing: 2
        textRole: "display"

        Accessible.name: qsTr("%1 QuickFX preset").arg(root.label)

        onActivated: (index) => {
            fxSelect.value = index;
        }
    }
}
