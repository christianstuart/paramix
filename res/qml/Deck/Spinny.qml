import ".." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

/// Platter with concentric rings, a rotating position tick and the live BPM /
/// pitch readout in the centre.
///
/// The platter is deliberately matte: flat surface tokens, 1 px hairlines and
/// no glow. The tick and hub only brighten when a track is loaded, so the
/// control reads as idle hardware rather than a lit ornament.
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

        anchors.centerIn: parent
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface2
        height: Math.min(root.width, root.height)
        radius: width / 2
        width: height

        Rectangle {
            anchors.fill: parent
            anchors.margins: Math.max(3, Math.round(spinner.width * 0.04))
            border.color: Theme.border
            border.width: 1
            color: "transparent"
            radius: width / 2
        }
        Rectangle {
            id: hub

            anchors.centerIn: parent
            border.color: Theme.border
            border.width: 1
            color: Theme.surface0
            height: Math.round(spinner.width * 0.46)
            opacity: trackLoadedControl.value ? 1 : 0.75
            radius: width / 2
            width: height

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.motionNormal
                }
            }
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
                    anchors.topMargin: Math.round(spinner.width * 0.1)
                    color: trackLoadedControl.value ? Theme.textSecondary : Theme.textMuted
                    height: Math.round(spinner.width * 0.11)
                    radius: 1
                    width: 2

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.motionFast
                        }
                    }
                }
            }
        }
    }
    Column {
        id: tempoInfo

        anchors.centerIn: spinner
        opacity: trackLoadedControl.value ? 1 : 0
        spacing: 1

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionNormal
            }
        }

        Label {
            id: tempo

            readonly property real bpm: bpmControl.value
            readonly property int precision: 2

            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textPrimary
            font.family: Theme.fontFamilyMono
            // The hub shrinks with the platter, so the readout drops a size.
            font.pixelSize: spinner.width >= 140 ? Theme.fontSizeLg : Theme.fontSizeMd
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
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

            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textMuted
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeXs
            horizontalAlignment: Text.AlignHCenter
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
