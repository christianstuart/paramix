import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

/// Controller bindings for the library pane.
///
/// The classic LibraryControl targets QWidgets and deliberately disconnects
/// its library-navigation slots in QML mode, so the QML skin has to own these
/// controls. Instantiated once by Library.qml: two instances would both write
/// [Library],focused_widget and fight over the focus.
Item {
    id: root

    required property var browser
    required property var trackList

    readonly property int paneNone: Skin.FocusedWidgetControl.WidgetKind.None
    readonly property int paneSearch: Skin.FocusedWidgetControl.WidgetKind.Searchbar
    readonly property int paneSidebar: Skin.FocusedWidgetControl.WidgetKind.Sidebar
    readonly property int paneTracks: Skin.FocusedWidgetControl.WidgetKind.LibraryView
    /// Pane that currently owns keyboard focus, None while a popup owns it.
    readonly property int focusedPane: {
        if (root.trackList && root.trackList.listHasFocus) {
            return root.paneTracks;
        }
        if (root.browser && root.browser.treeHasFocus) {
            return root.paneSidebar;
        }
        if (root.browser && root.browser.searchHasFocus) {
            return root.paneSearch;
        }
        return root.paneNone;
    }
    /// Last pane a control acted on, so browse controls keep working while a
    /// dialog or popup holds the focus.
    property int lastPane: root.paneTracks

    property alias focusWidget: focusedWidgetControl.value

    function currentPane() {
        return root.focusedPane !== root.paneNone ? root.focusedPane : root.lastPane;
    }
    function focusPane(kind) {
        switch (kind) {
        case root.paneSearch:
            root.browser.focusSearch();
            break;
        case root.paneSidebar:
            root.browser.focusTree();
            break;
        case root.paneTracks:
            root.trackList.forceFocus();
            break;
        }
    }
    /// Cycles Searchbar -> Sidebar -> LibraryView and wraps around. Only the
    /// sign of `delta` matters, matching the classic Tab/Backtab behaviour.
    function moveFocus(delta) {
        if (delta === 0) {
            return;
        }
        const panes = [root.paneSearch, root.paneSidebar, root.paneTracks];
        let index = panes.indexOf(root.currentPane());
        if (index < 0) {
            index = panes.length - 1;
        }
        index = (index + (delta > 0 ? 1 : -1) + panes.length) % panes.length;
        root.lastPane = panes[index];
        root.focusPane(root.lastPane);
    }
    function moveVertical(offset) {
        if (offset === 0) {
            return;
        }
        if (root.currentPane() === root.paneSidebar) {
            root.browser.moveSelection(offset);
        } else {
            root.trackList.moveSelectionVertical(offset);
        }
    }
    function page(delta) {
        if (delta === 0) {
            return;
        }
        if (root.currentPane() === root.paneSidebar) {
            root.browser.page(delta);
        } else {
            root.trackList.page(delta);
        }
    }
    function moveHorizontal(delta) {
        if (delta === 0) {
            return;
        }
        if (root.currentPane() === root.paneSidebar) {
            root.browser.moveHorizontal(delta);
        } else if (root.currentPane() === root.paneTracks) {
            root.trackList.scrollColumns(delta);
        }
    }
    function goToItem() {
        if (root.currentPane() === root.paneSidebar) {
            // A branch toggles in place, a leaf jumps to its track list.
            if (root.browser.currentHasChildren) {
                root.browser.toggleSelected();
            } else {
                root.lastPane = root.paneTracks;
                root.focusPane(root.paneTracks);
            }
        } else if (root.currentPane() === root.paneTracks) {
            root.trackList.loadSelectedTrackIntoNextAvailableDeck(false);
        }
    }
    function selectedTrackUrl() {
        return root.trackList ? root.trackList.selectedTrackUrl() : "";
    }

    onFocusedPaneChanged: {
        if (root.focusedPane === root.paneNone) {
            return;
        }
        root.lastPane = root.focusedPane;
        if (root.focusWidget !== root.focusedPane) {
            root.focusWidget = root.focusedPane;
        }
    }
    onFocusWidgetChanged: {
        // The control also moves the focus when a mapping writes it.
        if (root.focusWidget !== root.paneNone && root.focusWidget !== root.focusedPane) {
            root.focusPane(root.focusWidget);
        }
    }
    Component.onCompleted: {
        root.lastPane = root.currentPane();
        if (root.focusWidget !== root.lastPane) {
            root.focusWidget = root.lastPane;
        }
    }

    Skin.FocusedWidgetControl {
        id: focusedWidgetControl
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "GoToItem"
        onValueChanged: (value) => {
            if (value != 0) {
                root.goToItem();
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveVertical"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveVertical(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveUp"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveVertical(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveDown"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveVertical(1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "ScrollVertical"
        onValueChanged: (value) => {
            if (value != 0) {
                root.page(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "ScrollUp"
        onValueChanged: (value) => {
            if (value != 0) {
                root.page(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "ScrollDown"
        onValueChanged: (value) => {
            if (value != 0) {
                root.page(1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveHorizontal"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveHorizontal(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveLeft"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveHorizontal(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveRight"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveHorizontal(1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocus"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveFocus(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocusForward"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveFocus(1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocusBackward"
        onValueChanged: (value) => {
            if (value != 0) {
                root.moveFocus(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectTrackKnob"
        onValueChanged: (value) => {
            if (value != 0) {
                root.trackList.moveSelectionVertical(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectPrevTrack"
        onValueChanged: (value) => {
            if (value != 0) {
                root.trackList.moveSelectionVertical(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectNextTrack"
        onValueChanged: (value) => {
            if (value != 0) {
                root.trackList.moveSelectionVertical(1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectNextPlaylist"
        onValueChanged: (value) => {
            if (value != 0) {
                root.browser.moveSelection(1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectPrevPlaylist"
        onValueChanged: (value) => {
            if (value != 0) {
                root.browser.moveSelection(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectPlaylist"
        onValueChanged: (value) => {
            if (value != 0) {
                root.browser.moveSelection(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "ToggleSelectedSidebarItem"
        onValueChanged: (value) => {
            if (value != 0) {
                root.browser.toggleSelected();
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "LoadSelectedIntoFirstStopped"
        onValueChanged: (value) => {
            if (value != 0) {
                root.trackList.loadSelectedTrackIntoNextAvailableDeck(false);
            }
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
            onLoadTrackRequested: (play) => {
                root.trackList.loadSelectedTrack(this.group, play);
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
            onLoadTrackRequested: (play) => {
                root.trackList.loadSelectedTrack(this.group, play);
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
            // Same-track presses toggle the preview instead of reloading it.
            enabled: true
            onLoadTrackRequested: (play) => {
                const player = Mixxx.PlayerManager.getPlayer(previewHandler.group);
                const selectedUrl = root.selectedTrackUrl();
                const loadedUrl = player?.currentTrack?.trackLocationUrl ?? "";
                if (play && selectedUrl && loadedUrl &&
                        String(selectedUrl) === String(loadedUrl)) {
                    if (previewPlay.value > 0) {
                        previewPlay.value = 0;
                    } else if (previewPosition.value >= 0.999) {
                        previewStartPlay.value = 1;
                    } else {
                        previewPlay.value = 1;
                    }
                    return;
                }
                root.trackList.loadSelectedTrack(previewHandler.group, play);
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
