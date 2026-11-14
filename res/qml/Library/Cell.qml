import Qt5Compat.GraphicalEffects
import QtQuick
import QtQuick.Layouts
import "../Theme"

/// One track list cell. Owns the row background (selection, hover, zebra
/// stripes) and the drag image shown while dragging a track.
Rectangle {
    id: root

    readonly property alias dragImage: dragImageEffect

    Drag.dragType: Drag.Automatic
    Drag.mimeData: {
        "text/uri-list": file_url.toString(),
        "text/plain": file_url.toString()
    }
    Drag.supportedActions: Qt.CopyAction
    anchors.fill: parent
    color: selected ? Qt.alpha(Theme.accent, 0.2) : hoverHandler.hovered ? Theme.surface2 : (row % 2 == 0 ? Theme.surface1 : Theme.surface0)

    HoverHandler {
        id: hoverHandler
    }

    // Accent marker on the left edge of the selected row.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        color: Theme.accent
        visible: selected
        width: 2
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
