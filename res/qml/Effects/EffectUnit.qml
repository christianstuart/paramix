import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "../Theme"

/// One effect unit: three slots plus the unit controls in a flat side panel.
Item {
    id: root

    readonly property bool expanded: expandedControl.value > 0
    readonly property string group: unit.group
    readonly property Mixxx.EffectUnitProxy unit: Mixxx.EffectsManager.getEffectUnit(unitNumber)
    required property int unitNumber

    implicitHeight: effectContainer.height

    Item {
        id: effectContainer

        anchors.left: parent.left
        anchors.margins: Theme.space1
        anchors.right: effectUnitControlsFrame.left
        anchors.top: parent.top
        height: root.expanded ? 160 : 84

        EffectSlot {
            id: effect1

            anchors.left: parent.left
            anchors.top: parent.top
            effectNumber: 1
            expanded: root.expanded
            height: 50
            unitNumber: root.unitNumber
            width: root.expanded ? parent.width : parent.width / 3
        }
        EffectSlot {
            id: effect2

            anchors.left: root.expanded ? parent.left : effect1.right
            anchors.top: root.expanded ? effect1.bottom : parent.top
            effectNumber: 2
            expanded: root.expanded
            height: 50
            unitNumber: root.unitNumber
            width: root.expanded ? parent.width : parent.width / 3
        }
        EffectSlot {
            id: effect3

            anchors.left: root.expanded ? parent.left : effect2.right
            anchors.top: root.expanded ? effect2.bottom : parent.top
            effectNumber: 3
            expanded: root.expanded
            height: 50
            unitNumber: root.unitNumber
            width: root.expanded ? parent.width : parent.width / 3
        }
    }
    Rectangle {
        id: effectUnitControlsFrame

        anchors.bottom: parent.bottom
        anchors.margins: Theme.space1
        anchors.right: parent.right
        anchors.top: parent.top
        border.color: Theme.border
        border.width: 1
        color: Theme.surface2
        radius: Theme.radiusSm
        width: 150

        Column {
            anchors.fill: parent
            anchors.margins: Theme.space1
            spacing: Theme.space1

            Row {
                spacing: Theme.space1

                Skin.ControlButton {
                    Accessible.name: root.expanded ? qsTr("Hide effect parameters") : qsTr("Show effect parameters")
                    activeColor: Theme.textOnAccent
                    group: root.group
                    height: 24
                    key: "show_parameters"
                    text: root.expanded ? qsTr("Hide") : qsTr("Show")
                    toggleable: true
                    width: 40
                }
                Skin.Button {
                    Accessible.name: qsTr("Effect unit %1 preset").arg(root.unitNumber)
                    activeColor: Theme.textOnAccent
                    height: 24
                    text: qsTr("FX %1").arg(root.unitNumber)
                    width: 44

                    onClicked: presetPopup.open()
                }
                Skin.ControlButton {
                    activeColor: Theme.textOnAccent
                    group: root.group
                    height: 24
                    key: "group_[Headphone]_enable"
                    text: qsTr("PFL")
                    toggleable: true
                    width: 42
                }
            }
            Row {
                spacing: Theme.space1

                Skin.ControlButton {
                    activeColor: Theme.textOnAccent
                    group: root.group
                    height: 30
                    key: "mix_mode"
                    text: qsTr("D/W")
                    toggleable: true
                    width: 40
                }
                Skin.ControlKnob {
                    Accessible.name: qsTr("Effect unit dry/wet")
                    Accessible.role: Accessible.Slider
                    arcStart: Skin.Knob.ArcStart.Minimum
                    color: Theme.textPrimary
                    group: root.group
                    height: 40
                    key: "mix"
                    width: 40
                }
                Skin.ControlKnob {
                    Accessible.name: qsTr("Effect unit super knob")
                    Accessible.role: Accessible.Slider
                    arcStart: Skin.Knob.ArcStart.Minimum
                    color: Theme.textSecondary
                    group: root.group
                    height: 40
                    key: "super1"
                    visible: showSuperKnobsControl.value > 0
                    width: visible ? 40 : 0
                }
            }
        }
    }
    EffectChainPresetPopup {
        id: presetPopup

        slot1: effect1.slot
        slot2: effect2.slot
        slot3: effect3.slot
        unit: root.unit
    }
    Mixxx.ControlProxy {
        id: expandedControl

        group: root.group
        key: "show_parameters"
    }
    Mixxx.ControlProxy {
        id: showSuperKnobsControl

        group: "[Skin]"
        key: "show_superknobs"
    }
}
