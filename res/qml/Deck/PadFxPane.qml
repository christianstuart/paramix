import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "../Theme"

/// Per-deck Pad FX bank editor.
///
/// A bank holds four independent pad effects. Each pad has its own effect and
/// stem mask, so a bank can contain e.g. Brake on Instrumental, Brake on Vocals,
/// Echo Out on Instrumental and Echo Out on Vocals.
Rectangle {
    id: root

    required property string group

    readonly property string padFxGroup: group.substr(0, group.length - 1) + "_PadFX]"
    readonly property int bank: Math.round(bankControl.value)
    readonly property int stemCount: Math.max(0, Math.round(stemCountControl.value))
    readonly property var effectNames: [qsTr("Off"), qsTr("Vinyl Brake"), qsTr("Echo Out")]
    readonly property var modeNames: [qsTr("Momentary"), qsTr("Toggle"), qsTr("One Shot")]
    property int editingPad: 0
    readonly property int editingEffect: Math.round(editingEffectControl.value)

    function targetSummary(mask) {
        const value = Math.round(mask);
        if (value === 15)
            return qsTr("All");
        if (value === 7)
            return qsTr("Inst");
        if (value === 8)
            return qsTr("Voc");
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
    function toggleTarget(targetControl, bit) {
        const current = Math.round(targetControl.value);
        const next = current ^ (1 << bit);
        targetControl.value = next === 0 ? (1 << bit) : next;
    }
    function stemName(bit) {
        return ["D", "B", "O", "V"][bit] || "";
    }
    function paramLabel(effect, index) {
        if (effect === 1)
            return [qsTr("Time"), qsTr("Curve")][index] || "";
        if (effect === 2)
            return [qsTr("Beats"), qsTr("Feedback"), qsTr("Tail")][index] || "";
        return "";
    }

    color: "transparent"

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
    Mixxx.ControlProxy {
        id: editingEffectControl

        group: root.padFxGroup
        key: "effect" + (root.editingPad + 1)
    }

    Popup {
        id: paramPopup

        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: Theme.space2
        width: 190

        background: Rectangle {
            border.color: Theme.borderStrong
            border.width: 1
            color: Theme.surface1
            radius: Theme.radiusMd
        }
        contentItem: Column {
            spacing: Theme.space2

            Text {
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: qsTr("Pad %1 · %2").arg(root.editingPad + 1).arg(root.effectNames[Math.max(0, Math.min(2, root.editingEffect))])
            }
            Row {
                spacing: Theme.space2

                Repeater {
                    model: 3

                    delegate: Column {
                        required property int index
                        visible: root.paramLabel(root.editingEffect, index).length > 0

                        spacing: 2
                        Skin.ControlMiniKnob {
                            implicitHeight: 34
                            implicitWidth: 34
                            group: root.padFxGroup
                            key: "pad" + (root.editingPad + 1) + "_param" + (index + 1)
                        }
                        Text {
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            text: root.paramLabel(root.editingEffect, index)
                        }
                    }
                }
            }
            Skin.Button {
                Layout.alignment: Qt.AlignRight
                implicitHeight: 20
                text: qsTr("Close")
                onClicked: paramPopup.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space1
        spacing: Theme.space1

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            spacing: Theme.space2

            Repeater {
                model: 4

                delegate: Skin.Button {
                    required property int index
                    readonly property bool selected: root.bank === (index + 1)

                    Layout.preferredHeight: 18
                    Layout.preferredWidth: 24
                    enabled: root.stemCount > 0
                    highlight: selected
                    text: String.fromCharCode(65 + index)

                    onClicked: bankControl.value = index + 1
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                color: root.stemCount > 0 ? Theme.textMuted : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                text: root.stemCount > 0
                      ? qsTr("Pad FX · bank %1").arg(String.fromCharCode(64 + root.bank))
                      : qsTr("Pad FX need a stem track")
            }
        }

        Repeater {
            model: 4

            delegate: RowLayout {
                id: padRow

                required property int index

                Layout.fillWidth: true
                Layout.preferredHeight: 18
                enabled: root.stemCount > 0
                spacing: Theme.space1

                readonly property int pad: padRow.index + 1
                readonly property string padKey: "pad" + padRow.pad
                readonly property int effectIndex: Math.round(padEffect.value)
                readonly property int targetMask: Math.round(padTarget.value)
                readonly property string effectText: root.effectNames[Math.max(0, Math.min(2, padRow.effectIndex))]

                Mixxx.ControlProxy {
                    id: padControl

                    group: root.padFxGroup
                    key: padRow.padKey
                }
                Mixxx.ControlProxy {
                    id: padActive

                    group: root.padFxGroup
                    key: padRow.padKey + "_active"
                }
                Mixxx.ControlProxy {
                    id: padEffect

                    group: root.padFxGroup
                    key: "effect" + padRow.pad
                }
                Mixxx.ControlProxy {
                    id: padTarget

                    group: root.padFxGroup
                    key: "target" + padRow.pad
                }
                Mixxx.ControlProxy {
                    id: padMode

                    group: root.padFxGroup
                    key: "mode" + padRow.pad
                }

                Skin.Button {
                    id: trigger

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 52
                    activeColor: Theme.textOnAccent
                    highlight: padActive.value != 0
                    text: padRow.effectText + " " + root.targetSummary(padRow.targetMask)

                    onPressedChanged: {
                        padControl.value = pressed ? 1 : 0;
                    }
                }
                Skin.ComboBox {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 74
                    clip: true
                    currentIndex: Math.max(0, Math.min(2, padRow.effectIndex))
                    model: root.effectNames
                    popupMaxItem: 4

                    onActivated: index => {
                        padEffect.value = index;
                    }
                }
                Row {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 2

                    Repeater {
                        model: 4

                        delegate: Skin.Button {
                            required property int index
                            readonly property bool on: (padRow.targetMask & (1 << index)) != 0

                            implicitHeight: 18
                            implicitWidth: 14
                            highlight: on
                            text: root.stemName(index)

                            onClicked: root.toggleTarget(padTarget, index)
                        }
                    }
                }
                Skin.ComboBox {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 56
                    clip: true
                    currentIndex: Math.max(0, Math.min(2, Math.round(padMode.value)))
                    model: root.modeNames
                    popupMaxItem: 4

                    onActivated: index => {
                        padMode.value = index;
                    }
                }
                Skin.Button {
                    implicitHeight: 18
                    implicitWidth: 18
                    text: "..."

                    onClicked: {
                        root.editingPad = padRow.index;
                        paramPopup.open();
                    }
                }
            }
        }
    }
}
