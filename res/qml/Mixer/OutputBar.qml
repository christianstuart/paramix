import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "../Theme"

/// Master and headphone controls for the mixer.
///
/// The narrow mixer column cannot fit these without clipping, so they live in a
/// full-width bar directly below the deck/mixer row: the output knobs on the
/// far left, the crossfader centered in the bar, and a horizontal master level
/// meter on the right.
Item {
    id: root

    property bool show4decks: false

    readonly property int sideMargin: Theme.space3
    readonly property int tileWidth: root.width < 1200 ? 56 : 64
    readonly property int meterWidth: root.width < 1100 ? 130 : 190

    implicitHeight: 66

    Skin.SectionBackground {
        anchors.fill: parent
        radius: 0
    }
    Mixxx.ControlProxy {
        id: boothEnabledControl

        group: "[Master]"
        key: "booth_enabled"
    }
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

    // Output knobs, far left.
    Row {
        id: leftControls

        anchors.left: parent.left
        anchors.leftMargin: root.sideMargin
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2

        OutputTile {
            height: 54
            group: "[Master]"
            key: "gain"
            label: qsTr("MAIN")
            readoutMode: "gain"
            width: root.tileWidth
        }
        OutputTile {
            height: 54
            group: "[Master]"
            key: "balance"
            label: qsTr("BAL")
            readoutMode: "balance"
            width: root.tileWidth
        }
        OutputTile {
            enabled: boothEnabledControl.value > 0
            group: "[Master]"
            height: 54
            key: "booth_gain"
            label: qsTr("BOOTH")
            opacity: enabled ? 1 : 0.45
            readoutMode: "gain"
            width: root.tileWidth
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.border
            height: 36
            width: 1
        }
        OutputTile {
            height: 54
            group: "[Master]"
            key: "headGain"
            label: qsTr("HEAD")
            readoutMode: "gain"
            width: root.tileWidth
        }
        OutputTile {
            height: 54
            group: "[Master]"
            key: "headMix"
            label: qsTr("MIX")
            readoutMode: "mix"
            width: root.tileWidth
        }
        Skin.ControlButton {
            anchors.verticalCenter: parent.verticalCenter
            group: "[Master]"
            height: 54
            key: "headSplit"
            text: qsTr("Split")
            toggleable: true
            width: root.tileWidth
        }
    }

    // Crossfader, centered in the bar.
    CrossfaderUnit {
        id: crossfaderUnit

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        height: 44
        show4decks: root.show4decks
        width: Math.max(160, Math.min(360,
                root.width - leftControls.width - vuBlock.width - 6 * root.sideMargin))
    }

    // Master level meter, horizontal and on the right.
    Item {
        id: vuBlock

        anchors.right: parent.right
        anchors.rightMargin: root.sideMargin
        anchors.verticalCenter: parent.verticalCenter
        height: 50
        width: root.meterWidth + 44

        Rectangle {
            anchors.fill: parent
            border.color: Theme.borderStrong
            border.width: 1
            color: Theme.surface1
            radius: Theme.radiusSm
        }
        Column {
            anchors.centerIn: parent
            spacing: 3

            Text {
                color: Theme.textMuted
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("MASTER")
            }
            Row {
                spacing: 4

                Text {
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    height: 12
                    text: qsTr("L")
                    verticalAlignment: Text.AlignVCenter
                    width: 8
                }
                HorizontalVuMeter {
                    group: "[Main]"
                    height: 14
                    key: "vu_meter_left"
                    width: root.meterWidth
                }
                Rectangle {
                    color: peakLeft.value > 0 ? Theme.vuRed : Theme.surface3
                    height: 8
                    radius: 1
                    width: 8

                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Row {
                spacing: 4

                Text {
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    height: 12
                    text: qsTr("R")
                    verticalAlignment: Text.AlignVCenter
                    width: 8
                }
                HorizontalVuMeter {
                    group: "[Main]"
                    height: 14
                    key: "vu_meter_right"
                    width: root.meterWidth
                }
                Rectangle {
                    color: peakRight.value > 0 ? Theme.vuRed : Theme.surface3
                    height: 8
                    radius: 1
                    width: 8

                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
