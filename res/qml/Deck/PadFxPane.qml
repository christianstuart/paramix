pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Layouts
import "../Theme"

/// Deck pane tab: the shared Pad FX assignment as an 8-pad 4 x 2 grid.
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
        anchors.bottomMargin: Theme.space2
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        anchors.topMargin: Theme.space2
        columnSpacing: Theme.space1
        columns: 4
        rowSpacing: Theme.space1

        Repeater {
            model: 8

            delegate: Skin.PadFxPadFace {
                required property int index

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: 0
                Layout.minimumWidth: 0
                bank: root.bank
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
