import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

/// Flat, segmented level meter.
///
/// Rekordbox ladder: dim recessed rail, lit segments rising green / amber /
/// red with 1 px gaps, rounded 1 px caps. The rail sits flush beside a fader
/// so the strip reads as one hardware channel.
Rectangle {
    id: root

    required property string group
    required property string key

    /// Normalised level, 0.0 - 1.0.
    readonly property real level: Math.max(0, Math.min(1, control.parameter))
    /// One segment per 4 px of height keeps the meter readable at any size.
    readonly property int segmentCount: Math.max(1, Math.floor(height / 4))
    readonly property int segmentSpacing: 1
    readonly property real segmentHeight: Math.max(1, (segments.height - (root.segmentCount - 1) * root.segmentSpacing) / root.segmentCount)

    antialiasing: true
    border.color: Theme.border
    border.width: 1
    color: Theme.vuRail
    radius: 2

    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key
    }

    Item {
        id: segments

        anchors.fill: parent
        clip: true

        Repeater {
            model: root.segmentCount

            Rectangle {
                required property int index

                // index 0 is the top segment, the level fills from the bottom.
                readonly property real position: 1 - (index + 0.5) / root.segmentCount
                readonly property bool active: position < root.level
                readonly property color activeColor: position > 0.9
                        ? Theme.vuRed
                        : position > 0.75
                          ? Theme.vuYellow
                          : Theme.vuGreen

                antialiasing: false
                color: active ? activeColor : Theme.surface2
                height: root.segmentHeight
                radius: 1
                width: segments.width
                y: index * (height + root.segmentSpacing)
            }
        }
    }
}
