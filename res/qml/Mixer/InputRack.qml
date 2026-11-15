import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.15
import "../Theme"

/// Microphone and auxiliary input bank.
///
/// A single compact strip below the mixer. The units stay on one row, so the
/// rack height is bounded no matter how many inputs are configured: when there
/// are more units than the window can show, the strip scrolls horizontally
/// instead of wrapping into more rows. The header is a collapse affordance
/// that hides the units; that state is local to the rack, while section
/// visibility stays on the toolbar's show_microphones control.
Item {
    id: root

    property int auxiliaryCount: 4
    property int microphoneCount: 4
    /// Host hint for the collapsed state, e.g. "library visible on a short
    /// window". A manual toggle on the header overrides it from then on.
    property bool defaultCollapsed: false
    property bool collapsed: defaultCollapsed

    readonly property int unitCount: Math.max(0, microphoneCount) + Math.max(0, auxiliaryCount)
    readonly property int headerHeight: 18
    /// Hard cap on the unit strip so an input strip can never push the library
    /// down: the tallest unit (the ducking panel) is 63 px.
    readonly property int stripMaxHeight: 64

    implicitHeight: content.implicitHeight + 2 * Theme.space1

    Skin.SectionBackground {
        anchors.fill: parent
    }
    Column {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space1
        spacing: Theme.space1

        // Collapse affordance: chevron + unit count. Clicking anywhere on the
        // header toggles the strip.
        Item {
            id: header

            Accessible.name: root.collapsed ? qsTr("Show microphone and auxiliary inputs") : qsTr("Hide microphone and auxiliary inputs")
            Accessible.role: Accessible.Button
            height: root.headerHeight
            width: parent.width

            Rectangle {
                anchors.fill: parent
                color: headerInput.containsMouse ? Theme.surfaceHover : "transparent"
                radius: Theme.radiusSm
            }
            // Chevron drawn as two hairline strokes: points right when
            // collapsed, down when expanded.
            Item {
                id: chevron

                height: 8
                rotation: root.collapsed ? -90 : 0
                width: 8
                y: (parent.height - height) / 2

                Behavior on rotation {
                    NumberAnimation {
                        duration: Theme.motionFast
                    }
                }
                Rectangle {
                    antialiasing: true
                    color: Theme.textMuted
                    height: 1
                    rotation: 45
                    width: 5
                    x: 0.25
                    y: 4.25
                }
                Rectangle {
                    antialiasing: true
                    color: Theme.textMuted
                    height: 1
                    rotation: -45
                    width: 5
                    x: 2.75
                    y: 4.25
                }
            }
            Skin.SectionText {
                anchors.left: chevron.right
                anchors.leftMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height
                text: qsTr("Inputs")
            }
            Skin.SectionText {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textMuted
                height: parent.height
                text: qsTr("%1").arg(root.unitCount)
            }
            MouseArea {
                id: headerInput

                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: root.collapsed = !root.collapsed
            }
        }
        Item {
            id: strip

            height: root.collapsed ? 0 : Math.min(root.stripMaxHeight, unitRow.implicitHeight)
            visible: !root.collapsed
            width: parent.width

            Flickable {
                id: rack

                anchors.fill: parent
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                contentHeight: unitRow.height
                contentWidth: Math.max(width, unitRow.implicitWidth)
                flickableDirection: Flickable.HorizontalFlick

                ScrollBar.horizontal: ScrollBar {
                    id: horizontalScrollBar

                    height: 4
                    policy: ScrollBar.AsNeeded
                    visible: rack.contentWidth > rack.width

                    background: Rectangle {
                        color: "transparent"
                    }
                    contentItem: Rectangle {
                        color: horizontalScrollBar.pressed ? Theme.accent : Theme.borderStrong
                        implicitHeight: 3
                        radius: Theme.radiusXs
                    }
                }

                Row {
                    id: unitRow

                    spacing: Theme.space2

                    Repeater {
                        model: Math.max(0, root.microphoneCount)

                        Skin.MicrophoneUnit {
                            required property int index

                            unitNumber: index + 1
                            width: implicitWidth
                        }
                    }
                    Repeater {
                        model: Math.max(0, root.auxiliaryCount)

                        Skin.AuxiliaryUnit {
                            required property int index

                            unitNumber: index + 1
                            width: implicitWidth
                        }
                    }
                    Skin.MicrophoneDuckingPanel {
                        visible: duckingControls.value > 0
                        width: implicitWidth

                        Accessible.name: qsTr("Microphone talkover ducking")
                    }
                }
            }
        }
    }
    Mixxx.ControlProxy {
        id: duckingControls

        group: "[Skin]"
        key: "show_ducking_controls"
    }
}
