import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

/// Master output level for the main toolbar.
///
/// Framed to the same size as the recording box so the pair reads as a set:
/// "OUT" on the left, the L/R ladder starting from the same content column the
/// recording box uses, and peak dots at the end of each row.
Item {
    id: root

    implicitHeight: 38
    implicitWidth: 216

    Mixxx.ControlProxy {
        id: peakLeft

        group: "[Main]"
        key: "peak_indicator_left"
    }
    Mixxx.ControlProxy {
        id: peakRight

        group: "[Main]"
        key: "peak_indicator_right"
    }

    Rectangle {
        anchors.fill: parent
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface2
        radius: Theme.radiusSm
    }

    Text {
        id: legend

        anchors.left: parent.left
        anchors.leftMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.textMuted
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        text: qsTr("Out")
    }

    // ── L/R ladder ─────────────────────────────────────────────────────────
    Column {
        id: ladder

        anchors.left: legend.right
        anchors.leftMargin: Theme.space4
        anchors.right: gainColumn.left
        anchors.rightMargin: Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3

        Row {
            id: rowL

            height: 8
            spacing: 3

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                height: 8
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("L")
                verticalAlignment: Text.AlignVCenter
                width: 8
            }
            HorizontalVuMeter {
                group: "[Main]"
                height: 8
                key: "vu_meter_left"
                width: ladder.width - 8 - 3 - 3 - 6
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: peakLeft.value > 0 ? Theme.vuRed : Theme.surface3
                height: 6
                radius: 1
                width: 6
            }
        }
        Row {
            id: rowR

            height: 8
            spacing: 3

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                height: 8
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("R")
                verticalAlignment: Text.AlignVCenter
                width: 8
            }
            HorizontalVuMeter {
                group: "[Main]"
                height: 8
                key: "vu_meter_right"
                width: ladder.width - 8 - 3 - 3 - 6
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: peakRight.value > 0 ? Theme.vuRed : Theme.surface3
                height: 6
                radius: 1
                width: 6
            }
        }
    }

    // ── Gain dial on the far right, matching the recording box ─────────────
    Item {
        id: gainColumn

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.top: parent.top
        anchors.topMargin: 1
        width: 28

        Text {
            id: gainLabel

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            color: Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: qsTr("Gain")
        }
        Skin.ControlKnob {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textPrimary
            group: "[Master]"
            height: 24
            key: "gain"
            width: 24

            Accessible.name: qsTr("Master output gain")
        }
    }
}
