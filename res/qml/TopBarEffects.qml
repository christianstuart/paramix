pragma ComponentBehavior: Bound

import "Effects" as Effects
import QtQuick
import "Theme"

/// The effects strip under the main toolbar. It always shows standard effect
/// units 1 and 2: there is no paging and no width-dependent four-up mode, so
/// what is on screen never depends on the window width or on a hidden page
/// state. Units 3 and 4 live in EffectUnitsWindow.qml, opened from the
/// "FX 3-4" toolbar key or the View menu.
Row {
    id: root

    readonly property int assignKeyWidth: Math.max(Theme.fxAssignKeyWidth, Theme.fontSizeXs * 2 + 4)
    // Same key sizes as TopBarEffectUnit.qml: Skin.Button keeps 6 px of padding
    // per side, so a one-glyph key needs this much before its label elides.
    readonly property int badgeWidth: Math.max(Theme.fxUnitKeyMinWidth, Theme.fontSizeXs * 3 + 12)
    readonly property int presetKeyWidth: Math.max(Math.round(20 * Theme.uiScale), Theme.fontSizeXs * 2 + 4)
    readonly property int unitCount: 2
    // One unit at its natural size: badge, power, deck keys, three slot rows,
    // dry/wet, super and the preset key, plus the eight gaps between them.
    // The strip compares each unit against this before dropping the super
    // knob, so the freed width keeps that knob visible at the minimum window
    // size instead of at a hard-coded strip width.
    readonly property int unitMinWidth: badgeWidth + Theme.fxPowerDotSize + 4 * assignKeyWidth + 3 * Theme.space1 + 3 * (Theme.fxSlotMinWidth + Theme.fxOnKeyWidth + Theme.space1) + 2 * Theme.fxKnobSize + presetKeyWidth + 8 * Theme.space1
    readonly property real unitWidth: Math.max(0, (width - (root.unitCount - 1) * spacing) / root.unitCount)

    spacing: Theme.space2

    Repeater {
        model: root.unitCount

        Effects.TopBarEffectUnit {
            required property int index

            height: root.height
            showSuper: root.unitWidth >= root.unitMinWidth
            unitNumber: index + 1
            width: root.unitWidth
        }
    }
}
