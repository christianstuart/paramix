import Qt5Compat.GraphicalEffects
import QtQuick
import QtQuick.Layouts
import "../Theme"

/// One track list cell. Owns the row background (selection, hover, zebra
/// stripes) and the drag image shown while dragging a track.
Rectangle {
    id: root

    readonly property alias dragImage: dragImageEffect
    /// The track table this cell belongs to. The table's drop handler uses it
    /// to tell an internal row drag from a drag that comes from outside Mixxx.
    readonly property var trackList: tableView

    Drag.dragType: Drag.Automatic
    Drag.mimeData: {
        // Dragging a selected row moves the whole selection.
        const selection = trackList ? trackList.selectionModel : null;
        let urls = [];
        if (selection && selected) {
            urls = selection.selectedTrackUrls();
        }
        if (urls.length === 0) {
            urls = [file_url.toString()];
        }
        return {
            "text/uri-list": urls.join("\n"),
            "text/plain": urls.join("\n")
        };
    }
    Drag.supportedActions: Qt.CopyAction
    anchors.fill: parent
    color: selected ? Qt.alpha(Theme.accent, 0.22) : hoverHandler.hovered ? Theme.surfaceHover : (row % 2 == 0 ? Theme.surface1 : Theme.surface0)

    Drag.onActiveChanged: {
        // Starting a drag on an unselected row acts on that row, like a press
        // in the classic table.
        if (!Drag.active || selected) {
            return;
        }
        if (trackList && trackList.selectionModel) {
            trackList.selectionModel.selectSingleton(row);
        }
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
    Item {
        id: dragImageSource

        height: 85
        visible: false
        width: 190

        Rectangle {
            anchors.fill: parent
            anchors.margins: Theme.space1
            border.color: Theme.border
            border.width: 1
            color: Theme.surface2
            radius: Theme.radiusMd

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.space2
                spacing: Theme.space2

                Image {
                    id: cover

                    Layout.fillHeight: true
                    Layout.preferredWidth: cover_art ? 60 : 0
                    asynchronous: true
                    clip: true
                    fillMode: Image.PreserveAspectFit
                    source: cover_art
                }
                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        Layout.fillWidth: true
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Theme.fontWeightDemiBold
                        text: track ? track.title : qsTr("Unknown title")
                    }
                    Text {
                        Layout.fillWidth: true
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        text: track ? track.artist : qsTr("Unknown artist")
                    }
                }
            }
        }
    }
    DropShadow {
        id: dragImageEffect

        anchors.fill: dragImageSource
        color: Qt.rgba(0, 0, 0, 0.5)
        horizontalOffset: 0
        radius: 10
        source: dragImageSource
        verticalOffset: 0
        visible: false
    }
}
