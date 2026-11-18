pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

/// Crossfader with its per-deck assignment buttons.
///
/// It lives in the output bar below the deck/mixer row so the mixer column can
/// end at the channel strips and the decks can be shortened to that height.
/// Each deck gets one three-segment L/C/R key, matching the classic skin: the
/// engine still takes momentary [ChannelN],orientation_left/center/right
/// pushes, while the lit segment follows [ChannelN],orientation so the three
/// states are mutually exclusive.
Item {
    id: root

    required property bool show4decks

    implicitHeight: 34
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
        // Decks 1-2 on the left, 3-4 on the right in four-deck mode. The
        // selectors are split by deck so the same control is never drawn twice.
        Column {
            id: leftDeckAssignment

            anchors.left: parent.left
            anchors.leftMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Repeater {
                model: root.show4decks ? 2 : 1

                AssignmentButton {
                    required property int index

                    deck: index
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
            // The background image's own 41 px would overflow the 36 px slot.
            height: parent.height - 2
            key: "crossfader"
            orientation: Qt.Horizontal

            handleImage {
                height: 20
            }
        }
        Column {
            id: rightDeckAssignment

            anchors.right: parent.right
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Repeater {
                model: root.show4decks ? 2 : 1

                AssignmentButton {
                    required property int index

                    deck: root.show4decks ? index + 2 : index + 1
                }
            }
        }
    }

    /// Three-segment crossfader assignment key for one deck: a deck digit and
    /// the L / C / R pushes beside it.
    component AssignmentButton: Row {
        id: assignment

        required property int deck
        readonly property string group: "[Channel" + (deck + 1) + "]"
        readonly property int orientation: orientationControl.value

        height: 12
        spacing: 2

        Mixxx.ControlProxy {
            id: orientationControl

            group: assignment.group
            key: "orientation"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            height: assignment.height
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("%1").arg(assignment.deck + 1)
            verticalAlignment: Text.AlignVCenter
            width: 8
        }
        Repeater {
            model: ["orientation_left", "orientation_center", "orientation_right"]

            Skin.ControlButton {
                id: segment

                required property int index
                required property string modelData
                readonly property bool selected: assignment.orientation === index

                // The three segments are one exclusive state, not three
                // toggles, so the accessible role is a radio group and the
                // selected segment is checked.
                Accessible.checkable: true
                Accessible.checked: segment.selected
                Accessible.description: index === 1 ? qsTr("Thru: deck %1 plays through regardless of the crossfader").arg(assignment.deck + 1) : ""
                Accessible.name: qsTr("Assign deck %1 to the %2%3").arg(assignment.deck + 1).arg(index === 0 ? qsTr("left") : index === 1 ? qsTr("center (thru)") : qsTr("right")).arg(segment.selected ? qsTr(", selected") : "")
                Accessible.role: Accessible.RadioButton
                activeColor: Theme.textOnAccent
                group: assignment.group
                height: assignment.height
                highlight: segment.selected
                key: modelData
                text: [qsTr("L"), qsTr("C"), qsTr("R")][index]
                width: 11

                // 11 px is narrower than Button.qml's 6 px label margins on
                // each side, which would elide the letter to nothing.
                contentItem: Text {
                    color: segment.labelColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Theme.fontWeightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                    text: segment.text
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
