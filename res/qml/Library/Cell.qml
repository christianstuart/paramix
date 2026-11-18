import QtQuick
import "../Theme"

/// One track list cell. Owns the row background (selection, hover, zebra
/// stripes) and the drag payload. The drag image itself is shared by the list
/// (see TrackList.qml), so a viewport does not instantiate a preview per cell.
Rectangle {
    id: root

    Drag.dragType: Drag.Automatic
    Drag.mimeData: {
        // Use the track object instead of a separate model role. The list
        // delegate is reused for every column, so resolving the URL once per
        // cell would be repeated work for no benefit.
        const location = track ? track.trackLocationUrl.toString() : "";
        return {
            "text/uri-list": location,
            "text/plain": location
        };
    }
    Drag.supportedActions: Qt.CopyAction
    anchors.fill: parent
    color: selected ? Qt.alpha(Theme.accent, 0.22) : hoverHandler.hovered ? Theme.surfaceHover : (row % 2 == 0 ? Theme.surface1 : Theme.surface0)

    HoverHandler {
        id: hoverHandler
    }

    // Accent marker on the left edge of the selected row + subtle bottom key.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        color: Theme.accent
        visible: selected
        width: 3
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: selected ? Qt.alpha(Theme.accent, 0.35) : Theme.border
        height: 1
        opacity: selected ? 1.0 : 0.4
    }
}
