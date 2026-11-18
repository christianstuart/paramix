import QtQuick
import Mixxx 1.0 as Mixxx
import "." as LibraryComponent
import "../Theme"

Mixxx.LibrarySourceTree {
    id: root

    readonly property var sidebarProxy: root.sidebar()

    /// Restores the column order and visibility saved in Config. A layout
    /// from an older version may not know every column; the model appends
    /// those, so an update never silently hides a newly added column.
    function restoreColumnLayout() {
        const saved = Mixxx.Config.paramixLibraryColumns;
        const sidebar = root.sidebar();
        if (!saved || !sidebar)
            return;
        try {
            const layout = JSON.parse(saved);
            if (Array.isArray(layout))
                sidebar.applyColumnLayout(layout);
        } catch (e) {
            console.warn("Ignoring invalid library column layout:", e);
        }
    }
    function saveColumnLayout() {
        const sidebar = root.sidebar();
        if (!sidebar)
            return;
        Mixxx.Config.paramixLibraryColumns = JSON.stringify(sidebar.columnLayout());
    }

    Component.onCompleted: root.restoreColumnLayout()

    Connections {
        function onColumnLayoutChanged() {
            root.saveColumnLayout();
        }

        target: root.sidebarProxy
    }

    defaultColumns: [
        Mixxx.TrackListColumn {
            key: "cover"
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
            key: "title"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Title
            fillSpan: 3
            label: qsTr("Title")

            delegate: DefaultDelegate {
                bold: true
            }
        },
        Mixxx.TrackListColumn {
            key: "artist"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Artist
            fillSpan: 2
            label: qsTr("Artist")

            delegate: DefaultDelegate {
                bold: true
            }
        },
        Mixxx.TrackListColumn {
            key: "album"
            autoHideWidth: 690
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Album
            fillSpan: 1
            label: qsTr("Album")

            delegate: DefaultDelegate {
            }
        },
        MetaColumn {
            key: "genre"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Genre
            label: qsTr("Genre")
            preferredWidth: 100
        },
        Mixxx.TrackListColumn {
            key: "year"
            autoHideWidth: 750
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Year
            label: qsTr("Year")
            preferredWidth: 80

            delegate: DefaultDelegate {
            }
        },
        MetaColumn {
            key: "albumartist"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.AlbumArtist
            hidden: true
            label: qsTr("Album Artist")
            preferredWidth: 120
        },
        MetaColumn {
            key: "composer"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Composer
            hidden: true
            label: qsTr("Composer")
            preferredWidth: 120
        },
        MetaColumn {
            key: "grouping"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Grouping
            hidden: true
            label: qsTr("Grouping")
            preferredWidth: 120
        },
        MetaColumn {
            key: "tracknumber"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.TrackNumber
            hidden: true
            label: qsTr("Track #")
            preferredWidth: 60
        },
        MetaColumn {
            key: "comment"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Comment
            hidden: true
            label: qsTr("Comment")
            preferredWidth: 160
        },
        Mixxx.TrackListColumn {
            key: "bpm"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Bpm
            label: qsTr("Bpm")
            preferredWidth: 60

            delegate: DefaultDelegate {
            }
        },
        Mixxx.TrackListColumn {
            key: "key"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Key
            label: qsTr("Key")
            preferredWidth: 70

            delegate: DefaultDelegate {
            }
        },
        MetaColumn {
            key: "duration"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Duration
            hidden: true
            label: qsTr("Time")
            preferredWidth: 60
        },
        MetaColumn {
            key: "dateadded"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.DateAdded
            hidden: true
            label: qsTr("Date Added")
            preferredWidth: 110
        },
        MetaColumn {
            key: "timesplayed"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.TimesPlayed
            hidden: true
            label: qsTr("Played")
            preferredWidth: 70
        },
        MetaColumn {
            key: "lastplayed"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.LastPlayed
            hidden: true
            label: qsTr("Last Played")
            preferredWidth: 110
        },
        MetaColumn {
            key: "replaygain"
            columnIdx: Mixxx.TrackListColumn.SQLColumns.ReplayGain
            hidden: true
            label: qsTr("Replay Gain")
            preferredWidth: 70
        },
        // Stem state is not a sortable column. TrackList.qml draws a small
        // badge in the left gutter of the Title cell for rows whose track has
        // separated stems (model role `has_stems`), so no extra column here.
        Mixxx.TrackListColumn {
            key: "filetype"
            autoHideWidth: 900
            columnIdx: Mixxx.TrackListColumn.SQLColumns.FileType
            label: qsTr("File Type")
            preferredWidth: 70

            delegate: DefaultDelegate {
            }
        },
        Mixxx.TrackListColumn {
            key: "bitrate"
            autoHideWidth: 1200
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Bitrate
            label: qsTr("Bitrate")
            preferredWidth: 70

            delegate: DefaultDelegate {
            }
        }
    ]

    component MetaColumn: Mixxx.TrackListColumn {
        delegate: Component {
            DefaultDelegate {
            }
        }
    }

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
