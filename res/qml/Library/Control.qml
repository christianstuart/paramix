import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Item {
    id: root

    property alias focusWidget: focusedWidgetControl.value

    signal moveVertical(int offset)
    signal loadSelectedTrack(string group, bool play)
    signal loadSelectedTrackIntoNextAvailableDeck(bool play)

    /// The selected URL lives on the sibling TableView's selection model. This
    /// keeps the preview handler in Control.qml while still being able to tell
    /// "same track" from "different track" before loading.
    function selectedTrackUrl() {
        const siblings = parent ? parent.children : [];
        for (let i = 0; i < siblings.length; ++i) {
            const sibling = siblings[i];
            if (sibling && sibling.selectionModel &&
                    typeof sibling.selectionModel.currentTrackUrl === "function") {
                return sibling.selectionModel.currentTrackUrl();
            }
        }
        return "";
    }

    Skin.FocusedWidgetControl {
        id: focusedWidgetControl

        Component.onCompleted: this.value = Skin.FocusedWidgetControl.WidgetKind.LibraryView
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "GoToItem"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.loadSelectedTrackIntoNextAvailableDeck(false);
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "LoadSelectedIntoFirstStopped"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.loadSelectedTrackIntoNextAvailableDeck(false);
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectTrackKnob"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectPrevTrack"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectNextTrack"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveVertical"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(value);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveUp"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(-1);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveDown"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(1);
        }
    }

    Mixxx.ControlProxy {
        id: numDecksControl

        group: "[App]"
        key: "num_decks"
    }

    Instantiator {
        model: numDecksControl.value

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            required property int index

            group: "[Channel" + (index + 1) + "]"
            enabled: root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView
            onLoadTrackRequested: (play) => {
                root.loadSelectedTrack(this.group, play);
            }
        }
    }

    Mixxx.ControlProxy {
        id: numSamplersControl

        group: "[App]"
        key: "num_samplers"
    }

    Instantiator {
        model: numSamplersControl.value

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            required property int index

            group: "[Sampler" + (index + 1) + "]"
            enabled: root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView
            onLoadTrackRequested: (play) => {
                root.loadSelectedTrack(this.group, play);
            }
        }
    }

    Mixxx.ControlProxy {
        id: numPreviewDecksControl

        group: "[App]"
        key: "num_preview_decks"
    }

    // CoreServices creates the sole preview deck before QML loads. Keep the
    // model at least one so the handler exists even if the count arrives late.
    Instantiator {
        model: Math.max(1, numPreviewDecksControl.value)

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            id: previewHandler

            required property int index

            group: "[PreviewDeck" + (index + 1) + "]"
            // The QML library never writes [Library],focused_widget (it stays 0),
            // so the sampler-style gate would leave p dead. The keyboard filter
            // already suppresses shortcuts while a text input is being edited.
            enabled: true
            onLoadTrackRequested: (play) => {
                const player = Mixxx.PlayerManager.getPlayer(previewHandler.group);
                const selectedUrl = root.selectedTrackUrl();
                const loadedUrl = player?.currentTrack?.trackLocationUrl ?? "";
                if (play && selectedUrl && loadedUrl &&
                        String(selectedUrl) === String(loadedUrl)) {
                    // Same track already loaded: toggle instead of asking the
                    // player to reload it (QmlPlayerProxy dedupes the load).
                    if (previewPlay.value > 0) {
                        previewPlay.value = 0;
                    } else if (previewPosition.value >= 0.999) {
                        // At the end a plain play request would immediately stop;
                        // start_play seeks to zero and starts.
                        previewStartPlay.value = 1;
                    } else {
                        previewPlay.value = 1;
                    }
                    return;
                }
                root.loadSelectedTrack(previewHandler.group, play);
            }

            Mixxx.ControlProxy {
                id: previewPlay

                group: previewHandler.group
                key: "play"
            }
            Mixxx.ControlProxy {
                id: previewPosition

                group: previewHandler.group
                key: "playposition"
            }
            Mixxx.ControlProxy {
                id: previewStartPlay

                group: previewHandler.group
                key: "start_play"
            }
        }
    }
}
