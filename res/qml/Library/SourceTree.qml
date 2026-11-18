import QtQuick
import Mixxx 1.0 as Mixxx
import "." as LibraryComponent
import "../Theme"

Mixxx.LibrarySourceTree {
    id: root

    defaultColumns: [
        Mixxx.TrackListColumn {
            autoHideWidth: 750
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Album
            preferredWidth: 100

            delegate: Rectangle {
                color: {
                    // Read the revision so a track colour edit triggers the
                    // invokable again even though this is not a model role.
                    const revision = tableView && tableView.model ? tableView.model.revision : 0;
                    return revision >= 0 && tableView ? tableView.model.getDecorationColor(row) : "transparent";
                }
                implicitHeight: 30

                Image {
                    anchors.fill: parent
                    asynchronous: true
                    clip: true
                    fillMode: Image.PreserveAspectCrop
                    // Ask the list model only for this cell. The row-level
                    // cover role used to be resolved by every column of every
                    // row, which made a viewport pay for metadata it never
                    // displayed.
                    source: {
                        const revision = tableView && tableView.model ? tableView.model.revision : 0;
                        return revision >= 0 && tableView ? tableView.model.getCoverArtUrl(row) : "";
                    }
                    // Decode only what this small cell can show. Without a
                    // sourceSize the provider decodes and keeps a full-size
                    // cover for every visible row.
                    sourceSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
                }
            }
        },
        // A "Preview" waveform-overview column was removed: WaveformOverview is
        // too slow without an image cache, as in the legacy UI.
        Mixxx.TrackListColumn {
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Title
            fillSpan: 3
            label: qsTr("Title")

            delegate: DefaultDelegate {
                bold: true
            }
        },
        Mixxx.TrackListColumn {
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Artist
            fillSpan: 2
            label: qsTr("Artist")

            delegate: DefaultDelegate {
                bold: true
            }
        },
        Mixxx.TrackListColumn {
            autoHideWidth: 690
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Album
            fillSpan: 1
            label: qsTr("Album")

            delegate: DefaultDelegate {
            }
        },
        Mixxx.TrackListColumn {
            autoHideWidth: 750
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Year
            label: qsTr("Year")
            preferredWidth: 80

            delegate: DefaultDelegate {
            }
        },
        Mixxx.TrackListColumn {
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Bpm
            label: qsTr("Bpm")
            preferredWidth: 60

            delegate: DefaultDelegate {
            }
        },
        Mixxx.TrackListColumn {
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Key
            label: qsTr("Key")
            preferredWidth: 70

            delegate: DefaultDelegate {
            }
        },
        // Stem state is not a sortable column. TrackList.qml draws a small
        // badge in the left gutter of the Title cell for rows whose track has
        // separated stems (model role `has_stems`), so no extra column here.
        Mixxx.TrackListColumn {
            autoHideWidth: 900
            columnIdx: Mixxx.TrackListColumn.SQLColumns.FileType
            label: qsTr("File Type")
            preferredWidth: 70

            delegate: DefaultDelegate {
            }
        },
        Mixxx.TrackListColumn {
            autoHideWidth: 1200
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Bitrate
            label: qsTr("Bitrate")
            preferredWidth: 70

            delegate: DefaultDelegate {
            }
        }
    ]

    component DefaultDelegate: LibraryComponent.Cell {
        id: cell

        /// Title and artist are the identifying columns; the host sets this so
        /// they read heavier than the metadata next to them.
        property bool bold: false

        readonly property var caps: capabilities

        // FIXME: https://bugreports.qt.io/browse/QTBUG-111789
        Binding on Drag.active {
            // This delays the update until the even queue is cleared
            // preventing any potential oscillations causing a loop
            delayed: true
            value: dragArea.drag.active
        }

        LibraryComponent.Track {
            id: dragArea

            anchors.fill: parent
            capabilities: cell.caps

            drag.onGrabChanged: (transition, eventPoint) => {
                if (transition != PointerDevice.GrabPassive && transition != PointerDevice.GrabExclusive) {
                    return;
                }
                trackList.captureDragPreview(track, url => {
                    parent.Drag.imageSource = url;
                });
            }
            tap.onDoubleTapped: {
                tableView.selectionModel.selectRow(row);
                tableView.loadSelectedTrackIntoNextAvailableDeck(false);
            }
            tap.onTapped: (eventPoint, button) => {
                if (button == Qt.LeftButton) {
                    tableView.selectionModel.selectRow(row);
                }
            }
        }
        Text {
            id: value

            anchors.fill: parent
            // Leave room for the stem badge that the track list draws in the
            // left gutter of the title cell (see TrackList.qml).
            anchors.leftMargin: Theme.space3 + (stemBadgeVisible ? 16 : 0)
            color: Theme.textPrimary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: cell.bold ? Theme.fontWeightBold : Theme.fontWeightRegular
            text: display ?? ""
            verticalAlignment: Text.AlignVCenter
        }
    }
}
