pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme"

/// One standard effect chain in the top-bar strip: unit power, the three
/// effect selectors with their On keys, dry/wet, the chain preset key and the
/// slot settings flyout behind the FX badge. `compact` is the narrow variant
/// used when all four units share the strip.
RowLayout {
    id: root

    readonly property int assignKeyWidth: compact ? Math.round(16 * Theme.uiScale) : Theme.fxAssignKeyWidth
    readonly property int badgeWidth: compact ? Math.max(Math.round(30 * Theme.uiScale), Theme.fontSizeXs * 3 + 6) : Theme.fxUnitKeyMinWidth
    property bool compact: false
    readonly property string group: unit.group
    readonly property int knobSize: compact ? Math.round(20 * Theme.uiScale) : Theme.fxKnobSize
    readonly property int onKeyWidth: compact ? Math.max(Math.round(28 * Theme.uiScale), Theme.fontSizeXs * 2 + 10) : Theme.fxOnKeyWidth
    readonly property int partialSpacing: compact ? 2 : Theme.space1
    readonly property int presetKeyWidth: Math.round(18 * Theme.uiScale)
    property bool showSuper: true
    readonly property int slotMinWidth: compact ? Math.round(56 * Theme.uiScale) : Theme.fxSlotMinWidth
    readonly property Mixxx.EffectUnitProxy unit: Mixxx.EffectsManager.getEffectUnit(unitNumber)
    readonly property bool unitEnabled: unitEnabledControl.value > 0
    required property int unitNumber

    implicitHeight: Theme.fxControlHeight
    spacing: partialSpacing

    // The FX badge is also the settings key for the unit's three slots: meta,
    // parameters and focus are one click away instead of taking strip width.
    Skin.Button {
        id: unitBadge

        Accessible.name: qsTr("Effect unit %1 controls").arg(root.unitNumber)
        Layout.preferredHeight: Theme.fxControlHeight
        Layout.preferredWidth: root.badgeWidth
        ToolTip.delay: 500
        ToolTip.text: qsTr("Effect unit %1 parameters").arg(root.unitNumber)
        ToolTip.visible: badgeHover.hovered
        activeColor: Theme.textOnAccent
        highlight: root.unitEnabled
        implicitWidth: root.badgeWidth
        text: qsTr("FX%1").arg(root.unitNumber)

        onClicked: controlsPopup.opened ? controlsPopup.close() : controlsPopup.open()

        HoverHandler {
            id: badgeHover

            cursorShape: Qt.PointingHandCursor
        }
    }
    // Unit power. The FX badge beside it is the settings key, so this dot is
    // the only switch for the chain.
    Item {
        Accessible.checkable: true
        Accessible.checked: root.unitEnabled
        Accessible.name: qsTr("Effect unit %1 enabled").arg(root.unitNumber)
        Accessible.role: Accessible.CheckBox
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: Theme.fxPowerDotSize
        Layout.preferredWidth: Theme.fxPowerDotSize
        ToolTip.delay: 500
        ToolTip.text: qsTr("Enable effect unit %1").arg(root.unitNumber)
        ToolTip.visible: powerHover.hovered

        Rectangle {
            anchors.fill: parent
            border.color: root.unitEnabled ? Theme.accent : Theme.borderStrong
            border.width: 1
            color: root.unitEnabled ? Theme.accent : "transparent"
            radius: width / 2
        }
        TapHandler {
            onTapped: unitEnabledControl.value = root.unitEnabled ? 0 : 1
        }
        HoverHandler {
            id: powerHover

            cursorShape: Qt.PointingHandCursor
        }
    }
    // Deck assignment. Each key toggles independently, so one chain can feed
    // any combination of the four decks (Rekordbox-style multi-select).
    Row {
        id: deckAssign

        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: Theme.fxControlHeight
        Layout.preferredWidth: 4 * root.assignKeyWidth + 3 * root.partialSpacing
        spacing: root.partialSpacing

        Repeater {
            model: 4

            Skin.ControlButton {
                required property int index

                Accessible.name: qsTr("Assign effect unit %1 to deck %2").arg(root.unitNumber).arg(String.fromCharCode(65 + index))
                activeColor: Theme.textOnAccent
                group: root.group
                height: Theme.fxControlHeight
                key: "group_[Channel" + (index + 1) + "]_enable"
                normalColor: [Theme.deckA, Theme.deckB, Theme.deckC, Theme.deckD][index]
                text: String.fromCharCode(65 + index)
                toggleable: true
                width: root.assignKeyWidth
            }
        }
    }
    Repeater {
        model: 3

        RowLayout {
            id: slotRow

            required property int index
            readonly property Mixxx.EffectSlotProxy slot: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, slotRow.index + 1)

            Layout.fillWidth: true
            Layout.minimumWidth: root.slotMinWidth + root.onKeyWidth + root.partialSpacing
            spacing: root.partialSpacing

            EffectSelector {
                Accessible.name: qsTr("Effect unit %1 slot %2").arg(root.unitNumber).arg(slotRow.index + 1)
                Layout.fillWidth: true
                Layout.minimumWidth: root.slotMinWidth
                font.pixelSize: Theme.fontSizeXs
                implicitHeight: Theme.fxControlHeight
                slot: slotRow.slot
            }
            Skin.ControlButton {
                Accessible.name: qsTr("Effect unit %1 slot %2 enabled").arg(root.unitNumber).arg(slotRow.index + 1)
                Layout.preferredWidth: root.onKeyWidth
                activeColor: Theme.textOnAccent
                group: slotRow.slot.group
                implicitHeight: Theme.fxControlHeight
                key: "enabled"
                text: qsTr("On")
                toggleable: true
            }
        }
    }
    Skin.ControlKnob {
        id: mixKnob

        Accessible.name: qsTr("Effect unit %1 dry/wet").arg(root.unitNumber)
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: root.knobSize
        Layout.preferredWidth: root.knobSize
        ToolTip.delay: 500
        ToolTip.text: qsTr("Dry/wet")
        ToolTip.visible: mixHover.hovered
        arcStart: Skin.Knob.ArcStart.Minimum
        color: Theme.textPrimary
        group: root.group
        key: "mix"

        HoverHandler {
            id: mixHover
        }
    }
    Skin.ControlKnob {
        id: superKnob

        Accessible.name: qsTr("Effect unit %1 super knob").arg(root.unitNumber)
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: root.knobSize
        Layout.preferredWidth: root.knobSize
        ToolTip.delay: 500
        ToolTip.text: qsTr("Super knob")
        ToolTip.visible: superHover.hovered
        arcStart: Skin.Knob.ArcStart.Minimum
        color: Theme.textSecondary
        group: root.group
        key: "super1"
        visible: root.showSuper

        HoverHandler {
            id: superHover
        }
    }
    Skin.Button {
        id: presetButton

        Accessible.name: qsTr("Effect unit %1 presets").arg(root.unitNumber)
        Layout.preferredHeight: Theme.fxControlHeight
        Layout.preferredWidth: root.presetKeyWidth
        ToolTip.delay: 500
        ToolTip.text: qsTr("Chain presets")
        ToolTip.visible: presetHover.hovered
        icon.height: Math.round(12 * Theme.uiScale)
        icon.source: "images/gear.svg"
        icon.width: Math.round(12 * Theme.uiScale)

        onClicked: chainPresetMenu.opened ? chainPresetMenu.close() : chainPresetMenu.open()

        HoverHandler {
            id: presetHover
        }
    }
    Popup {
        id: controlsPopup

        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        dim: false
        focus: true
        modal: false
        padding: Theme.space2
        x: Math.min(0, root.width - width)
        y: root.height + Theme.space1

        background: Rectangle {
            border.color: Theme.border
            border.width: 1
            color: Theme.surface1
            radius: Theme.radiusMd
        }
        contentItem: RowLayout {
            spacing: Theme.space3

            Repeater {
                model: 3

                EffectSlotControls {
                    id: slotControls

                    required property int index

                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: slotControls.columnWidth
                    slot: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, index + 1)
                    unitGroup: root.group
                }
            }
        }
    }
    EffectChainPresetPopup {
        id: chainPresetMenu

        slot1: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 1)
        slot2: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 2)
        slot3: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 3)
        unit: root.unit
        x: Math.min(0, root.width - width)
        y: root.height + Theme.space1
    }
    Mixxx.ControlProxy {
        id: unitEnabledControl

        group: root.group
        key: "enabled"
    }
}
