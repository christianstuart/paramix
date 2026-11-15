import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "../Theme"

/// Crossfader with its per-deck assignment buttons.
///
/// It lives in the output bar below the deck/mixer row so the mixer column can
/// end at the channel strips and the decks can be shortened to that height.
Item {
    id: root

    required property bool show4decks

    implicitHeight: 44
    implicitWidth: 320

    Rectangle {
        id: crossfaderSurface

        anchors.fill: parent
        anchors.margins: Theme.space1
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface0
        radius: Theme.radiusSm

        // Lit top edge inside the recessed fader well.
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 1
            color: Theme.topHighlight
            height: 1
        }

        // Center detent marker behind the fader.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.borderStrong
            height: 10
            width: 1
        }
        GridLayout {
            id: leftDeckAssignment

            anchors.left: parent.left
            anchors.leftMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            columnSpacing: 2
            rowSpacing: 2
            width: root.show4decks ? 34 : 16

            Repeater {
                model: root.show4decks ? 4 : 2

                Item {
                    required property int index

                    Layout.column: show4decks ? index % 2 : 0
                    Layout.row: root.show4decks ? parseInt(index / 2) : index
                    implicitHeight: 16
                    implicitWidth: 16

                    Skin.ControlButton {
                        id: deckButton

                        Accessible.name: qsTr("Assign crossfader to deck %1").arg(index + 1)
                        activeColor: Theme.textOnAccent
                        anchors.fill: parent
                        group: `[Channel${index + 1}]`
                        key: "orientation_left"
                        text: qsTr("%1").arg(index + 1)
                        toggleable: true
                    }
                }
            }
        }
        Skin.ControlFader {
            id: crossfaderSlider

            Accessible.name: qsTr("Crossfader")
            Accessible.role: Accessible.Slider

            anchors.left: leftDeckAssignment.right
            anchors.leftMargin: Theme.space1
            anchors.right: rightDeckAssignment.left
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            bar.color: Theme.accent
            bar.start: 0.5
            bg: Theme.imgCrossfaderBackground
            fg: Theme.imgCrossfaderHandle
            group: "[Master]"
            key: "crossfader"
            orientation: Qt.Horizontal

            handleImage {
                height: 28
            }
        }
        GridLayout {
            id: rightDeckAssignment

            anchors.right: parent.right
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            columnSpacing: 2
            rowSpacing: 2
            width: root.show4decks ? 34 : 16

            Repeater {
                model: root.show4decks ? 4 : 2

                Item {
                    required property int index

                    Layout.column: show4decks ? index % 2 : 0
                    Layout.row: root.show4decks ? parseInt(index / 2) : index
                    implicitHeight: 16
                    implicitWidth: 16

                    Skin.ControlButton {
                        id: deckButtonRight

                        Accessible.name: qsTr("Assign crossfader to deck %1").arg(index + 1)
                        activeColor: Theme.textOnAccent
                        anchors.fill: parent
                        group: `[Channel${index + 1}]`
                        key: "orientation_right"
                        text: qsTr("%1").arg(index + 1)
                        toggleable: true
                    }
                }
            }
        }
    }

}
