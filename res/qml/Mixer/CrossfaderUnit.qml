import ".." as Skin
import QtQuick 2.12
import "../Theme"

/// Crossfader sitting in the output bar.
///
/// It lives in the output bar below the deck/mixer row so the mixer column can
/// end at the channel strips and the decks can be shortened to that height.
Item {
    id: root

    implicitHeight: 34
    implicitWidth: 360

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
        Skin.ControlFader {
            id: crossfaderSlider

            Accessible.name: qsTr("Crossfader")
            Accessible.role: Accessible.Slider

            anchors.left: parent.left
            anchors.leftMargin: Theme.space1
            anchors.right: parent.right
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            bar.color: Theme.accent
            bar.start: 0.5
            bg: Theme.imgCrossfaderBackground
            // The background image's own 41 px would overflow the short slot.
            height: parent.height - 2
            fg: Theme.imgCrossfaderHandle
            group: "[Master]"
            key: "crossfader"
            orientation: Qt.Horizontal

            handleImage {
                height: 20
            }
        }
    }
}
