import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Shapes
import QtQuick.Controls 2.12
import ".." as Skin
import "../Theme"

/// Transport button for play/pause.
///
/// Pioneer-style transport: neutral raised face at rest, signal-green
/// backlight while playing. The glyph switches between play and pause so the
/// state reads by colour, shape and label at once.
Skin.ControlButton {
    id: root

    property bool minimized: false

    activeColor: "#06130C"
    key: "play"
    toggleable: trackLoadedControl.value == 1

    Accessible.name: root.highlight ? qsTr("Pause") : qsTr("Play")

    background: Rectangle {
        border.color: root.highlight ? Qt.lighter(Theme.positive, 1.15) : (root.hovered ? Theme.borderStrong : Theme.border)
        border.width: 1
        color: root.highlight ? Theme.positive : (root.pressed ? Theme.surface3 : (root.hovered ? Theme.surfaceHover : Theme.surface2))
        radius: Theme.radiusSm

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 1
            color: root.highlight ? Qt.rgba(255, 255, 255, 0.28) : Theme.topHighlight
            height: 1
        }

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
    contentItem: Item {
        id: content

        Item {
            id: glyph

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 10
            height: 22
            visible: !root.minimized
            width: 20

            Shape {
                id: playGlyph

                anchors.fill: parent
                antialiasing: true
                layer.enabled: true
                layer.samples: 4
                visible: !root.highlight

                ShapePath {
                    fillColor: trackLoadedControl.value > 0 ? Theme.textPrimary : Theme.textMuted
                    startX: 3
                    startY: 0
                    strokeColor: 'transparent'

                    PathLine {
                        x: 19
                        y: 11
                    }
                    PathLine {
                        x: 3
                        y: 22
                    }
                    PathLine {
                        x: 3
                        y: 0
                    }
                }
            }
            Row {
                id: pauseGlyph

                anchors.centerIn: parent
                spacing: 4
                visible: root.highlight

                Rectangle {
                    color: "#06130C"
                    height: 20
                    radius: 1
                    width: 5
                }
                Rectangle {
                    color: "#06130C"
                    height: 20
                    radius: 1
                    width: 5
                }
            }
        }
        Label {
            id: label

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 7
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.highlight ? "#06130C" : Theme.textSecondary
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: root.highlight ? qsTr("Pause") : qsTr("Play")
            verticalAlignment: Text.AlignVCenter

            states: [
                State {
                    when: root.minimized

                    AnchorChanges {
                        anchors.bottom: undefined
                        anchors.verticalCenter: content.verticalCenter
                        target: label
                    }
                }
            ]
        }
    }

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
}
