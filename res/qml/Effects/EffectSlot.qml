pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "../Theme"

Item {
    id: root

    required property int effectNumber
    readonly property string effectUnitGroup: slot.chainSlotGroup
    property bool expanded: false
    readonly property string group: slot.group
    property real maxSelectorWidth: 300
    readonly property int maximumParametersPerType: 8
    property Mixxx.EffectSlotProxy slot: Mixxx.EffectsManager.getEffectSlot(unitNumber, effectNumber)
    required property int unitNumber

    height: 50

    // Flat card behind the slot so the three slots read as separate rows.
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        color: Theme.surface2
        radius: Theme.radiusSm
    }
    Item {
        id: selector

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        width: Math.min(root.width, root.maxSelectorWidth)

        EffectFocusButton {
            id: focusButton

            anchors.left: parent.left
            anchors.margins: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            effectNumber: root.effectNumber
            effectUnitGroup: root.effectUnitGroup
            height: 16
            width: visible ? 16 : 0
        }
        Skin.ControlButton {
            id: effectEnableButton

            activeColor: Theme.textOnAccent
            anchors.bottom: parent.bottom
            anchors.left: focusButton.right
            anchors.margins: Theme.space1
            anchors.top: parent.top
            group: root.group
            key: "enabled"
            text: qsTr("ON")
            toggleable: true
            width: 40
        }
        EffectSelector {
            id: effectSelector

            anchors.bottom: parent.bottom
            anchors.left: effectEnableButton.right
            anchors.margins: Theme.space1
            anchors.right: effectMetaKnob.left
            anchors.top: parent.top
            slot: root.slot
        }
        Skin.ControlMiniKnob {
            id: effectMetaKnob

            anchors.bottom: parent.bottom
            anchors.margins: Theme.space1
            anchors.right: parent.right
            anchors.top: parent.top
            arcStart: Skin.Knob.ArcStart.Minimum
            color: Theme.warning
            group: root.group
            key: "meta"
            width: 40
        }
    }
    ListView {
        id: parametersView

        anchors.bottom: parent.bottom
        anchors.left: selector.right
        anchors.leftMargin: Theme.space2
        anchors.right: parent.right
        anchors.top: parent.top
        clip: true
        model: root.slot.parametersModel
        orientation: ListView.Horizontal
        spacing: Theme.space1
        visible: root.expanded

        delegate: Item {
            id: parameter

            required property string controlKey
            required property int index
            readonly property bool isButton: type === Mixxx.EffectSlotParametersModel.Button
            readonly property bool isKnob: type === Mixxx.EffectSlotParametersModel.Knob
            readonly property string label: shortName || name
            required property bool loaded
            required property string name
            required property string shortName
            readonly property bool shown: loaded && slotNumber > 0 && slotNumber <= root.maximumParametersPerType
            readonly property int slotNumber: {
                const match = controlKey.match(/(\d+)$/);
                return match ? Number(match[1]) : 0;
            }
            required property int type

            height: 50
            visible: shown
            width: shown ? 55 : 0

            Skin.EmbeddedText {
                anchors.bottomMargin: parameter.isKnob ? 14 : 0
                anchors.fill: parent
                font.bold: false
                text: parameter.label
                verticalAlignment: Text.AlignBottom
            }
            Loader {
                active: parameter.shown && parameter.isKnob
                anchors.horizontalCenter: parent.horizontalCenter
                height: 30
                width: 30

                sourceComponent: Skin.ControlMiniKnob {
                    arcStart: 0
                    color: Theme.warning
                    group: root.group
                    key: parameter.controlKey
                }
            }
            Loader {
                active: parameter.shown && parameter.isButton
                anchors.horizontalCenter: parent.horizontalCenter
                height: 22
                width: parent.width

                sourceComponent: Skin.ControlButton {
                    activeColor: Theme.textOnAccent
                    group: root.group
                    key: parameter.controlKey
                    text: qsTr("ON")
                    toggleable: true
                }
            }
            Row {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                height: 14
                spacing: 1
                visible: parameter.shown && parameter.isKnob

                Skin.ControlButton {
                    activeColor: Theme.textOnAccent
                    group: root.group
                    height: 14
                    key: parameter.controlKey + "_link_inverse"
                    text: qsTr("I")
                    toggleable: true
                    width: 14
                }
                Item {
                    height: 14
                    width: 38

                    Rectangle {
                        anchors.fill: parent
                        color: linkTypeControl.value > 0 ? Theme.accent : Theme.surface3
                        radius: Theme.radiusXs
                    }
                    Text {
                        anchors.centerIn: parent
                        color: linkTypeControl.value > 0 ? Theme.textOnAccent : Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        text: qsTr("LINK %1").arg(Math.round(linkTypeControl.value))
                    }
                    TapHandler {
                        onTapped: linkTypeControl.value = (Math.round(linkTypeControl.value) + 1) % 5
                    }
                    Mixxx.ControlProxy {
                        id: linkTypeControl

                        group: root.group
                        key: parameter.controlKey + "_link_type"
                    }
                }
            }
        }
    }
}
