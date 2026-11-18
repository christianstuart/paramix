import "." as Skin
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import QtQuick.Shapes 1.6
import "Theme"
import "Library" as LibraryComponent

/// Library pane: source tree (with the search field at its top) and track list.
/// There is no separate library toolbar any more.
Item {
    id: root

    property var sidebar: librarySources.sidebar()

    function applySearch() {
        // Always remember the query, also while the Auto DJ source is shown.
        // QmlSidebarModelProxy::setSearchText() skips the queue itself, because
        // the processor plays row 0 and a filter would change the order.
        if (root.sidebar)
            root.sidebar.search(libraryBrowser.searchInput.text);
    }
    function clearSearch() {
        libraryBrowser.searchInput.text = "";
        // A clear should not wait for the debounce interval.
        searchDebounce.stop();
        root.applySearch();
        libraryBrowser.searchInput.forceActiveFocus(Qt.ShortcutFocusReason);
    }
    function focusSearch() {
        searchDebounce.stop();
        root.applySearch();
        libraryBrowser.searchInput.forceActiveFocus(Qt.ShortcutFocusReason);
        libraryBrowser.searchInput.selectAll();
    }

    LibraryComponent.SourceTree {
        id: librarySources
    }
    SplitView {
        id: librarySplitView

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        orientation: Qt.Horizontal

        handle: Rectangle {
            id: libraryHandle

            color: "transparent"
            implicitHeight: 8
            implicitWidth: 8

            containmentMask: Item {
                height: librarySplitView.height
                width: 8
                x: (libraryHandle.width - width) / 2
            }

            Rectangle {
                anchors.centerIn: parent
                color: SplitHandle.pressed || SplitHandle.hovered ? Theme.accent : Theme.border
                height: parent.height
                width: 1
            }
        }

        SplitView {
            id: sideBarSplitView

            SplitView.maximumWidth: 550
            SplitView.minimumWidth: 150
            SplitView.preferredWidth: root.width * 0.15
            orientation: Qt.Vertical

            handle: Rectangle {
                id: sidebarHandle

                color: "transparent"
                implicitHeight: 8
                implicitWidth: 8

                containmentMask: Item {
                    height: 8
                    width: sideBarSplitView.width
                    x: (sidebarHandle.width - width) / 2
                }

                Rectangle {
                    anchors.centerIn: parent
                    color: SplitHandle.pressed || SplitHandle.hovered ? Theme.accent : Theme.border
                    height: 1
                    width: parent.width
                }
            }

            LibraryComponent.Browser {
                id: libraryBrowser

                SplitView.fillHeight: true
                SplitView.minimumHeight: 120
                model: root.sidebar

                onSearchCleared: root.clearSearch()
                onSearchEdited: searchDebounce.restart()
            }
        }
        Item {
            id: trackBrowserPane

            SplitView.fillHeight: true
            SplitView.minimumWidth: 200

            LibraryComponent.AutoDjBar {
                id: autoDjBar

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                sidebar: root.sidebar
                trackList: trackList
                visible: root.sidebar !== null && root.sidebar.autoDjActive === true
            }
            LibraryComponent.TrackList {
                id: trackList

                anchors.bottom: taskStatusStrip.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: autoDjBar.visible ? autoDjBar.bottom : parent.top
                // FIXME: this is necessary to prevent the header label to render outside of the table when horizontally scrolling: https://github.com/mixxxdj/mixxx/pull/14514#issuecomment-3311914346
                clip: true
                model: root.sidebar.tracklist
                sidebar: root.sidebar
            }
            // Background jobs (stem separation, analysis) report at the bottom
            // of the track browser only, so the source tree keeps its full
            // height. Idle -> zero height.
            Skin.TaskStatusStrip {
                id: taskStatusStrip

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: implicitHeight
            }
        }
    }
    Timer {
        id: searchDebounce

        interval: 300
        repeat: false

        onTriggered: root.applySearch()
    }
    Connections {
        function onTracklistChanged() {
            // The search text is shared by all sources, so it has to be
            // re-applied whenever the sidebar loads another track list.
            root.applySearch();
        }

        target: root.sidebar
    }
}
