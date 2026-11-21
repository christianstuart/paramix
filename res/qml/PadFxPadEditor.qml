pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

/// The editable part of one Pad FX pad: effect, mode, stem targets and the
/// parameter rows for the selected effect.
///
/// Shared by the Pad FX window's expanded card and the deck pane's right-click
/// editor, so both write the same [ChannelN_PadFX] keys.
ColumnLayout {
    id: root

    required property string padFxGroup
    /// 1..4
    required property int pad
    /// The window's perform view shows the stem chips and shortcuts under every
    /// pad, so it hides the effect, mode and parameter rows.
    property bool showEffectRows: true

    readonly property var effectNames: [qsTr("Off"), qsTr("Vinyl Brake"), qsTr("Echo Out")]
    readonly property var modeNames: [qsTr("Momentary"), qsTr("Toggle"), qsTr("One Shot")]
    readonly property var stemLabels: [qsTr("Drums"), qsTr("Bass"), qsTr("Other"), qsTr("Vocals")]
    readonly property string effectRowLabel: qsTr("Effect")
    readonly property string modeRowLabel: qsTr("Mode")
    readonly property int effectIndex: Math.round(effectControl.value)
    readonly property int modeIndex: Math.max(0, Math.min(2, Math.round(modeControl.value)))
    readonly property int targetMask: Math.round(targetControl.value)

    /// Row labels share one column, sized by the wider of the two labels.
    readonly property int rowLabelWidth: Math.ceil(Math.max(rowFont.advanceWidth(root.effectRowLabel),
                                                             rowFont.advanceWidth(root.modeRowLabel)))
    /// Every slider in one editor starts at the same x: the column fits the
    /// widest parameter label of the current effect.
    readonly property int parameterLabelWidth: {
        let width = 0;
        for (let index = 0; index < 5; ++index)
            width = Math.max(width, Math.ceil(rowFont.advanceWidth(root.paramLabel(root.effectIndex, index))));
        return width;
    }
    /// Longest combo entry, plus Skin.ComboBox's paddings and chevron, so the
    /// closed selector never elides "Vinyl Brake" or "Momentary".
    readonly property int comboMinWidth: {
        let width = 0;
        for (let i = 0; i < root.effectNames.length; ++i)
            width = Math.max(width, comboFont.advanceWidth(root.effectNames[i]));
        for (let i = 0; i < root.modeNames.length; ++i)
            width = Math.max(width, comboFont.advanceWidth(root.modeNames[i]));
        return Math.ceil(width) + 2 * Theme.space2 + Theme.space2 + Theme.space1 + Theme.space5;
    }
    /// Stem chip: colour dot, gap, label and its trailing gap. Sized from the
    /// widest label so all four chips share one width.
    readonly property int stemChipMinWidth: {
        let width = 0;
        for (let i = 0; i < root.stemLabels.length; ++i)
            width = Math.max(width, rowFont.advanceWidth(root.stemLabels[i]));
        return Math.ceil(width) + Theme.space2 + 3 * Theme.space1;
    }

    FontMetrics {
        id: rowFont

        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
    }
    FontMetrics {
        id: comboFont

        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
    }

    function stemLabel(bit) {
        return root.stemLabels[bit] || "";
    }
    function stemColor(bit) {
        return [Theme.stemDrums, Theme.stemBass, Theme.stemOther, Theme.stemVocals][bit]
                || Theme.stemMuted;
    }
    function paramLabel(effect, index) {
        if (effect === 1)
            return [qsTr("Wind-down"), qsTr("Curve"), qsTr("Wind-up"), "", ""][index] || "";
        if (effect === 2)
            return [qsTr("Echo Time"), qsTr("Feedback"), qsTr("Tail"), qsTr("Reverb"), qsTr("Rev Time")][index] || "";
        return "";
    }
    function toggleTarget(bit) {
        const current = Math.round(targetControl.value);
        const next = current ^ (1 << bit);
        targetControl.value = next === 0 ? (1 << bit) : next;
    }

    spacing: Theme.space1

    Mixxx.ControlProxy {
        id: effectControl

        group: root.padFxGroup
        key: "effect" + root.pad
    }
    Mixxx.ControlProxy {
        id: modeControl

        group: root.padFxGroup
        key: "mode" + root.pad
    }
    Mixxx.ControlProxy {
        id: targetControl

        group: root.padFxGroup
        key: "target" + root.pad
    }

    // Effect selector
    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.padfxControlHeight
        spacing: Theme.space1
        visible: root.showEffectRows

        Text {
            Layout.minimumWidth: root.rowLabelWidth
            Layout.preferredHeight: Theme.padfxControlHeight
            Layout.preferredWidth: root.rowLabelWidth
            color: Theme.textSecondary
            elide: Text.ElideNone
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: root.effectRowLabel
            verticalAlignment: Text.AlignVCenter
        }
        Skin.ComboBox {
            Accessible.name: qsTr("Pad %1 effect").arg(root.pad)
            Layout.fillWidth: true
            Layout.minimumWidth: root.comboMinWidth
            currentIndex: root.effectIndex
            model: root.effectNames
            popupMaxItem: 3
            textLeftPadding: Theme.space2

            ToolTip.delay: 500
            ToolTip.text: qsTr("Effect for pad %1").arg(root.pad)
            ToolTip.visible: hovered

            onActivated: index => {
                effectControl.value = index;
            }
        }
    }

    // Mode selector
    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.padfxControlHeight
        spacing: Theme.space1
        visible: root.showEffectRows

        Text {
            Layout.minimumWidth: root.rowLabelWidth
            Layout.preferredHeight: Theme.padfxControlHeight
            Layout.preferredWidth: root.rowLabelWidth
            color: Theme.textSecondary
            elide: Text.ElideNone
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: root.modeRowLabel
            verticalAlignment: Text.AlignVCenter
        }
        Skin.ComboBox {
            Accessible.name: qsTr("Pad %1 mode").arg(root.pad)
            Layout.fillWidth: true
            Layout.minimumWidth: root.comboMinWidth
            currentIndex: root.modeIndex
            model: root.modeNames
            popupMaxItem: 3
            textLeftPadding: Theme.space2

            ToolTip.delay: 500
            ToolTip.text: qsTr("Press behavior for pad %1").arg(root.pad)
            ToolTip.visible: hovered

            onActivated: index => {
                modeControl.value = index;
            }
        }
    }

    // Labelled stem chips. The dot reinforces the label; it never replaces it.
    // Each chip is at least wide enough for its text, so a narrow editor wraps
    // them onto a second row instead of squeezing any label.
    Flow {
        id: stemFlow

        Layout.fillWidth: true
        Layout.preferredHeight: Math.ceil(stemFlow.childrenRect.height)
        spacing: Theme.space1

        readonly property int columns: 4
        readonly property int chipWidth: Math.max(root.stemChipMinWidth,
                Math.floor((stemFlow.width - (stemFlow.columns - 1) * Theme.space1)
                           / stemFlow.columns))

        Repeater {
            model: 4

            delegate: AbstractButton {
                id: stemChip

                required property int index

                readonly property bool on: (root.targetMask & (1 << stemChip.index)) !== 0
                readonly property color stemColor: root.stemColor(stemChip.index)
                readonly property string stemLabelText: root.stemLabel(stemChip.index)

                Accessible.checkable: true
                Accessible.checked: stemChip.on
                Accessible.name: qsTr("Pad %1 target %2").arg(root.pad).arg(stemChip.stemLabelText)
                Accessible.role: Accessible.CheckBox
                height: Theme.padfxHitHeight
                hoverEnabled: true
                padding: 0
                width: stemFlow.chipWidth

                ToolTip.delay: 500
                ToolTip.text: stemChip.on
                               ? qsTr("Remove %1 from pad %2").arg(stemChip.stemLabelText.toLowerCase()).arg(root.pad)
                               : qsTr("Include %1 in pad %2").arg(stemChip.stemLabelText.toLowerCase()).arg(root.pad)
                ToolTip.visible: stemChip.hovered

                onClicked: root.toggleTarget(stemChip.index)

                background: Rectangle {
                    border.color: stemChip.on ? stemChip.stemColor : Theme.border
                    border.width: 1
                    color: stemChip.on ? Qt.alpha(stemChip.stemColor, 0.18) : Theme.surface2
                    radius: Theme.radiusSm

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.motionFast
                        }
                    }
                }
                contentItem: RowLayout {
                    spacing: Theme.space1

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.leftMargin: Theme.space1
                        Layout.preferredHeight: Theme.space2
                        Layout.preferredWidth: Theme.space2
                        color: stemChip.on ? stemChip.stemColor : Theme.stemMuted
                        radius: Theme.radiusPill
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.rightMargin: Theme.space1
                        color: stemChip.on ? Theme.textPrimary : Theme.textMuted
                        elide: Text.ElideNone
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: stemChip.on
                                     ? Theme.fontWeightMedium
                                     : Theme.fontWeightRegular
                        text: stemChip.stemLabelText
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // All / Instrumental / Vocals shortcuts. Lit only when the mask equals the
    // preset exactly. Each key keeps its natural label width, so a narrow
    // editor wraps them onto a second row.
    Flow {
        id: presetFlow

        Layout.fillWidth: true
        Layout.preferredHeight: Math.ceil(presetFlow.childrenRect.height)
        spacing: Theme.space1

        Repeater {
            model: [
                {
                    "label": qsTr("All"),
                    "mask": 15,
                    "hint": qsTr("Set pad %1 to all stems")
                },
                {
                    "label": qsTr("Instrumental"),
                    "mask": 7,
                    "hint": qsTr("Set pad %1 to drums, bass and other")
                },
                {
                    "label": qsTr("Vocals"),
                    "mask": 8,
                    "hint": qsTr("Set pad %1 to vocals")
                }
            ]

            delegate: Skin.Button {
                id: presetKey

                required property var modelData

                Accessible.name: presetKey.modelData.hint.arg(root.pad)
                height: Theme.padfxHitHeight
                highlight: root.targetMask === presetKey.modelData.mask
                text: presetKey.modelData.label
                width: Math.max(presetKey.implicitWidth,
                                Math.floor((presetFlow.width - 2 * Theme.space1) / 3))

                ToolTip.delay: 500
                ToolTip.text: presetKey.modelData.hint.arg(root.pad)
                ToolTip.visible: presetKey.hovered

                onClicked: targetControl.value = presetKey.modelData.mask
            }
        }
    }

    // Effect parameters, only the rows the effect labels.
    Repeater {
        model: 5

        delegate: Skin.PadFxParameter {
            required property int index

            readonly property string parameterLabel: root.paramLabel(root.effectIndex, index)

            Layout.fillWidth: true
            Layout.preferredHeight: Theme.padfxControlHeight
            group: root.padFxGroup
            key: "pad" + root.pad + "_param" + (index + 1)
            label: parameterLabel
            labelWidth: root.parameterLabelWidth
            ownerLabel: qsTr("Pad %1").arg(root.pad)
            visible: root.showEffectRows && parameterLabel.length > 0
        }
    }
}
