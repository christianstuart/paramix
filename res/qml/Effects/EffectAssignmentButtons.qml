pragma ComponentBehavior: Bound

import ".." as Skin
import QtQuick
import "../Theme"

/// Deck-side keys that assign a deck to one of the two effect units.
Row {
    id: root

    property color activeColor: Theme.textOnAccent
    required property string channelGroup
    property color inactiveColor: Theme.knobBackgroundColor

    Repeater {
        model: 2

        Skin.ControlButton {
            required property int index

            activeColor: root.activeColor
            group: "[EffectRack1_EffectUnit" + (index + 1) + "]"
            height: Theme.fxControlHeight
            key: "group_" + root.channelGroup + "_enable"
            text: qsTr("FX%1").arg(index + 1)
            toggleable: true
            width: Theme.fxUnitKeyMinWidth
        }
    }
}
