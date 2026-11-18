pragma ComponentBehavior: Bound

import "Effects" as Effects
import "." as Skin
import QtQuick
import QtQuick.Controls
import "Theme"

/// The effects strip under the main toolbar. Two standard effect chains are
/// visible at a time; the key at the end pages to units 3 and 4, so every unit
/// is reachable at 1280 without squeezing the effect names. All four only sit
/// side by side when the row is wide enough for four readable units.
///
/// Paging is local by design: [Skin],show_4effectunits is a classic-skin
/// control that the QML UI never creates, so binding to it would leave the page
/// key dead, and the strip also has to page automatically when it is narrow.
Row {
    id: root

    readonly property int assignKeyWidth: Math.max(Theme.fxAssignKeyWidth, Theme.fontSizeXs * 2 + 4)
    // Same key sizes as TopBarEffectUnit.qml: Skin.Button keeps 6 px of padding
    // per side, so a one-glyph key needs this much before its label elides.
    readonly property int badgeWidth: Math.max(Theme.fxUnitKeyMinWidth, Theme.fontSizeXs * 3 + 12)
    readonly property int firstUnitNumber: showAllUnits ? 1 : root.page * 2 + 1
    // 0 = units 1-2, 1 = units 3-4. Ignored while all four are shown.
    property int page: 0
    readonly property int presetKeyWidth: Math.max(Math.round(20 * Theme.uiScale), Theme.fontSizeXs * 2 + 4)
    // Leave each of the four units 3 x space2 for the selectors' glyph area.
    readonly property bool showAllUnits: width >= 4 * (unitMinWidth + 3 * Theme.space2) + 3 * spacing
    readonly property int unitCount: showAllUnits ? 4 : 2
    // One unit at its natural size: badge, power, deck keys, three slot rows,
    // dry/wet, super and the preset key, plus the eight gaps between them.
    readonly property int unitMinWidth: badgeWidth + Theme.fxPowerDotSize + 4 * assignKeyWidth + 3 * Theme.space1 + 3 * (Theme.fxSlotMinWidth + Theme.fxOnKeyWidth + Theme.space1) + 2 * Theme.fxKnobSize + presetKeyWidth + 8 * Theme.space1
    readonly property real unitWidth: Math.max(0, (width - (unitCount - 1) * spacing - (showAllUnits ? 0 : pageKey.implicitWidth + spacing)) / unitCount)

    spacing: Theme.space2

    Repeater {
        model: root.unitCount

        Effects.TopBarEffectUnit {
            required property int index

            height: root.height
            showSuper: root.width >= Theme.fxSuperMinWidth
            unitNumber: root.firstUnitNumber + index
            width: root.unitWidth
        }
    }
    Skin.Button {
        id: pageKey

        Accessible.name: root.page === 0 ? qsTr("Show effect units 3 and 4") : qsTr("Show effect units 1 and 2")
        ToolTip.delay: 500
        ToolTip.text: Accessible.name
        ToolTip.visible: pageHover.hovered
        implicitHeight: Theme.fxControlHeight
        text: root.page === 0 ? qsTr("FX 3-4") : qsTr("FX 1-2")
        visible: !root.showAllUnits

        onClicked: root.page = root.page === 0 ? 1 : 0

        HoverHandler {
            id: pageHover
        }
    }
}
