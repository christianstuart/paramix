import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme"

/// One standard effect chain in the top-bar strip: unit power, the three
/// effect selectors with their On keys, dry/wet and (on wide windows) super.
RowLayout {
    id: root

    required property int unitNumber
    property bool showSuper: true

    readonly property string group: unit.group
    readonly property Mixxx.EffectUnitProxy unit: Mixxx.EffectsManager.getEffectUnit(unitNumber)
    readonly property bool unitEnabled: unitEnabledControl.value > 0

    implicitHeight: Theme.fxControlHeight
    spacing: Theme.space1

    Rectangle {
        Layout.preferredHeight: Theme.fxControlHeight
        Layout.preferredWidth: Theme.fxUnitKeyMinWidth
        border.color: root.unitEnabled ? Qt.lighter(Theme.accent, 1.12) : Theme.border
        border.width: 1
        color: root.unitEnabled ? Theme.accent : Theme.surface2
        radius: Theme.radiusSm

        Accessible.name: qsTr("Effect unit %1").arg(root.unitNumber)

        Text {
            anchors.fill: parent
            color: root.unitEnabled ? Theme.textOnAccent : Theme.textSecondary
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("FX%1").arg(root.unitNumber)
            verticalAlignment: Text.AlignVCenter
        }
    }
    // Unit power. The FX badge beside it is a status label, so this dot is the
    // only switch for the chain.
    Item {
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: Theme.fxPowerDotSize
        Layout.preferredWidth: Theme.fxPowerDotSize

        Accessible.checkable: true
        Accessible.checked: root.unitEnabled
        Accessible.name: qsTr("Effect unit %1 enabled").arg(root.unitNumber)
        Accessible.role: Accessible.CheckBox

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
        ToolTip.delay: 500
        ToolTip.text: qsTr("Enable effect unit %1").arg(root.unitNumber)
        ToolTip.visible: powerHover.hovered
    }
    // Deck assignment. Each key toggles independently, so one chain can feed
    // any combination of the four decks (Rekordbox-style multi-select).
    Row {
        id: deckAssign

        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: Theme.fxControlHeight
        Layout.preferredWidth: 4 * Theme.fxAssignKeyWidth + 3 * Theme.space1
        spacing: Theme.space1

        Repeater {
            model: 4

            Skin.ControlButton {
                required property int index

                activeColor: Theme.textOnAccent
                group: root.group
                height: Theme.fxControlHeight
                key: "group_[Channel" + (index + 1) + "]_enable"
                normalColor: [Theme.deckA, Theme.deckB, Theme.deckC, Theme.deckD][index]
                text: String.fromCharCode(65 + index)
                toggleable: true
                width: Theme.fxAssignKeyWidth

                Accessible.name: qsTr("Assign effect unit %1 to deck %2")
                                 .arg(root.unitNumber)
                                 .arg(String.fromCharCode(65 + index))
            }
        }
    }
    RowLayout {
        id: slot1

        Layout.fillWidth: true
        Layout.minimumWidth: Theme.fxSlotMinWidth + Theme.fxOnKeyWidth + Theme.space1
        spacing: Theme.space1

        readonly property Mixxx.EffectSlotProxy slot: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 1)

        EffectSelector {
            Layout.fillWidth: true
            Layout.minimumWidth: Theme.fxSlotMinWidth
            implicitHeight: Theme.fxControlHeight
            font.pixelSize: Theme.fontSizeXs
            slot: slot1.slot
        }
        Skin.ControlButton {
            activeColor: Theme.textOnAccent
            implicitHeight: Theme.fxControlHeight
            group: slot1.slot.group
            key: "enabled"
            text: qsTr("On")
            toggleable: true

            Layout.preferredWidth: Theme.fxOnKeyWidth
        }
    }
    RowLayout {
        id: slot2

        Layout.fillWidth: true
        Layout.minimumWidth: Theme.fxSlotMinWidth + Theme.fxOnKeyWidth + Theme.space1
        spacing: Theme.space1

        readonly property Mixxx.EffectSlotProxy slot: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 2)

        EffectSelector {
            Layout.fillWidth: true
            Layout.minimumWidth: Theme.fxSlotMinWidth
            implicitHeight: Theme.fxControlHeight
            font.pixelSize: Theme.fontSizeXs
            slot: slot2.slot
        }
        Skin.ControlButton {
            activeColor: Theme.textOnAccent
            implicitHeight: Theme.fxControlHeight
            group: slot2.slot.group
            key: "enabled"
            text: qsTr("On")
            toggleable: true

            Layout.preferredWidth: Theme.fxOnKeyWidth
        }
    }
    RowLayout {
        id: slot3

        Layout.fillWidth: true
        Layout.minimumWidth: Theme.fxSlotMinWidth + Theme.fxOnKeyWidth + Theme.space1
        spacing: Theme.space1

        readonly property Mixxx.EffectSlotProxy slot: Mixxx.EffectsManager.getEffectSlot(root.unitNumber, 3)

        EffectSelector {
            Layout.fillWidth: true
            Layout.minimumWidth: Theme.fxSlotMinWidth
            implicitHeight: Theme.fxControlHeight
            font.pixelSize: Theme.fontSizeXs
            slot: slot3.slot
        }
        Skin.ControlButton {
            activeColor: Theme.textOnAccent
            implicitHeight: Theme.fxControlHeight
            group: slot3.slot.group
            key: "enabled"
            text: qsTr("On")
            toggleable: true

            Layout.preferredWidth: Theme.fxOnKeyWidth
        }
    }
    Skin.ControlKnob {
        id: mixKnob

        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: Theme.fxKnobSize
        Layout.preferredWidth: Theme.fxKnobSize
        arcStart: Skin.Knob.ArcStart.Minimum
        color: Theme.textPrimary
        group: root.group
        key: "mix"

        Accessible.name: qsTr("Effect unit %1 dry/wet").arg(root.unitNumber)

        HoverHandler {
            id: mixHover
        }
        ToolTip.delay: 500
        ToolTip.text: qsTr("Dry/wet")
        ToolTip.visible: mixHover.hovered
    }
    Skin.ControlKnob {
        id: superKnob

        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: Theme.fxKnobSize
        Layout.preferredWidth: Theme.fxKnobSize
        arcStart: Skin.Knob.ArcStart.Minimum
        color: Theme.textSecondary
        group: root.group
        key: "super1"
        visible: root.showSuper

        Accessible.name: qsTr("Effect unit %1 super knob").arg(root.unitNumber)

        HoverHandler {
            id: superHover
        }
        ToolTip.delay: 500
        ToolTip.text: qsTr("Super knob")
        ToolTip.visible: superHover.hovered
    }
    Mixxx.ControlProxy {
        id: unitEnabledControl

        group: root.group
        key: "enabled"
    }
}
