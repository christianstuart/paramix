import Mixxx 1.0 as Mixxx
import QtQuick.Shapes
import QtQuick 2.12
import QtQuick.Layouts
import ".." as Skin
import "../Theme"

/// Deck utility row: reverse, beatgrid toggle and eject.
Rectangle {
    id: root

    property color buttonColor: trackLoadedControl.value > 0 ? Theme.textPrimary : Theme.textMuted
    required property string group

    border.color: Theme.border
    border.width: 1
    color: Theme.surface1
    implicitHeight: 24
    radius: Theme.radiusSm

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    RowLayout {
        anchors.fill: parent
        anchors.margins: 2
        spacing: Theme.space1

        Skin.ControlButton {
            id: reverseButton

            Layout.fillHeight: true
            Layout.preferredWidth: 22
            activeColor: Theme.accent
            group: root.group
            implicitHeight: 20
            implicitWidth: 22
            key: "reverse"

            Accessible.name: qsTr("Reverse")

            contentItem: Shape {
                anchors.fill: parent
                antialiasing: true
                layer.enabled: true
                layer.samples: 4

                ShapePath {
                    fillColor: root.buttonColor
                    startX: 5
                    startY: 10
                    strokeColor: 'transparent'

                    PathLine {
                        x: 17
                        y: 4
                    }
                    PathLine {
                        x: 17
                        y: 16
                    }
                    PathLine {
                        x: 5
                        y: 10
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
        }
        Skin.Button {
            id: beatgridButton

            Layout.fillHeight: true
            Layout.preferredWidth: 64
            implicitHeight: 20
            text: qsTr("Beatgrid")
            visible: root.width > 165
        }
        Skin.ControlButton {
            id: ejectButton

            Layout.fillHeight: true
            Layout.preferredWidth: 22
            activeColor: Theme.accent
            group: root.group
            implicitHeight: 20
            implicitWidth: 22
            key: "eject"

            Accessible.name: qsTr("Eject")

            contentItem: Item {
                anchors.fill: parent

                Shape {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    antialiasing: true
                    height: 8
                    layer.enabled: true
                    layer.samples: 4
                    width: 14

                    ShapePath {
                        fillColor: root.buttonColor
                        startX: 7
                        startY: 0
                        strokeColor: 'transparent'

                        PathLine {
                            x: 14
                            y: 8
                        }
                        PathLine {
                            x: 0
                            y: 8
                        }
                        PathLine {
                            x: 7
                            y: 0
                        }
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: root.buttonColor
                    height: 2
                    radius: 1
                    width: 14
                }
            }
        }
    }
}
