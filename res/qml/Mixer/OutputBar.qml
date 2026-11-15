import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "../Theme"

/// Master and headphone controls for the mixer.
///
/// The narrow mixer column cannot fit these without clipping, so they live in a
/// full-width bar directly below the deck/mixer row: main output (gain,
/// balance, booth) and headphones (gain, cue mix, split) with the stereo meter
/// on the right.
Item {
    id: root

    property bool show4decks: false

    implicitHeight: 62

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
    RowLayout {
        anchors.fill: parent
        anchors.bottomMargin: Theme.space1
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        anchors.topMargin: Theme.space1
        spacing: Theme.space2

        CrossfaderUnit {
            Layout.fillHeight: true
            Layout.minimumWidth: 220
            Layout.preferredWidth: 340
            show4decks: root.show4decks
        }
        Rectangle {
            Layout.bottomMargin: Theme.space1
            Layout.fillHeight: true
            Layout.topMargin: Theme.space1
            color: Theme.border
            implicitWidth: 1
        }
        OutputTile {
            Layout.preferredHeight: 54
            Layout.preferredWidth: 64
            group: "[Master]"
            key: "gain"
            label: qsTr("MAIN")
            readoutMode: "gain"
        }
        OutputTile {
            Layout.preferredHeight: 54
            Layout.preferredWidth: 64
            group: "[Master]"
            key: "balance"
            label: qsTr("BAL")
            readoutMode: "balance"
        }
        OutputTile {
            Layout.preferredHeight: 54
            Layout.preferredWidth: 64
            enabled: boothEnabledControl.value > 0
            group: "[Master]"
            key: "booth_gain"
            label: qsTr("BOOTH")
            opacity: enabled ? 1 : 0.45
            readoutMode: "gain"
        }
        Rectangle {
            Layout.bottomMargin: Theme.space1
            Layout.fillHeight: true
            Layout.topMargin: Theme.space1
            color: Theme.border
            implicitWidth: 1
        }
        OutputTile {
            Layout.preferredHeight: 54
            Layout.preferredWidth: 64
            group: "[Master]"
            key: "headGain"
            label: qsTr("HEAD")
            readoutMode: "gain"
        }
        OutputTile {
            Layout.preferredHeight: 54
            Layout.preferredWidth: 64
            group: "[Master]"
            key: "headMix"
            label: qsTr("MIX")
            readoutMode: "mix"
        }
        Skin.ControlButton {
            Layout.preferredHeight: 54
            Layout.preferredWidth: 64
            activeColor: Theme.textOnAccent
            group: "[Master]"
            key: "headSplit"
            text: qsTr("Split")
            toggleable: true
        }
        Item {
            Layout.fillWidth: true
        }
        Item {
            Layout.preferredHeight: 54
            Layout.preferredWidth: 10

            Row {
                anchors.fill: parent
                spacing: 2

                Column {
                    height: parent.height
                    spacing: 2
                    width: 4

                    Rectangle {
                        color: peakLeft.value > 0 ? Theme.vuRed : Theme.surface2
                        height: 3
                        radius: 1
                        width: 4
                    }
                    Skin.VuMeter {
                        group: "[Main]"
                        height: Math.max(0, parent.height - 5)
                        key: "vu_meter_left"
                        width: 4
                    }
                }
                Column {
                    height: parent.height
                    spacing: 2
                    width: 4

                    Rectangle {
                        color: peakRight.value > 0 ? Theme.vuRed : Theme.surface2
                        height: 3
                        radius: 1
                        width: 4
                    }
                    Skin.VuMeter {
                        group: "[Main]"
                        height: Math.max(0, parent.height - 5)
                        key: "vu_meter_right"
                        width: 4
                    }
                }
            }
        }
    }
}
