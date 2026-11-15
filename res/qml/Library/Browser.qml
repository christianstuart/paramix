import ".." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import "../Theme"

/// Library source tree (All tracks, playlists, crates, ...).
Rectangle {
    id: root

    readonly property var featureSelection: ItemSelectionModel {
        model: root.model
    }
    required property var model

    color: Theme.surface1

    function activateDefaultSource() {
        if (!root.model || root.model.tracklist) {
            return;
        }
        // activate() ignores invalid indexes, so no validity check is needed.
        root.model.activate(root.model.index(0, 0));
    }
    function selectRow(index) {
        if (!index) {
            return;
        }
        featureSelection.setCurrentIndex(index, ItemSelectionModel.NoUpdate);
        featureSelection.select(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
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
                        // Note: the modelIndex() signature changed with Qt 6.4:
                        // the arguments are (row, column). index() is the
                        // non-deprecated equivalent.
                        readonly property var sourceIndex: treeView.index(row, column)

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
                            color: current ? Qt.alpha(Theme.accent, 0.18) : rowMouseArea.containsMouse ? Theme.surface3 : "transparent"
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

                            acceptedButtons: Qt.LeftButton
                            anchors.fill: parent
                            hoverEnabled: true

                            onClicked: {
                                root.selectRow(sourceIndex);
                                // Selecting a source activates it, i.e. shows its
                                // tracks. Expanding is a separate action.
                                root.model.activate(sourceIndex);
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
                        RowLayout {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space2 + indentation * depth
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.space2
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.space1

                            Image {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredHeight: 16
                                Layout.preferredWidth: 16
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                source: icon ?? ""
                                visible: !!icon
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
                }
            }
        }
    }
}
