import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "Theme"
import "Effects" as Effects

/// Two rows of effect units, split by a hairline so each half reads as one
/// bank.
Item {
    id: root

    implicitHeight: columns.height

    Rectangle {
        anchors.fill: parent
        border.color: Theme.border
        border.width: 1
        color: Theme.surface1
        radius: Theme.radiusMd
    }
    Row {
        id: columns

        width: root.width

        Column {
            width: parent.width / 2

            Effects.EffectUnit {
                unitNumber: 1
                width: parent.width
            }
            Effects.EffectUnit {
                unitNumber: 3
                visible: showFourUnitsControl.value > 0
                width: parent.width
            }
        }
        Column {
            width: parent.width / 2

            Effects.EffectUnit {
                unitNumber: 2
                width: parent.width
            }
            Effects.EffectUnit {
                unitNumber: 4
                visible: showFourUnitsControl.value > 0
                width: parent.width
            }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        color: Theme.border
        width: 1
    }
    Mixxx.ControlProxy {
        id: showFourUnitsControl

        group: "[Skin]"
        key: "show_4effectunits"
    }
}
