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

    readonly property var featureSelection: ItemSelectionModel {}
    required property var model

    color: Theme.surface1

    Component.onCompleted: {
        root.model.activate(root.model.index(0, 0));
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
                    selectionModel: featureSelection

                    delegate: FocusScope {
                        required property int column
                        required property bool current
                        required property int depth
                        required property bool expanded
                        required property int hasChildren
                        required property var icon
                        readonly property real indentation: Theme.space3
                        // FIXME The signature for that function has changed after Qt 6.4.2 (currently shipped on Ubuntu 24.04)
                        // See https://github.com/mixxxdj/mixxx/pull/14514#issuecomment-2770811094 for further details
                        readonly property var index: treeView.modelIndex(column, row)

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

                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            anchors.fill: parent
                            hoverEnabled: true

                            onClicked: event => {
                                treeView.selectionModel.select(treeView.selectionModel.model.index(row, 0), ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Clear | ItemSelectionModel.Current);
                                treeView.model.activate(index);
                                if (isTreeNode && hasChildren) {
                                    treeView.toggleExpanded(row);
                                }
                                event.accepted = true;
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
