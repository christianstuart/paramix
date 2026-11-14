import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Shapes
import QtQuick.Controls 2.12
import ".." as Skin
import "../Theme"

/// Accent-filled transport button. The triangle/label use the accent token
/// while stopped and the inverse `onAccent` token while playing.
Skin.ControlButton {
    id: root

    property bool minimized: false

    activeColor: Theme.textOnAccent
    key: "play"
    toggleable: trackLoadedControl.value == 1

    Accessible.name: qsTr("Play")

    background: Rectangle {
        border.color: root.highlight ? Theme.accentHover : (root.hovered ? Theme.borderStrong : Theme.border)
        border.width: 1
        color: root.highlight ? Theme.accent : (root.pressed ? Theme.surface3 : (root.hovered ? Theme.surface2 : Theme.surface1))
        radius: Theme.radiusLg

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

        Shape {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 8
            antialiasing: true
            height: 30
            layer.enabled: true
            layer.samples: 4
            visible: !root.minimized
            width: 26

            ShapePath {
                capStyle: ShapePath.RoundCap
                fillColor: root.highlight ? Theme.textOnAccent : (trackLoadedControl.value > 0 ? Theme.accent : Theme.textMuted)
                fillRule: ShapePath.WindingFill
                startX: 4
                startY: 0
                strokeColor: 'transparent'

                PathLine {
                    x: 26
                    y: 15
                }
                PathLine {
                    x: 4
                    y: 30
                }
                PathLine {
                    x: 4
                    y: 0
                }
            }
        }
        Label {
            id: label

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.highlight ? Theme.textOnAccent : Theme.textSecondary
            font.bold: true
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.buttonFontPixelSize
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
