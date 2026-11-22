pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

/// One deck column inside the Pad FX window.
///
/// Every deck shows the same 8 shared pad assignments. The column only owns the
/// deck's active bank and its pad triggers.
Rectangle {
    id: root

    required property string group

    readonly property int deckNumber: Number(root.group.replace(/[^0-9]/g, ""))
    readonly property string padFxGroup: root.group.substr(0, root.group.length - 1) + "_PadFX]"
    readonly property string presetGroup: "[PadFX]"
    readonly property int bank: Math.max(1, Math.min(2,
            Math.round(bankControl.value)))

    color: Theme.surface1
    implicitWidth: Theme.padfxDeckMinWidth
    radius: Theme.radiusMd

    Mixxx.ControlProxy {
        id: bankControl

        group: root.padFxGroup
        key: "bank"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.space6
            spacing: Theme.space1

            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                elide: Text.ElideNone
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Deck %1").arg(root.deckNumber)
                verticalAlignment: Text.AlignVCenter
            }
            Skin.Button {
                id: bankKey

                Accessible.description: qsTr("Click for FX 1, shift-click for FX 2")
                Accessible.name: qsTr("Pad FX bank for deck %1").arg(root.deckNumber)
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: Theme.padfxHitHeight
                highlight: root.bank === 2
                implicitHeight: Theme.padfxHitHeight
                text: qsTr("FX %1").arg(root.bank)

                ToolTip.delay: 500
                ToolTip.text: qsTr("Pad FX bank. Click for FX 1, shift-click for FX 2.")
                ToolTip.visible: bankHover.hovered

                onClicked: bankControl.value = 1

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton

                    onClicked: mouse => {
                        bankControl.value = (mouse.modifiers & Qt.ShiftModifier) !== 0
                                ? 2 : 1;
                    }
                }
                HoverHandler {
                    id: bankHover
                }
            }
        }

        GridLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            columnSpacing: Theme.space1
            columns: 4
            rowSpacing: Theme.space1

            Repeater {
                model: 8

                delegate: Skin.PadFxPadFace {
                    required property int index

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumHeight: 0
                    Layout.minimumWidth: 0
                    bank: root.bank
                    pad: index + 1
                    padFxGroup: root.padFxGroup
                    presetGroup: root.presetGroup

                    onEditRequested: (pad, anchor) => padPopup.openFor(anchor, pad)
                }
            }
        }

        Text {
            Layout.fillWidth: true
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Right click a pad to edit it on every deck")
            wrapMode: Text.WordWrap
        }
    }

    Skin.PadFxPadPopup {
        id: padPopup

        bank: root.bank
        padFxGroup: root.padFxGroup
        presetGroup: root.presetGroup
    }
}
