import "." as Skin
import QtQuick 2.12
import QtQuick.Shapes
import "Theme"

/// One mixer channel strip: channel tag, trim, level faders and cue.
///
/// Rekordbox-style hardware strip: deck-dot channel tag on top, recessed
/// fader well with flush meter rails, amber cue key at the bottom.
Item {
    id: root

    required property string group

    readonly property color deckColor: {
        if (group === "[Channel1]")
            return Theme.deckA;
        if (group === "[Channel2]")
            return Theme.deckB;
        if (group === "[Channel3]")
            return Theme.deckC;
        if (group === "[Channel4]")
            return Theme.deckD;
        return Theme.accent;
    }
    readonly property string channelLabel: {
        if (group === "[Channel1]")
            return "1";
        if (group === "[Channel2]")
            return "2";
        if (group === "[Channel3]")
            return "3";
        return "4";
    }

    // Channel tag: deck dot + number in micro-type.
    Item {
        id: channelTag

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 14

        Rectangle {
            anchors.centerIn: parent
            color: "transparent"
            height: 14
            width: 36

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: root.deckColor
                height: 6
                radius: 3
                width: 6
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                font.weight: Theme.fontWeightDemiBold
                text: "CH " + root.channelLabel
            }
        }
    }

    Rectangle {
        id: gainKnobFrame

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: channelTag.bottom
        border.width: 0
        color: "transparent"
        height: width
        radius: 0

        Skin.ControlKnob {
            id: gainKnob

            anchors.centerIn: parent
            color: Theme.textPrimary
            group: root.group
            height: parent.height - 2 * Theme.space1
            key: "pregain"
            width: height
        }
    }
    Rectangle {
        id: faderFrame

        anchors.bottom: cueIcon.top
        anchors.bottomMargin: Theme.space1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: gainKnobFrame.bottom
        anchors.topMargin: Theme.space1
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface0
        radius: Theme.radiusSm

        // Lit top edge inside the recessed well.
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

        // Meter rails flank the fader, outside the handle travel.
        Skin.VuMeter {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space1
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.top: parent.top
            anchors.topMargin: Theme.space1
            group: root.group
            key: "vu_meter_left"
            width: 2
        }
        Skin.VuMeter {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space1
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.top: parent.top
            anchors.topMargin: Theme.space1
            group: root.group
            key: "vu_meter_right"
            width: 2
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
    // Headphone cue icon above the CUE key. The fader frame ends above it, so
    // the fader is a few pixels shorter to make room.
    Item {
        id: cueIcon

        anchors.bottom: pflButton.top
        anchors.bottomMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        height: 11
        width: 13
        z: 2

        Shape {
            anchors.fill: parent

            ShapePath {
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                joinStyle: ShapePath.RoundJoin
                startX: 2
                startY: 8
                strokeColor: Theme.textMuted
                strokeWidth: 1.2

                PathArc {
                    direction: PathArc.Clockwise
                    radiusX: 4.5
                    radiusY: 4.5
                    useLargeArc: false
                    x: 11
                    y: 8
                }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            color: Theme.textMuted
            height: 4
            radius: 1
            width: 2.5
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            color: Theme.textMuted
            height: 4
            radius: 1
            width: 2.5
        }
    }
    Skin.ControlButton {
        id: pflButton

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        activeColor: Theme.textOnAccent
        group: root.group
        height: 22
        key: "pfl"
        text: qsTr("CUE")
        toggleable: true
    }
}
