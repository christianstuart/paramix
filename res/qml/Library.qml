import "." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import QtQuick.Shapes 1.6
import "Theme"
import "Library" as LibraryComponent

/// Library pane: a slim toolbar with the search field on top of the source
/// tree, preview deck and track list.
Item {
    id: root

    property var sidebar: librarySources.sidebar()

    function applySearch() {
        if (root.sidebar && root.sidebar.tracklist) {
            root.sidebar.tracklist.search(searchField.text);
        }
    }
    function focusSearch() {
        root.applySearch();
        searchField.forceActiveFocus(Qt.ShortcutFocusReason);
        searchField.selectAll();
    }

    LibraryComponent.SourceTree {
        id: librarySources
    }
    Rectangle {
        id: toolbar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.surface1
        height: Theme.toolbarHeight

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: Theme.border
            height: 1
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            spacing: Theme.space3

            Text {
                Layout.alignment: Qt.AlignVCenter
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Library")
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                color: Theme.border
                implicitHeight: Theme.space4
                implicitWidth: 1
            }
            Item {
                Layout.fillWidth: true
            }
            TextField {
                id: searchField

                Layout.alignment: Qt.AlignVCenter
                Layout.maximumWidth: 320
                Layout.preferredWidth: 280
                Accessible.name: qsTr("Search the library")
                Accessible.role: Accessible.EditableText
                placeholderText: qsTranslate("WSearchLineEdit", "Search...")

                onTextChanged: root.applySearch()
            }
        }
    }
    SplitView {
        id: librarySplitView

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: toolbar.bottom
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
                SplitView.fillHeight: true
                SplitView.minimumHeight: 200
                SplitView.preferredHeight: 500
                model: root.sidebar
            }
            Skin.PreviewDeck {
                SplitView.maximumHeight: 200
                SplitView.minimumHeight: 100
                SplitView.preferredHeight: 100
            }
        }
        LibraryComponent.TrackList {
            SplitView.fillHeight: true

            // FIXME: this is necessary to prevent the header label to render outside of the table when horizontally scrolling: https://github.com/mixxxdj/mixxx/pull/14514#issuecomment-3311914346
            clip: true
            model: root.sidebar.tracklist
        }
    }
}
