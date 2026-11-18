pragma ComponentBehavior: Bound

import "Effects" as Effects
import QtQuick
import "Theme"

/// The effects strip under the main toolbar: the four standard effect chains.
/// Units 1 and 2 always fit; 3 and 4 appear once the strip is wide enough for
/// four compact units, which is what keeps the row readable at 1280.
Row {
    id: root

    // Width a compact unit needs: badge 33, power 12, deck keys 70, three
    // slot rows 258, dry/wet 20, presets 18 and the gaps 14 - about 425. Kept
    // in step with the `compact` sizes in TopBarEffectUnit.qml; the extra 10
    // leaves room for the selectors before four units are allowed in.
    readonly property int compactUnitWidth: 435
    readonly property bool showAllUnits: width >= 4 * compactUnitWidth + 3 * spacing
    readonly property int unitCount: showAllUnits ? 4 : 2
    readonly property real unitWidth: Math.max(0, (width - (unitCount - 1) * spacing) / unitCount)

    spacing: Theme.space2

    Repeater {
        model: root.unitCount

        Effects.TopBarEffectUnit {
            required property int index

            compact: root.showAllUnits
            height: root.height
            showSuper: !root.showAllUnits && root.width >= Theme.fxSuperMinWidth
            unitNumber: index + 1
            width: root.unitWidth
        }
    }
}
