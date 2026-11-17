import "Effects" as Effects
import QtQuick
import "Theme"

/// The effects strip under the main toolbar: the two standard effect chains,
/// each assignable to any combination of the four decks. A plain Row with an
/// explicit half width keeps the two chains equal even when one has effects
/// loaded and the other is empty.
Row {
    id: root

    readonly property bool showSuper: width >= Theme.fxSuperMinWidth

    spacing: Theme.space2

    Effects.TopBarEffectUnit {
        height: root.height
        showSuper: root.showSuper
        unitNumber: 1
        width: Math.max(0, (root.width - root.spacing) / 2)
    }
    Effects.TopBarEffectUnit {
        height: root.height
        showSuper: root.showSuper
        unitNumber: 2
        width: Math.max(0, (root.width - root.spacing) / 2)
    }
}
