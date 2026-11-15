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
    function clearSearch() {
        searchField.text = "";
        // A clear should not wait for the debounce interval.
        searchDebounce.stop();
        root.applySearch();
        searchField.forceActiveFocus(Qt.ShortcutFocusReason);
    }
    function focusSearch() {
        searchDebounce.stop();
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
        color: Theme.surface2
        height: Theme.toolbarHeight

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: Theme.borderStrong
            height: 1
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.topHighlight
            height: 1
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            spacing: Theme.space3

            Text {
                Layout.alignment: Qt.AlignVCenter
                color: Theme.textSecondary
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
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
            Item {
                id: searchBox

                Layout.alignment: Qt.AlignVCenter
                Layout.maximumWidth: 320
                Layout.preferredWidth: 280
                implicitHeight: searchField.implicitHeight

                TextField {
                    id: searchField

                    anchors.fill: parent
                    Accessible.name: qsTr("Search the library")
                    Accessible.role: Accessible.EditableText
                    placeholderText: qsTranslate("WSearchLineEdit", "Search...")
                    rightPadding: Theme.space3 + clearButton.width + Theme.space1

                    onTextChanged: searchDebounce.restart()
                }
                Item {
                    id: clearButton

                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    height: 16
                    visible: searchField.text.length > 0
                    width: 16

                    Accessible.name: qsTr("Clear search")
                    Accessible.role: Accessible.Button
                    Accessible.onPressAction: root.clearSearch()

                    Rectangle {
                        anchors.fill: parent
                        color: clearMouseArea.containsMouse ? Theme.surfaceHover : "transparent"
                        radius: Theme.radiusXs
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        color: clearMouseArea.containsMouse ? Theme.textPrimary : Theme.textMuted
                        height: 1
                        rotation: 45
                        width: 8
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        color: clearMouseArea.containsMouse ? Theme.textPrimary : Theme.textMuted
                        height: 1
                        rotation: -45
                        width: 8
                    }
                    MouseArea {
                        id: clearMouseArea

                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: root.clearSearch()
                    }
                }
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
                visible: Mixxx.Config.paramixShowPreview
            }
        }
        Item {
            id: trackBrowserPane

            SplitView.fillHeight: true
            SplitView.minimumWidth: 200

            LibraryComponent.TrackList {
                anchors.bottom: taskStatusStrip.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                // FIXME: this is necessary to prevent the header label to render outside of the table when horizontally scrolling: https://github.com/mixxxdj/mixxx/pull/14514#issuecomment-3311914346
                clip: true
                model: root.sidebar.tracklist
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
        target: root.sidebar

        function onTracklistChanged() {
            // The search text is shared by all sources, so it has to be
            // re-applied whenever the sidebar loads another track list.
            root.applySearch();
        }
    }
}
