import QtQuick
import "../Theme"

/// One track list cell. Owns the row background (selection, hover, zebra
/// stripes) and the drag payload. The drag image itself is shared by the list
/// (see TrackList.qml), so a viewport does not instantiate a preview per cell.
Rectangle {
    id: root

    /// The track table this cell belongs to. The table's drop handler uses it
    /// to tell an internal row drag from a drag that comes from outside Mixxx.
    readonly property var trackList: tableView

    Drag.dragType: Drag.Automatic
    // The payload is built when the drag starts: selectedIndexes has no change
    // signal, so a binding here would keep the selection of the previous drag.
    Drag.mimeData: ({})
    Drag.supportedActions: Qt.CopyAction
    anchors.fill: parent
    color: selected ? Qt.alpha(Theme.accent, 0.22) : hoverHandler.hovered ? Theme.surfaceHover : (row % 2 == 0 ? Theme.surface1 : Theme.surface0)

    Drag.onActiveChanged: {
        if (!Drag.active) {
            return;
        }
        // Dragging an unselected row acts on that row, like a press in the
        // classic table.
        if (!selected && trackList && trackList.selectionModel) {
            trackList.selectionModel.selectSingleton(row);
        }
        const selection = trackList ? trackList.selectionModel : null;
        let urls = selection ? selection.selectedTrackUrls() : [];
        if (urls.length === 0) {
            urls = [track ? track.trackLocationUrl.toString() : ""];
        }
        Drag.mimeData = {
            "text/uri-list": urls.join("\n"),
            "text/plain": urls.join("\n")
        };
    }

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
