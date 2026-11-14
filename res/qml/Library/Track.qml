import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.15
import "../Theme"

/// Interaction layer for a track list cell: drag, tap and the track context
/// menu. The menu follows the flat dark surface language of the library.
Item {
    id: root

    required property var capabilities
    property alias drag: dragHandler
    readonly property var library: Mixxx.Library
    property alias tap: tapHandler

    function hasCapabilities(caps) {
        return (root.capabilities & caps) == caps;
    }

    DragHandler {
        id: dragHandler

        target: value
    }
    TapHandler {
        id: tapHandler

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onLongPressed: mouse => {
            contextMenu.popup();
        }
        onTapped: (eventPoint, button) => {
            if (button === Qt.RightButton) {
                contextMenu.popup();
            }
        }
    }
    Menu {
        id: contextMenu

        title: qsTr("File")

        background: Rectangle {
            border.color: Theme.border
            border.width: 1
            color: Theme.surface2
            radius: Theme.radiusMd
        }
        palette.buttonText: Theme.textPrimary
        palette.highlight: Theme.accent
        palette.highlightedText: Theme.onAccent
        palette.mid: Theme.textMuted
        palette.text: Theme.textPrimary
        palette.window: Theme.surface2
        palette.windowText: Theme.textPrimary

        Menu {
            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToDeck) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToSampler) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToPreviewDeck);
            }
            title: qsTr("Load to")

            Menu {
                id: loadToDeckMenu

                enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToDeck)
                title: qsTr("Deck")

                Instantiator {
                    model: 4

                    delegate: MenuItem {
                        text: qsTr("Deck %1").arg(modelData + 1)

                        onTriggered: Mixxx.PlayerManager.getPlayer(`[Channel${modelData + 1}]`).loadTrack(track)
                    }

                    onObjectAdded: (index, object) => loadToDeckMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => loadToDeckMenu.removeItem(object)
                }
            }
            Menu {
                enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToSampler)
                title: qsTr("Sampler")
            }

            // Instantiator {
            //     id: recentFilesInstantiator
            //     model: settings.recentFiles
            //     delegate: MenuItem {
            //         text: settings.displayableFilePath(modelData)
            //         onTriggered: loadFile(modelData)
            //     }

            //     onObjectAdded: (index, object) => recentFilesMenu.insertItem(index, object)
            //     onObjectRemoved: (index, object) => recentFilesMenu.removeItem(object)
            // }
        }
        Menu {
            id: addToPlaylistMenu

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.AddToTrackSet);
            }
            title: qsTr("Add to playlists")

            MenuSeparator {
            }
            MenuItem {
                enabled: false // TODO implement
                text: qsTr("Create New Playlist")
            }
        }
        Menu {
            id: addToCrateMenu

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.AddToTrackSet);
            }
            title: qsTr("Crates")

            MenuSeparator {
            }
            MenuItem {
                enabled: false // TODO implement
                text: qsTr("Create New Crate")
            }
        }
        Menu {
            id: analyzeMenu

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.EditMetadata) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Analyze);
            }
            title: qsTr("Analyze")

            MenuItem {
                text: qsTr("Analyze")

                onTriggered: {
                    library.analyze(track);
                }
            }
            MenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze")
            }
            MenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze (constant BPM)")
            }
            MenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze (variable BPM)")
            }
        }
    }
}
