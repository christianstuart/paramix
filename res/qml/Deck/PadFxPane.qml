pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Layouts
import "../Theme"

/// Deck pane tab: the shared Pad FX assignment as 8 pads, 4 x 2 with
/// one-line faces on narrow panes and one row of eight on wide ones.
///
/// The active bank belongs to this deck; the pad assignments themselves are
/// shared through [PadFX], so every deck shows the same effects and targets.
Rectangle {
    id: root

    required property string group

    readonly property string padFxGroup: root.group.substr(0, root.group.length - 1) + "_PadFX]"
    readonly property string presetGroup: "[PadFX]"
    readonly property int bank: Math.max(1, Math.min(2,
            Math.round(bankControl.value)))
    readonly property bool oneRow: width >= Theme.deckPadRowMinWidth

    border.color: Theme.border
    border.width: 1
    clip: true
    color: Theme.surface1
    radius: Theme.radiusMd

    Mixxx.ControlProxy {
        id: bankControl

        group: root.padFxGroup
        key: "bank"
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: Theme.deckPaneTabGap
        columnSpacing: Theme.deckPaneTabGap
        // Same rule as the Cue pane: one row of eight once two-line faces fit.
        columns: root.oneRow ? 8 : 4
        rowSpacing: Theme.deckPaneTabGap

        Repeater {
            model: 8

            delegate: Skin.PadFxPadFace {
                required property int index

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: 0
                Layout.minimumWidth: 0
                bank: root.bank
                oneLine: !root.oneRow
                pad: index + 1
                padFxGroup: root.padFxGroup
                presetGroup: root.presetGroup

                onEditRequested: (pad, anchor) => padPopup.openFor(anchor, pad)
            }
        }
    }

    Skin.PadFxPadPopup {
        id: padPopup

        bank: root.bank
        padFxGroup: root.padFxGroup
        presetGroup: root.presetGroup
    }
}
