pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme"

/// One standard effect chain in the top-bar strip: unit power, the three
/// effect selectors with their On keys, dry/wet, super, the chain preset key
/// and the slot settings flyout behind the FX badge.
RowLayout {
    id: root

    readonly property int assignKeyWidth: Math.max(Theme.fxAssignKeyWidth, Theme.fontSizeXs * 2 + 4)
    // Skin.Button keeps 6 px of padding on each side, so a key needs this much
    // before its single glyph starts to elide.
    readonly property int badgeWidth: Math.max(Theme.fxUnitKeyMinWidth, Theme.fontSizeXs * 3 + 12)
    readonly property string group: unit.group
    readonly property int presetKeyWidth: Math.max(Math.round(20 * Theme.uiScale), Theme.fontSizeXs * 2 + 4)
    // Flyouts open as native popup windows when the unit is used outside the
    // main window, so a small EffectUnitsWindow cannot clip the parameter
    // columns or the effect list.
    property bool popupAsWindow: false
    property bool showSuper: true
    readonly property Mixxx.EffectUnitProxy unit: Mixxx.EffectsManager.getEffectUnit(unitNumber)
    readonly property bool unitEnabled: unitEnabledControl.value > 0
    required property int unitNumber

    implicitHeight: Theme.fxControlHeight
    spacing: Theme.space1

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
        Layout.preferredWidth: 4 * root.assignKeyWidth + 3 * Theme.space1
        spacing: Theme.space1

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
                ToolTip.delay: 500
                ToolTip.text: Accessible.name
                ToolTip.visible: hovered

                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
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
            Layout.minimumWidth: Theme.fxSlotMinWidth + Theme.fxOnKeyWidth + Theme.space1
            spacing: Theme.space1

            EffectSelector {
                Accessible.name: qsTr("Effect unit %1 slot %2").arg(root.unitNumber).arg(slotRow.index + 1)
                Layout.fillWidth: true
                Layout.minimumWidth: Theme.fxSlotMinWidth
                // Pin the preferred width to the minimum so every selector in
                // every unit grows by the same share; otherwise the layout
                // shrinks units with longer effect names more than short ones.
                Layout.preferredWidth: Theme.fxSlotMinWidth
                font.pixelSize: Theme.fontSizeXs
                implicitHeight: Theme.fxControlHeight
                popupAsWindow: root.popupAsWindow
                slot: slotRow.slot
            }
            Skin.ControlButton {
                Accessible.name: qsTr("Effect unit %1 slot %2 enabled").arg(root.unitNumber).arg(slotRow.index + 1)
                Layout.preferredWidth: Theme.fxOnKeyWidth
                ToolTip.delay: 500
                ToolTip.text: Accessible.name
                ToolTip.visible: hovered
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
        Layout.preferredHeight: Theme.fxKnobSize
        Layout.preferredWidth: Theme.fxKnobSize
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
        Layout.preferredHeight: Theme.fxKnobSize
        Layout.preferredWidth: Theme.fxKnobSize
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
        // Qt.resolvedUrl keeps the lookup relative to this file; a plain
        // string would be resolved by Skin.Button against res/qml instead.
        icon.source: Qt.resolvedUrl("../images/gear.svg")
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
        // In the effects window the unit sits in a small window; a native popup
        // can extend past it instead of being clipped.
        popupType: root.popupAsWindow ? Popup.Window : Popup.Item
        x: Math.max(0, root.width - width)
        y: root.height + Theme.space1

        background: Rectangle {
            border.color: Theme.border
            border.width: 1
            color: Theme.surface1
            radius: Theme.radiusMd
        }
        // Only build the three parameter columns while the flyout is open, so
        // four units do not instantiate 3 x parametersModel rows at startup.
        contentItem: Loader {
            active: controlsPopup.visible

            sourceComponent: RowLayout {
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
    }
    EffectChainPresetPopup {
        id: chainPresetMenu

        slot1: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 1)
        slot2: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 2)
        slot3: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 3)
        unit: root.unit
        x: Math.max(0, root.width - width)
        y: root.height + Theme.space1
    }
    Mixxx.ControlProxy {
        id: unitEnabledControl

        group: root.group
        key: "enabled"
    }
}
