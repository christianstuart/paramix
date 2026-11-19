pragma ComponentBehavior: Bound

import ".." as Skin
import QtQuick
import "../Theme"

/// Deck-side keys that assign a deck to one of the four effect units. The four
/// units are always present now that units 3 and 4 live in their own window, so
/// this no longer follows the classic [Skin],show_4effectunits display hint.
Row {
    id: root

    property color activeColor: Theme.textOnAccent
    required property string channelGroup
    property color inactiveColor: Theme.knobBackgroundColor

    Repeater {
        model: 4

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
