import QtQuick
import "../Theme"

/// Interaction layer for a track list cell: drag and tap.
///
/// The context menu lives in TrackList.qml, which owns the selection and the
/// shared TrackMenu instance. Putting a menu here as well would open a second
/// popup for the same right-click, because each cell instantiates this layer.
Item {
    id: root

    required property var capabilities
    property alias drag: dragHandler
    property alias tap: tapHandler

    DragHandler {
        id: dragHandler

        target: value
    }
    TapHandler {
        id: tapHandler

        acceptedButtons: Qt.LeftButton | Qt.RightButton
    }
}
