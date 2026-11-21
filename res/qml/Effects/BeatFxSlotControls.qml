pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme"

/// One Beat FX slot inside the top-bar unit flyout: the slot header, the step
/// keys, the Level/Depth knob, the meta knob and the remaining parameters.
/// EffectSlotControls has no room for the step grid and its rows are not
/// reusable without changing it, so the header, meta and parameter rows are
/// duplicated here.
ColumnLayout {
    id: root

    /// The step grid wraps at this width; the flyout binds its
    /// Layout.preferredWidth to columnWidth.
    readonly property int columnWidth: Theme.fxBeatColumnWidth
    /// Focus is the engine value only: `focused_effect` persists, while
    /// `show_focus` is a classic-skin display hint that starts at 0.
    readonly property bool focused: Math.round(focusedEffect.value) === root.slotNumber
    required property Mixxx.EffectSlotProxy slot
    /// `slot.number` is zero-based; the engine's `focused_effect` is 1-based.
    readonly property int slotNumber: root.slot.number + 1
    /// 0-based index of the current step, -1 when the slot is not Beat FX.
    readonly property int step: Math.round(beatStep.value)
    readonly property int stepCount: root.slot.beatStepLabels.length
    readonly property int gridColumns: Math.max(1, Math.floor((root.columnWidth + Theme.space1) / (Theme.fxBeatGridKeyWidth + Theme.space1)))
    required property string unitGroup

    implicitWidth: root.columnWidth
    opacity: root.slot.loaded ? 1 : 0.55
    spacing: Theme.space1

    /// There is no direct "set step" control: walk to the key with the same
    /// up/down pushes the strip stepper uses.
    function stepTo(index) {
        const delta = index - root.step;
        for (let count = 0; count < Math.abs(delta); ++count) {
            if (delta > 0) {
                beatUp.trigger();
            } else {
                beatDown.trigger();
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space1

        Text {
            Layout.fillWidth: true
            color: Theme.textPrimary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: root.slot.loaded ? qsTr("Slot %1: %2").arg(root.slotNumber).arg(root.slot.effectName) : qsTr("Slot %1").arg(root.slotNumber)
        }
        Text {
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: qsTr("BEAT")
        }
        // Focus selects the slot the unit's super knob drives.
        Skin.Button {
            Accessible.name: qsTr("Focus effect slot %1").arg(root.slotNumber)
            ToolTip.delay: 500
            ToolTip.text: qsTr("Focus this slot for the super knob")
            ToolTip.visible: focusHover.hovered
            enabled: root.slot.loaded
            highlight: root.focused
            implicitHeight: Theme.fxBeatKeySize
            implicitWidth: Theme.fxBeatKeySize
            text: "F"

            onClicked: {
                focusedEffect.value = root.focused ? 0 : root.slotNumber;
                // Turn the classic focus mode on so the persisted preference
                // and controller LEDs agree that a slot is focused.
                showFocus.value = 1;
            }

            HoverHandler {
                id: focusHover

                cursorShape: Qt.PointingHandCursor
            }
        }
        Skin.Button {
            Accessible.name: qsTr("Effect slot %1 presets").arg(root.slotNumber)
            ToolTip.delay: 500
            ToolTip.text: qsTr("Parameter visibility and snapshot")
            ToolTip.visible: slotPresetHover.hovered
            enabled: root.slot.loaded
            implicitHeight: Theme.fxBeatKeySize
            implicitWidth: Theme.fxBeatKeySize
            text: "P"

            onClicked: slotPresetMenu.opened ? slotPresetMenu.close() : slotPresetMenu.open()

            HoverHandler {
                id: slotPresetHover

                cursorShape: Qt.PointingHandCursor
            }
        }
    }
    GridLayout {
        Layout.fillWidth: true
        columnSpacing: Theme.space1
        columns: root.gridColumns
        rowSpacing: Theme.space1

        Repeater {
            model: root.slot.beatStepLabels

            Skin.Button {
                id: stepKey

                required property int index
                required property string modelData

                Accessible.name: qsTr("Beat step %1").arg(stepKey.modelData)
                Layout.preferredWidth: Theme.fxBeatGridKeyWidth
                ToolTip.delay: 500
                ToolTip.text: Accessible.name
                ToolTip.visible: hovered
                highlight: stepKey.index === root.step
                implicitHeight: Theme.fxControlHeight
                text: stepKey.modelData

                onClicked: root.stepTo(stepKey.index)

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space1
        visible: root.slot.beatLevelName.length > 0

        Text {
            Layout.fillWidth: true
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: root.slot.beatLevelName.length > 0 ? root.slot.beatLevelName : qsTr("Level/Depth")
        }
        Skin.ControlKnob {
            Accessible.name: qsTr("Effect slot %1 level").arg(root.slotNumber)
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: Theme.fxBeatLevelKnobSize
            Layout.preferredWidth: Theme.fxBeatLevelKnobSize
            ToolTip.delay: 500
            ToolTip.text: qsTr("Level/Depth")
            ToolTip.visible: levelHover.hovered
            arcStart: Skin.Knob.ArcStart.Minimum
            color: Theme.textPrimary
            group: root.slot.group
            key: "beat_level"

            HoverHandler {
                id: levelHover

                cursorShape: Qt.PointingHandCursor
            }
        }
    }
    // Meta is the effect's character parameter, so it keeps the knob row above
    // the remaining parameters.
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space1

        Text {
            Layout.fillWidth: true
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: qsTr("Meta")
        }
        Skin.ControlKnob {
            Accessible.name: qsTr("Effect slot %1 meta").arg(root.slotNumber)
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: Theme.fxKnobSize
            Layout.preferredWidth: Theme.fxKnobSize
            ToolTip.delay: 500
            ToolTip.text: qsTr("Meta")
            ToolTip.visible: metaHover.hovered
            arcStart: Skin.Knob.ArcStart.Minimum
            color: Theme.textPrimary
            enabled: root.slot.loaded
            group: root.slot.group
            key: "meta"

            HoverHandler {
                id: metaHover

                cursorShape: Qt.PointingHandCursor
            }
        }
    }
    Repeater {
        model: root.slot.parametersModel

        RowLayout {
            id: parameterRow

            required property string controlKey
            required property bool loaded
            required property string name
            required property string shortName
            required property int type

            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Theme.fxControlHeight : 0
            spacing: Theme.space1
            visible: parameterRow.loaded && parameterRow.controlKey.length > 0

            HoverHandler {
                id: parameterHover
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textSecondary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                text: parameterRow.shortName.length > 0 ? parameterRow.shortName : parameterRow.name
            }
            Skin.ControlKnob {
                Accessible.name: qsTr("Effect slot %1 %2").arg(root.slotNumber).arg(parameterRow.name)
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: Theme.fxKnobSize
                Layout.preferredWidth: Theme.fxKnobSize
                ToolTip.delay: 500
                ToolTip.text: parameterRow.name
                ToolTip.visible: parameterHover.hovered
                arcStart: Skin.Knob.ArcStart.Minimum
                color: Theme.textSecondary
                group: root.slot.group
                key: parameterRow.controlKey
                visible: parameterRow.type === Mixxx.EffectSlotParametersModel.Knob
            }
            Skin.ControlButton {
                Accessible.name: qsTr("Effect slot %1 %2").arg(root.slotNumber).arg(parameterRow.name)
                Layout.preferredWidth: Theme.fxOnKeyWidth
                ToolTip.delay: 500
                ToolTip.text: parameterRow.name
                ToolTip.visible: parameterHover.hovered
                activeColor: Theme.textOnAccent
                group: root.slot.group
                implicitHeight: Theme.fxControlHeight
                key: parameterRow.controlKey
                text: qsTr("On")
                toggleable: true
                visible: parameterRow.type === Mixxx.EffectSlotParametersModel.Button
            }
        }
    }
    EffectSlotPresetMenu {
        id: slotPresetMenu

        slot: root.slot
        x: Math.max(0, root.width - width)
        y: Theme.space4 + Theme.space1
    }
    Mixxx.ControlProxy {
        id: beatDown

        group: root.slot.group
        key: "beat_down"
    }
    Mixxx.ControlProxy {
        id: beatStep

        group: root.slot.group
        key: "beat_step"
    }
    Mixxx.ControlProxy {
        id: beatUp

        group: root.slot.group
        key: "beat_up"
    }
    // These controls live on the unit group, not on the slot group.
    Mixxx.ControlProxy {
        id: focusedEffect

        group: root.unitGroup
        key: "focused_effect"
    }
    Mixxx.ControlProxy {
        id: showFocus

        group: root.unitGroup
        key: "show_focus"
    }
}
