import ".." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

/// Platter with concentric rings and a rotating position tick. The BPM lives
/// in the tempo strip beside it.
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
}
