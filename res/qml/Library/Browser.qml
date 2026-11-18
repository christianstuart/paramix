import ".." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects
import "../Theme"

/// Library source tree (All tracks, playlists, crates, ...).
Rectangle {
    id: root

    readonly property var featureSelection: ItemSelectionModel {
        model: root.model
    }
    required property var model
    // The library search sits with the sources now that the full-width library
    // toolbar is gone. The owning pane drives the actual search; this only
    // reports edits so the debounce stays in one place.
    readonly property alias searchInput: searchField

    signal searchCleared
    signal searchEdited

    function activateDefaultSource() {
        if (!root.model || root.model.tracklist) {
            return;
        }
        // activate() ignores invalid indexes, so no validity check is needed.
        root.model.activate(root.model.index(0, 0));
    }
    function openSourceMenu(index) {
        if (!index || !index.valid || !root.model) {
            return;
        }
        // Keep the model index itself: index.row is the row inside the parent,
        // not the view row, so rebuilding it from row/column would act on a
        // different source (e.g. the first playlist instead of the fourth).
        sourceMenu.sourceIndex = index;
        sourceMenu.nodes = root.model.contextMenu(index);
        if (sourceMenu.nodes.length === 0) {
            return;
        }
        sourceMenu.rebuild();
        sourceMenu.popup();
    }
    function selectRow(index) {
        if (!index) {
            return;
        }
        featureSelection.setCurrentIndex(index, ItemSelectionModel.NoUpdate);
        featureSelection.select(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
    }
    function showDropMessage(message) {
        dropMessage.messageText = message;
        dropMessageTimer.restart();
    }
    function toggleNode(row) {
        if (featureView.isExpanded(row)) {
            featureView.collapse(row);
            return;
        }
        // Lazy sources (crates, playlists, folders, ...) only create their
        // children when the item is expanded. Do that before expanding the
        // view so the view can take the new rows into account.
        featureView.model.expand(featureView.index(row, 0));
        featureView.expand(row);
    }

    color: Theme.surface1

    Component.onCompleted: root.activateDefaultSource()

    Connections {
        function onModelReset() {
            root.activateDefaultSource();
        }

        target: root.model
    }
    Rectangle {
        id: treeFrame

        anchors.fill: parent
        anchors.margins: Theme.space2
        border.color: Theme.border
        border.width: 1
        color: Theme.surface0
        radius: Theme.radiusMd

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space1
            spacing: 0

            Item {
                id: searchBox

                Layout.fillWidth: true
                Layout.leftMargin: Theme.space1
                Layout.rightMargin: Theme.space1
                Layout.topMargin: Theme.space1
                implicitHeight: 30

                Skin.TextField {
                    id: searchField

                    Accessible.name: qsTr("Search the library")
                    Accessible.role: Accessible.EditableText
                    anchors.fill: parent
                    font.pixelSize: Theme.fontSizeSm
                    placeholderText: qsTranslate("WSearchLineEdit", "Search...")
                    rightPadding: Theme.space2 + (clearButton.visible ? clearButton.width + Theme.space1 : 0)
                    verticalAlignment: TextInput.AlignVCenter

                    onTextChanged: root.searchEdited()
                }
                Item {
                    id: clearButton

                    Accessible.name: qsTr("Clear search")
                    Accessible.role: Accessible.Button
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    height: 14
                    visible: searchField.text.length > 0
                    width: 14

                    Accessible.onPressAction: root.searchCleared()

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
                        width: 7
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        color: clearMouseArea.containsMouse ? Theme.textPrimary : Theme.textMuted
                        height: 1
                        rotation: -45
                        width: 7
                    }
                    MouseArea {
                        id: clearMouseArea

                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: root.searchCleared()
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space1
                Layout.rightMargin: Theme.space1
                Layout.topMargin: Theme.space1
                color: Theme.border
                implicitHeight: 1
            }
            ScrollView {
                Layout.fillHeight: true
                Layout.fillWidth: true

                TreeView {
                    id: featureView

                    Layout.fillWidth: true
                    clip: true
                    model: root.model
                    selectionModel: root.featureSelection

                    delegate: FocusScope {
                        required property int column
                        required property bool current
                        required property int depth
                        required property bool expanded
                        required property int hasChildren
                        required property var icon
                        readonly property real indentation: Theme.space3

                        // Rotate indicator when expanded by the user
                        // (requires TreeView to have a selectionModel)
                        property Animation indicatorAnimation: NumberAnimation {
                            duration: Theme.motionFast
                            easing.type: Easing.OutQuart
                            from: expanded ? 0 : 90
                            property: "rotation"
                            target: indicator
                            to: expanded ? 90 : 0
                        }
                        required property bool isTreeNode
                        required property string label
                        readonly property real padding: Theme.space1
                        required property int row
                        // Note: the modelIndex() signature changed with Qt 6.4:
                        // the arguments are (row, column). index() is the
                        // non-deprecated equivalent.
                        readonly property var sourceIndex: treeView.index(row, column)
                        readonly property bool topLevel: depth === 0

                        // Assigned to by TreeView:
                        required property TreeView treeView

                        Accessible.name: label
                        Accessible.role: Accessible.ListItem
                        implicitHeight: topLevel ? 32 : 28
                        implicitWidth: treeView.width

                        TableView.onPooled: indicatorAnimation.complete()
                        TableView.onReused: if (current)
                            indicatorAnimation.start()
                        onExpandedChanged: indicator.rotation = expanded ? 90 : 0

                        Rectangle {
                            id: background

                            anchors.fill: parent
                            color: topLevel ? Theme.surface2 : "transparent"
                        }
                        Rectangle {
                            id: rowHighlight

                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 1
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space1
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.space1
                            anchors.top: parent.top
                            anchors.topMargin: 1
                            color: current ? Qt.alpha(Theme.accent, 0.18) : rowDropArea.containsDrag ? Qt.alpha(Theme.accent, 0.25) : rowMouseArea.containsMouse ? Theme.surface3 : "transparent"
                            radius: Theme.radiusSm
                        }
                        Rectangle {
                            id: currentMarker

                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: Theme.space1
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space1
                            anchors.top: parent.top
                            anchors.topMargin: Theme.space1
                            color: Theme.accent
                            radius: Theme.radiusXs
                            visible: current
                            width: 2
                        }
                        MouseArea {
                            id: rowMouseArea

                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            anchors.fill: parent
                            hoverEnabled: true

                            onClicked: {
                                // Keep the tree keyboard-navigable: the Menu key
                                // acts on the selected source row.
                                featureView.forceActiveFocus();
                                root.selectRow(sourceIndex);
                                // Selecting a source activates it, i.e. shows its
                                // tracks. Expanding is a separate action.
                                if (mouse.button === Qt.RightButton) {
                                    root.openSourceMenu(sourceIndex);
                                } else {
                                    root.model.activate(sourceIndex);
                                }
                            }
                            onDoubleClicked: {
                                // Double click also expands sources that
                                // create their children lazily (e.g. folders
                                // which have no children yet).
                                if (isTreeNode) {
                                    root.toggleNode(row);
                                }
                            }
                        }
                        DropArea {
                            id: rowDropArea

                            anchors.fill: parent

                            // Playlists and crates accept dropped tracks and
                            // folders; the feature decides whether it does.
                            onDropped: drop => {
                                if (!drop.hasUrls)
                                    return;
                                if (!root.model.dropUrls(sourceIndex, drop.urls))
                                    root.showDropMessage(qsTr("Nothing was added. Drop audio files or folders onto a playlist or crate."));
                                drop.acceptProposedAction();
                            }
                        }
                        RowLayout {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space2 + indentation * depth
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.space2
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.space1

                            // Source icons ship in their own colours (orange
                            // tracks, red recordings, ...). Tint them with the
                            // accent so the tree reads as one system.
                            Image {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredHeight: 16
                                Layout.preferredWidth: 16
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                layer.enabled: !!icon
                                source: icon ?? ""
                                visible: !!icon

                                layer.effect: ColorOverlay {
                                    color: Theme.accent
                                }
                            }
                            Label {
                                id: indicator

                                Layout.alignment: Qt.AlignVCenter
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                text: "▶"
                                visible: isTreeNode && hasChildren

                                MouseArea {
                                    anchors.fill: parent

                                    onClicked: root.toggleNode(row)
                                }
                            }
                            Label {
                                id: labelItem

                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: true
                                color: topLevel ? Theme.textPrimary : Theme.textSecondary
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMd
                                font.weight: topLevel ? Font.DemiBold : Font.Medium
                                text: label
                            }
                            Item {
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: 20
                                visible: rowMouseArea.containsMouse && isTreeNode && hasChildren
                            }
                        }
                        Rectangle {
                            id: newItem

                            anchors.right: parent.right
                            anchors.rightMargin: Theme.space2
                            anchors.verticalCenter: parent.verticalCenter
                            border.color: Theme.borderStrong
                            border.width: 1
                            color: Theme.surface3
                            height: 20
                            radius: Theme.radiusSm
                            visible: rowMouseArea.containsMouse && isTreeNode && hasChildren
                            width: 20

                            Text {
                                anchors.centerIn: parent
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMd
                                text: "+"
                            }
                        }
                    }

                    Keys.onPressed: event => {
                        // Keyboard context menu for the current source row.
                        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                            root.openSourceMenu(root.featureSelection.currentIndex);
                            event.accepted = true;
                        }
                    }
                }
            }
        }
    }
    Rectangle {
        id: dropMessage

        property string messageText: ""

        Accessible.name: messageText
        Accessible.role: Accessible.StaticText
        anchors.bottom: treeFrame.bottom
        anchors.bottomMargin: Theme.space2
        anchors.horizontalCenter: parent.horizontalCenter
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface3
        height: 26
        radius: Theme.radiusSm
        visible: messageText.length > 0 && dropMessageTimer.running
        width: Math.min(treeFrame.width - 2 * Theme.space2, dropMessageLabel.implicitWidth + 2 * Theme.space3)
        z: 2

        Text {
            id: dropMessageLabel

            anchors.centerIn: parent
            color: Theme.textPrimary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            text: dropMessage.messageText
            width: parent.width - 2 * Theme.space2
        }
    }
    Timer {
        id: dropMessageTimer

        interval: 4000
    }
    Menu {
        id: sourceMenu

        property var nodes: []
        property var sourceIndex: null

        function rebuild() {
            while (count > 0) {
                removeItem(itemAt(0));
            }
            for (let i = 0; i < nodes.length; ++i) {
                const node = nodes[i];
                if (node.kind === "separator") {
                    addItem(separatorComponent.createObject(sourceMenu));
                    continue;
                }
                const item = actionComponent.createObject(sourceMenu);
                item.nodeAction = node.action;
                item.text = node.text;
                item.enabled = node.enabled;
                addItem(item);
            }
        }

        palette.buttonText: Theme.textPrimary
        palette.highlight: Theme.accent
        palette.highlightedText: Theme.textOnAccent
        palette.mid: Theme.textMuted
        palette.text: Theme.textPrimary
        palette.window: Theme.surface2
        palette.windowText: Theme.textPrimary

        background: Rectangle {
            border.color: Theme.border
            border.width: 1
            color: Theme.surface2
            implicitWidth: 260
            radius: Theme.radiusMd
        }

        Component {
            id: separatorComponent

            MenuSeparator {
            }
        }
        Component {
            id: actionComponent

            MenuItem {
                id: actionItem

                property string nodeAction: ""

                onTriggered: root.model.invokeContextMenuAction(sourceMenu.sourceIndex, actionItem.nodeAction)
            }
        }
    }
}
