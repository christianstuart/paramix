pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

/// The editable part of one shared Pad FX assignment: effect, targets and the
/// parameter rows for the selected effect. Every pad is momentary.
ColumnLayout {
    id: root

    required property string presetGroup
    required property int bank
    /// 1..8
    required property int pad
    /// The window's perform view used to hide these rows; the shared popup
    /// keeps the property so callers can still compact the editor.
    property bool showEffectRows: true

    readonly property var padEffects: Mixxx.EffectsManager.padEffectsModel
    readonly property var stemLabels: [qsTr("Drums"), qsTr("Bass"), qsTr("Other"), qsTr("Vocals")]
    readonly property string effectRowLabel: qsTr("Effect")
    readonly property int effectIndex: Math.max(0, Math.round(effectControl.value))
    readonly property int targetMask: Math.max(0, Math.min(15, Math.round(targetControl.value)))
    readonly property var effectRow: root.effectIndex > 0
            ? root.padEffects.get(root.effectIndex)
            : null
    readonly property bool beatFx: root.effectRow ? !!root.effectRow.isBeatFx : false
    readonly property var beatStepLabels: root.effectRow ? root.effectRow.beatStepLabels : []
    readonly property var beatStepValues: root.effectRow ? root.effectRow.beatStepValues : []
    readonly property int beatStepParamIndex: root.effectRow
            ? root.effectRow.beatStepParameterIndex
            : -1
    readonly property int currentBeatStep: {
        if (!root.beatFx || root.beatStepValues.length === 0)
            return -1;
        let nearest = 0;
        let nearestDistance = Number.MAX_VALUE;
        for (let index = 0; index < root.beatStepValues.length; ++index) {
            const distance = Math.abs(beatStepParamControl.value - root.beatStepValues[index]);
            if (distance < nearestDistance) {
                nearest = index;
                nearestDistance = distance;
            }
        }
        return nearest;
    }

    readonly property int rowLabelWidth: Math.ceil(
            rowFont.advanceWidth(root.effectRowLabel))
    /// Every slider in one editor starts at the same x: the column fits the
    /// widest parameter label of the current effect.
    readonly property int parameterLabelWidth: {
        let width = 0;
        for (let index = 0; index < 8; ++index)
            width = Math.max(width, Math.ceil(rowFont.advanceWidth(root.paramLabel(root.effectIndex, index))));
        return width;
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

    function stemLabel(bit) {
        return root.stemLabels[bit] || "";
    }
    function stemColor(bit) {
        return [Theme.stemDrums, Theme.stemBass, Theme.stemOther, Theme.stemVocals][bit]
                || Theme.stemMuted;
    }
    function effectIdAt(index) {
        const row = index > 0 ? root.padEffects.get(index) : null;
        return row ? row.effectId : "";
    }
    function paramLabel(effect, index) {
        if (effect <= 0)
            return "";
        const id = root.effectIdAt(effect);
        if (id === "org.paramix.effects.vinylbrake")
            return [qsTr("Wind-down"), qsTr("Curve"), qsTr("Wind-up"), "", "", "", "", ""][index] || "";
        if (id === "org.paramix.effects.echoout")
            return [qsTr("Echo Time"), qsTr("Feedback"), qsTr("Tail"), qsTr("Reverb"), qsTr("Rev Time"), "", "", ""][index] || "";
        const row = root.padEffects.get(effect);
        const names = row && row.parameters ? row.parameters : [];
        return index < names.length ? names[index] : "";
    }
    function toggleTarget(bit) {
        const current = Math.round(targetControl.value);
        const next = current ^ (1 << bit);
        targetControl.value = next === 0 ? (1 << bit) : next;
    }

    spacing: Theme.space1

    Mixxx.ControlProxy {
        id: effectControl

        group: root.presetGroup
        key: "fx" + root.bank + "_effect" + root.pad
    }
    Mixxx.ControlProxy {
        id: targetControl

        group: root.presetGroup
        key: "fx" + root.bank + "_target" + root.pad
    }
    Mixxx.ControlProxy {
        id: beatStepParamControl

        group: root.presetGroup
        key: root.beatStepParamIndex >= 0
             ? "fx" + root.bank + "_pad" + root.pad + "_param" + (root.beatStepParamIndex + 1)
             : "fx" + root.bank + "_pad" + root.pad + "_param1"
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
            Layout.minimumWidth: Theme.space8 * 2
            currentIndex: root.effectIndex
            model: root.padEffects
            popupMaxItem: 8
            textLeftPadding: Theme.space2
            textRole: "display"

            ToolTip.delay: 500
            ToolTip.text: qsTr("Effect for pad %1 on every deck").arg(root.pad)
            ToolTip.visible: hovered

            onActivated: index => {
                effectControl.value = index;
            }
        }
    }

    Text {
        Layout.fillWidth: true
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        text: qsTr("Momentary: hold the pad to engage and release to stop.")
        visible: root.showEffectRows
        wrapMode: Text.WordWrap
    }

    // Beat FX keep their quantized step grid, like the top effect chain.
    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? Math.ceil(stepFlow.childrenRect.height) : 0
        spacing: Theme.space1
        visible: root.beatFx

        Text {
            Layout.minimumWidth: root.rowLabelWidth
            Layout.preferredWidth: root.rowLabelWidth
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: qsTr("Step")
            verticalAlignment: Text.AlignTop
        }
        Flow {
            id: stepFlow

            Layout.fillWidth: true
            spacing: Theme.space1

            Repeater {
                model: root.beatStepLabels

                Skin.Button {
                    id: stepKey

                    required property int index
                    required property string modelData

                    Accessible.name: qsTr("Pad %1 beat step %2").arg(root.pad).arg(stepKey.modelData)
                    highlight: stepKey.index === root.currentBeatStep
                    implicitHeight: Theme.padfxHitHeight
                    implicitWidth: Theme.fxBeatGridKeyWidth
                    text: stepKey.modelData

                    ToolTip.delay: 500
                    ToolTip.text: stepKey.Accessible.name
                    ToolTip.visible: stepKey.hovered

                    onClicked: beatStepParamControl.value = root.beatStepValues[stepKey.index]
                }
            }
        }
    }

    // Deck target. The whole deck is one exclusive target; the stem chips set
    // a stem mask instead.
    Skin.Button {
        Accessible.checkable: true
        Accessible.checked: root.targetMask === 0
        Accessible.name: qsTr("Pad %1 target deck").arg(root.pad)
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.padfxHitHeight
        highlight: root.targetMask === 0
        text: qsTr("Deck")

        ToolTip.delay: 500
        ToolTip.text: qsTr("Apply pad %1 to the whole deck").arg(root.pad)
        ToolTip.visible: hovered

        onClicked: targetControl.value = 0
    }

    // Labelled stem chips. The dot reinforces the label; it never replaces it.
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

    // All / Instrumental / Vocals shortcuts. Deck is selected by the key above.
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
        model: 8

        delegate: Skin.PadFxParameter {
            required property int index

            readonly property string parameterLabel: root.paramLabel(root.effectIndex, index)

            Layout.fillWidth: true
            Layout.preferredHeight: Theme.padfxControlHeight
            group: root.presetGroup
            key: "fx" + root.bank + "_pad" + root.pad + "_param" + (index + 1)
            label: parameterLabel
            labelWidth: root.parameterLabelWidth
            ownerLabel: qsTr("Pad %1").arg(root.pad)
            visible: root.showEffectRows && parameterLabel.length > 0
                     && !(root.beatFx && index === root.beatStepParamIndex)
        }
    }
}
