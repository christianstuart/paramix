import ".." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

/// Flat platter with the live BPM and pitch readout in the centre.
Item {
    id: root

    required property string group

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Rectangle {
        id: spinner

        anchors.top: parent.top
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface3
        height: 140
        radius: width / 2
        width: 140

        transform: Rotation {
            angle: 45
            origin.x: spinner.width / 2
            origin.y: spinner.height / 2
        }
        MixxxControls.Spinny {
            id: spinnyIndicator

            anchors.fill: parent
            group: root.group
            indicatorVisible: trackLoadedControl.value

            indicator: Item {
                height: spinnyIndicator.height
                width: spinnyIndicator.width

                Rectangle {
                    id: tick

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: Theme.surface0
                    height: 70
                    radius: 1
                    width: 2
                }
            }
        }
    }
    Rectangle {
        id: tempoInfo

        anchors.centerIn: spinner
        clip: true
        color: Theme.surface0
        height: trackLoadedControl.value ? spinner.height / 4 : 0
        radius: height / 2 - 5
        width: spinner.width / 2 - 10

        Behavior on height {
            SpringAnimation {
                id: heightAnimation

                damping: 0.2
                duration: 500
                spring: 2
            }
        }

        Label {
            id: tempo

            readonly property real bpm: bpmControl.value
            readonly property int precision: 2

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 2
            color: Theme.textPrimary
            font.bold: true
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeMd
            text: `${Math.round(bpm)}.${((bpm % 1) * Math.pow(10, precision)).toFixed().padEnd(precision, "0")}`

            Accessible.name: qsTr("%1 BPM").arg(tempo.text)
            Accessible.role: Accessible.StaticText

            Mixxx.ControlProxy {
                id: bpmControl

                group: root.group
                key: "bpm"
            }
        }
        Label {
            id: pitchRatio

            readonly property real ratio: ((rateRatioControl.value - 1) * 100).toPrecision(2)

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textSecondary
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeXs
            text: ((ratio > 0) ? "+" + ratio.toFixed(2) : ratio.toFixed(2)) + "%"

            Accessible.name: qsTr("Pitch %1").arg(pitchRatio.text)
            Accessible.role: Accessible.StaticText

            Mixxx.ControlProxy {
                id: rateRatioControl

                group: root.group
                key: "rate_ratio"
            }
        }
    }
}
