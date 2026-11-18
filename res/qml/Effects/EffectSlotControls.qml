pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme"

/// One effect slot inside the top-bar unit flyout. The strip keeps only the
/// selector and On key, so the focus key, meta knob and the slot's visible
/// parameters live here instead of taking permanent width. Parameters are the
/// real ControlProxies, so they sweep the sound; hidden ones are re-enabled
/// from the slot preset menu.
ColumnLayout {
    id: root

    /// Preferred column width. Layouts ignore a declarative implicitWidth, so
    /// the flyout binds Layout.preferredWidth to this instead.
    readonly property int columnWidth: Math.round(136 * Theme.uiScale)
    readonly property int controlWidth: Math.round(30 * Theme.uiScale)
    /// Focus is the engine value only: `focused_effect` persists, while
    /// `show_focus` is a classic-skin display hint that starts at 0.
    readonly property bool focused: Math.round(focusedEffect.value) === root.slotNumber
    // Skin.Button keeps 6 px of padding on each side, so a single glyph needs
    // at least this much before the label elides.
    readonly property int keySize: Math.max(Math.round(22 * Theme.uiScale), Theme.fontSizeXs * 2 + 8)
    readonly property int rowHeight: Math.round(26 * Theme.uiScale)
    required property Mixxx.EffectSlotProxy slot

    /// `slot.number` is zero-based; the engine's `focused_effect` is 1-based.
    readonly property int slotNumber: root.slot.number + 1
    required property string unitGroup

    implicitWidth: root.columnWidth
    opacity: root.slot.loaded ? 1 : 0.55
    spacing: Theme.space1

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
        // Focus selects the slot the unit's super knob drives.
        Skin.Button {
            Accessible.name: qsTr("Focus effect slot %1").arg(root.slotNumber)
            ToolTip.delay: 500
            ToolTip.text: qsTr("Focus this slot for the super knob")
            ToolTip.visible: focusHover.hovered
            enabled: root.slot.loaded
            highlight: root.focused
            implicitHeight: root.keySize
            implicitWidth: root.keySize
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
            implicitHeight: root.keySize
            implicitWidth: root.keySize
            text: "P"

            onClicked: slotPresetMenu.opened ? slotPresetMenu.close() : slotPresetMenu.open()

            HoverHandler {
                id: slotPresetHover

                cursorShape: Qt.PointingHandCursor
            }
        }
    }
    // Meta is the effect's character parameter (a filter type, a reverb size),
    // so it gets a 30 px knob above the 26 px parameter rows.
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
            Layout.preferredHeight: root.controlWidth
            Layout.preferredWidth: root.controlWidth
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
            Layout.preferredHeight: visible ? root.rowHeight : 0
            spacing: Theme.space1
            visible: parameterRow.loaded && parameterRow.controlKey.length > 0

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
                Layout.preferredHeight: root.rowHeight
                Layout.preferredWidth: root.rowHeight
                arcStart: Skin.Knob.ArcStart.Minimum
                color: Theme.textSecondary
                group: root.slot.group
                key: parameterRow.controlKey
                visible: parameterRow.type === Mixxx.EffectSlotParametersModel.Knob
            }
            Skin.ControlButton {
                Accessible.name: qsTr("Effect slot %1 %2").arg(root.slotNumber).arg(parameterRow.name)
                Layout.preferredWidth: root.controlWidth
                activeColor: Theme.textOnAccent
                group: root.slot.group
                implicitHeight: root.rowHeight
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
        y: Math.round(20 * Theme.uiScale)
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
