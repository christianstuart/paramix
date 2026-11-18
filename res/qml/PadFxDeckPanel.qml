pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

/// One deck column inside the Pad FX window.
///
/// Four independent pads per bank, with per-pad effect, stem mask and mode.
/// The bank selector chooses which four pad configurations are loaded.
Rectangle {
    id: root

    required property string group

    readonly property string padFxGroup: group.substr(0, group.length - 1) + "_PadFX]"
    readonly property int stemCount: Math.max(0, Math.round(stemCountControl.value))
    readonly property var effectNames: [qsTr("Off"), qsTr("Vinyl Brake"), qsTr("Echo Out")]
    readonly property var modeNames: [qsTr("Momentary"), qsTr("Toggle"), qsTr("One Shot")]

    function targetSummary(mask) {
        const value = Math.round(mask);
        if (value === 15)
            return qsTr("All");
        if (value === 7)
            return qsTr("Instrumental");
        if (value === 8)
            return qsTr("Vocals");
        let parts = [];
        if (value & 1)
            parts.push("D");
        if (value & 2)
            parts.push("B");
        if (value & 4)
            parts.push("O");
        if (value & 8)
            parts.push("V");
        return parts.join("+");
    }
    function paramLabel(effect, index) {
        if (effect === 1)
            return [qsTr("Wind-down"), qsTr("Curve"), qsTr("Wind-up"), "", ""][index] || "";
        if (effect === 2)
            return [qsTr("Echo Time"), qsTr("Feedback"), qsTr("Tail"), qsTr("Reverb"), qsTr("Rev Time")][index] || "";
        return "";
    }
    function setTargetMask(targetControl, mask) {
        targetControl.value = mask;
    }
    function toggleTarget(targetControl, bit) {
        const current = Math.round(targetControl.value);
        const next = current ^ (1 << bit);
        targetControl.value = next === 0 ? (1 << bit) : next;
    }
    function stemName(bit) {
        return ["D", "B", "O", "V"][bit] || "";
    }

    color: Theme.surface1
    radius: Theme.radiusMd

    Component.onCompleted: {
        // The window exposes one bank per deck; keep the controller-facing
        // bank control at A and reserve the other banks for future layouts.
        if (bankControl.value !== 1)
            bankControl.value = 1;
    }

    Mixxx.ControlProxy {
        id: bankControl

        group: root.padFxGroup
        key: "bank"
    }
    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Deck %1").arg(root.group.replace(/[^0-9]/g, ""))
            }
            Text {
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: qsTr("Bank A")
            }
        }
        Text {
            Layout.fillWidth: true
            color: root.stemCount > 0 ? Theme.textMuted : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: root.stemCount > 0
                  ? qsTr("4 pads")
                  : qsTr("Load or separate a stem track")
        }

        Repeater {
            model: 4

            delegate: Rectangle {
                id: padCard

                required property int index

                Layout.fillWidth: true
                Layout.preferredHeight: 150
                color: Theme.surface2
                radius: Theme.radiusSm

                readonly property int pad: padCard.index + 1
                readonly property string padKey: "pad" + padCard.pad
                readonly property int effectIndex: Math.round(padEffect.value)
                readonly property int targetMask: Math.round(padTarget.value)
                readonly property string effectText: root.effectNames[Math.max(0, Math.min(2, padCard.effectIndex))]

                Mixxx.ControlProxy {
                    id: padControl

                    group: root.padFxGroup
                    key: padCard.padKey
                }
                Mixxx.ControlProxy {
                    id: padActive

                    group: root.padFxGroup
                    key: padCard.padKey + "_active"
                }
                Mixxx.ControlProxy {
                    id: padEffect

                    group: root.padFxGroup
                    key: "effect" + padCard.pad
                }
                Mixxx.ControlProxy {
                    id: padTarget

                    group: root.padFxGroup
                    key: "target" + padCard.pad
                }
                Mixxx.ControlProxy {
                    id: padMode

                    group: root.padFxGroup
                    key: "mode" + padCard.pad
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space1
                    enabled: root.stemCount > 0
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        Skin.Button {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 72
                            implicitHeight: 26
                            highlight: padActive.value != 0
                            text: qsTr("Pad %1 · %2").arg(padCard.pad).arg(padCard.effectText)

                            onPressedChanged: {
                                padControl.value = pressed ? 1 : 0;
                            }
                        }
                        Skin.ComboBox {
                            Layout.preferredWidth: 112
                            clip: true
                            currentIndex: Math.max(0, Math.min(2, padCard.effectIndex))
                            model: root.effectNames
                            popupMaxItem: 4

                            onActivated: index => {
                                padEffect.value = index;
                            }
                        }
                        Skin.ComboBox {
                            Layout.preferredWidth: 88
                            clip: true
                            currentIndex: Math.max(0, Math.min(2, Math.round(padMode.value)))
                            model: root.modeNames
                            popupMaxItem: 4

                            onActivated: index => {
                                padMode.value = index;
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Text {
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            text: qsTr("Stems")
                        }
                        Repeater {
                            model: 4

                            delegate: Skin.Button {
                                required property int index
                                readonly property bool on: (padCard.targetMask & (1 << index)) != 0

                                implicitHeight: 20
                                implicitWidth: 22
                                highlight: on
                                text: root.stemName(index)

                                onClicked: root.toggleTarget(padTarget, index)
                            }
                        }
                        Skin.Button {
                            implicitHeight: 20
                            implicitWidth: 38
                            highlight: padCard.targetMask === 7
                            text: qsTr("Inst")

                            onClicked: root.setTargetMask(padTarget, 7)
                        }
                        Skin.Button {
                            implicitHeight: 20
                            implicitWidth: 38
                            highlight: padCard.targetMask === 8
                            text: qsTr("Voc")

                            onClicked: root.setTargetMask(padTarget, 8)
                        }
                        Skin.Button {
                            implicitHeight: 20
                            implicitWidth: 30
                            highlight: padCard.targetMask === 15
                            text: qsTr("All")

                            onClicked: root.setTargetMask(padTarget, 15)
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        columnSpacing: Theme.space2
                        columns: 2
                        rowSpacing: 0

                        Repeater {
                            model: 5

                            delegate: Skin.PadFxParameter {
                                required property int index
                                readonly property string parameterLabel: root.paramLabel(padCard.effectIndex, index)

                                Layout.fillWidth: true
                                Layout.preferredWidth: 140
                                group: root.padFxGroup
                                key: "pad" + padCard.pad + "_param" + (index + 1)
                                label: parameterLabel
                                visible: parameterLabel.length > 0
                            }
                        }
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
        }
    }
}
