import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

/// Compact stem control card: colour chip, mute status, volume knob,
/// QuickFX enable toggle, super knob and chain preset selector.
Rectangle {
    id: root

    property alias knob: volume

    required property string stemGroup
    required property string label
    required property color stemColor

    readonly property string fxGroup: `[QuickEffectRack1_${stemGroup}]`

    border.color: Theme.border
    border.width: 1
    color: Theme.surface2
    height: 56
    opacity: statusControl.value ? 0.6 : 1
    radius: Theme.radiusMd
    width: 56

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

    // Stem colour chip. Identity is always paired with the text label.
    Rectangle {
        id: colorChip

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.top: parent.top
        anchors.topMargin: 4
        color: root.stemColor
        radius: 2
        width: 4
    }

    Rectangle {
        id: statusButton

        anchors.left: colorChip.right
        anchors.leftMargin: 4
        anchors.top: root.top
        anchors.topMargin: 4
        border.color: statusControl.value ? Theme.danger : Theme.borderStrong
        border.width: 1
        color: statusControl.value ? Theme.danger : "transparent"
        height: 8
        radius: width / 2
        width: 8

        Accessible.checkable: true
        Accessible.checked: statusControl.value != 0
        Accessible.name: qsTr("Mute %1").arg(root.label)
        Accessible.role: Accessible.CheckBox

        TapHandler {
            onTapped: statusControl.value = !statusControl.value
        }
    }

    Text {
        id: stemLabel

        anchors.left: statusButton.right
        anchors.leftMargin: 4
        anchors.right: root.right
        anchors.rightMargin: 4
        anchors.top: root.top
        anchors.topMargin: 2
        color: Theme.textPrimary
        elide: Text.ElideRight
        font.bold: true
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        text: root.label
    }

    Skin.ControlKnob {
        id: volume

        anchors.left: colorChip.right
        anchors.leftMargin: 4
        anchors.top: statusButton.bottom
        anchors.topMargin: 2
        color: Theme.accent
        group: root.stemGroup
        height: 30
        key: "volume"
        width: 30

        Accessible.name: qsTr("%1 volume").arg(root.label)

        arcStart: 0
    }

    Rectangle {
        id: fxButton

        anchors.right: root.right
        anchors.rightMargin: 4
        anchors.top: stemLabel.bottom
        anchors.topMargin: 2
        border.color: fxControl.value ? Theme.accent : Theme.borderStrong
        border.width: 1
        color: fxControl.value ? Theme.accent : "transparent"
        height: 8
        radius: width / 2
        width: 8

        Accessible.checkable: true
        Accessible.checked: fxControl.value != 0
        Accessible.name: qsTr("%1 QuickFX enabled").arg(root.label)
        Accessible.role: Accessible.CheckBox

        TapHandler {
            onTapped: fxControl.value = !fxControl.value
        }
    }

    Skin.ControlMiniKnob {
        id: effectSuperKnob

        anchors.bottom: effectSelector.top
        anchors.bottomMargin: 2
        anchors.left: volume.right
        anchors.leftMargin: 2
        anchors.right: root.right
        anchors.rightMargin: 2
        anchors.top: fxButton.bottom
        anchors.topMargin: 2
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
        id: effectSelector

        anchors.bottom: root.bottom
        anchors.bottomMargin: 2
        anchors.left: root.left
        anchors.leftMargin: 2
        anchors.right: root.right
        anchors.rightMargin: 2
        clip: true
        currentIndex: fxSelect.value == -1 ? 0 : fxSelect.value
        font.pixelSize: Theme.fontSizeXs
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
