import "." as Skin
import QtQuick 2.12
import "Theme"

/// One mixer channel strip: pregain, level faders and PFL.
///
/// Every control sits in the same flat, 1 px framed tile so the strips line up
/// with the EQ columns next to them.
Item {
    id: root

    required property string group

    Rectangle {
        id: gainKnobFrame

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        border.color: Theme.border
        border.width: 1
        color: Theme.surface2
        height: width
        radius: Theme.radiusSm

        Skin.ControlKnob {
            id: gainKnob

            anchors.centerIn: parent
            color: Theme.accent
            group: root.group
            height: parent.height - 2 * Theme.space1
            key: "pregain"
            width: height
        }
    }
    Rectangle {
        id: faderFrame

        anchors.bottom: pflButton.top
        anchors.bottomMargin: Theme.space1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: gainKnobFrame.bottom
        anchors.topMargin: Theme.space1
        border.color: Theme.border
        border.width: 1
        color: Theme.surface0
        radius: Theme.radiusSm

        // Meter rails flank the fader, outside the handle travel.
        Skin.VuMeter {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space1
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.top: parent.top
            anchors.topMargin: Theme.space1
            group: root.group
            key: "vu_meter_left"
            width: 3
        }
        Skin.VuMeter {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space1
            anchors.right: parent.right
            anchors.rightMargin: 3
            anchors.top: parent.top
            anchors.topMargin: Theme.space1
            group: root.group
            key: "vu_meter_right"
            width: 3
        }
        Skin.ControlFader {
            id: volumeSlider

            anchors.fill: parent
            anchors.margins: 2
            bar.color: Theme.accent
            bg: Theme.imgVolumeSliderBackground
            group: root.group
            key: "volume"

            handleImage {
                height: 8
                width: 26
            }
        }
    }
    Skin.ControlButton {
        id: pflButton

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        activeColor: Theme.textOnAccent
        group: root.group
        height: 24
        key: "pfl"
        text: qsTr("PFL")
        toggleable: true
    }
}
