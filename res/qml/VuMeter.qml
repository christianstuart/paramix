import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

/// Flat, segmented level meter.
///
/// The meter is a dim rail of rounded segments that light up from the bottom
/// in the usual green / yellow / red order. Compared to the legacy gradient
/// + OpacityMask implementation this is cheaper to repaint and matches the
/// flat panel language of the theme.
Rectangle {
    id: root

    required property string group
    required property string key

    /// Normalised level, 0.0 - 1.0.
    readonly property real level: Math.max(0, Math.min(1, control.parameter))
    /// One segment per 4 px of height keeps the meter readable at any size.
    readonly property int segmentCount: Math.max(1, Math.floor(height / 4))
    readonly property int segmentSpacing: 1

    border.color: Theme.border
    border.width: 1
    color: Theme.surface0
    radius: Theme.radiusXs

    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key
    }

    Item {
        id: segments

        anchors.fill: parent
        anchors.margins: 1
        clip: true

        Repeater {
            model: root.segmentCount

            Rectangle {
                required property int index

                // index 0 is the top segment, the level fills from the bottom.
                readonly property real position: 1 - (index + 0.5) / root.segmentCount
                readonly property bool active: position < root.level
                readonly property color activeColor: position > 0.9
                        ? Theme.danger
                        : position > 0.75
                          ? Theme.warning
                          : Theme.positive

                color: active ? activeColor : Theme.surface2
                height: Math.max(1, (segments.height - (root.segmentCount - 1) * root.segmentSpacing) / root.segmentCount)
                radius: 1
                width: segments.width
                y: index * (height + root.segmentSpacing)
            }
        }
    }
}
