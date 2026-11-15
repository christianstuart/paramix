import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

/// Horizontal segmented level meter for the master bar.
///
/// Same ladder language as the channel meters, but the segments rise from the
/// left and the rail is wider so the output level reads at a glance from
/// across the booth.
Rectangle {
    id: root

    required property string group
    required property string key
    /// Draw the active segments in neutral grey, for a meter that is live but
    /// not currently in use (e.g. the recording ladder while stopped).
    property bool muted: false

    /// Normalised level, 0.0 - 1.0.
    readonly property real level: Math.max(0, Math.min(1, control.parameter))
    /// One segment per 4 px of width keeps the meter readable at any size.
    readonly property int segmentCount: Math.max(1, Math.floor(width / 4))
    readonly property int segmentSpacing: 1
    readonly property real segmentWidth: Math.max(1, (segments.width - (root.segmentCount - 1) * root.segmentSpacing) / root.segmentCount)

    antialiasing: true
    border.color: Theme.borderStrong
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

                // index 0 is the left segment, the level fills to the right.
                readonly property real position: (index + 0.5) / root.segmentCount
                readonly property bool active: position <= root.level
                readonly property color activeColor: root.muted
                        ? Qt.alpha(Theme.textMuted, 0.55)
                        : position > 0.9
                          ? Theme.vuRed
                          : position > 0.75
                            ? Theme.vuYellow
                            : Theme.vuGreen

                antialiasing: false
                color: active ? activeColor : Theme.surface3
                height: segments.height
                radius: 1
                width: root.segmentWidth
                x: index * (width + root.segmentSpacing)
            }
        }
    }
}
