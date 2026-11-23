import Mixxx 1.0 as Mixxx
import QtQuick.Shapes
import QtQuick 2.12
import QtQuick.Layouts
import ".." as Skin
import "../Theme"

/// Deck utility row: reverse, deck modes and eject.
///
/// A single flat surface with neutral icon buttons and three compact mode
/// keys. Active states use the accent token; the icons themselves stay
/// low-contrast so the row does not compete with the transport buttons.
/// Beatgrid actions live in the GRID pane of the deck pane stack.
Rectangle {
    id: root

    property color buttonColor: trackLoadedControl.value > 0 ? Theme.textSecondary : Theme.textMuted
    required property string group

    /// Two rows (reverse, QNT, SLIP / RPT, eject) for the narrow slot under
    /// the jog platter.
    property bool stacked: false
    /// The mode keys need roughly 130 px in one row; below that the icon keys
    /// keep the row usable. Stacked, they always fit.
    readonly property bool showModeButtons: root.stacked || root.width > 250

    border.color: Theme.borderStrong
    border.width: 1
    color: Theme.surface1
    implicitHeight: root.stacked ? Theme.deckSideToolbarHeight : 26
    radius: Theme.radiusSm

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    // One row by default; stacked it is a 3-column grid, and the divider and
    // spacer drop out so the keys flow into two rows.
    GridLayout {
        anchors.bottomMargin: root.stacked ? 2 : 0
        anchors.fill: parent
        anchors.leftMargin: Theme.space1
        anchors.rightMargin: Theme.space1
        anchors.topMargin: root.stacked ? 2 : 0
        columnSpacing: Theme.space1
        columns: root.stacked ? 3 : -1
        rowSpacing: 2

        Skin.ControlButton {
            id: reverseButton

            Layout.fillHeight: true
            Layout.fillWidth: root.stacked
            Layout.preferredWidth: 24
            activeColor: Theme.textOnAccent
            group: root.group
            implicitHeight: 22
            implicitWidth: 24
            key: "reverse"

            Accessible.name: qsTr("Reverse")

            contentItem: Item {
                anchors.fill: parent

                Shape {
                    anchors.centerIn: parent
                    antialiasing: true
                    height: 12
                    layer.enabled: true
                    layer.samples: 4
                    width: 12

                    ShapePath {
                        capStyle: ShapePath.RoundCap
                        fillColor: reverseButton.highlight ? Theme.textOnAccent : (reverseButton.hovered ? Theme.textPrimary : root.buttonColor)
                        startX: 11
                        startY: 1
                        strokeColor: 'transparent'

                        PathLine {
                            x: 11
                            y: 11
                        }
                        PathLine {
                            x: 1
                            y: 6
                        }
                        PathLine {
                            x: 11
                            y: 1
                        }
                    }
                }
            }
            // Classic deck behaviour: hold right-click for reverse roll.
            Skin.ControlProxyButtonBehavior {
                id: reverseRollBehavior

                group: root.group
                key: "reverse"
                rightClickKey: "reverseroll"
                handlePointerInput: false
            }
            MouseArea {
                acceptedButtons: Qt.RightButton
                anchors.fill: parent

                onCanceled: {
                    reverseRollBehavior.releaseSecondary();
                }
                onPressed: function(mouse) {
                    reverseRollBehavior.pressSecondary(mouse.x, mouse.y);
                }
                onReleased: {
                    reverseRollBehavior.releaseSecondary();
                }
            }
        }
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            color: Theme.border
            implicitHeight: 12
            implicitWidth: 1
            visible: !root.stacked
        }
        ModeToggle {
            id: quantizeButton

            group: root.group
            key: "quantize"
            label: qsTr("QNT")
            visible: root.showModeButtons

            Accessible.description: qsTr("Snap cues and loops to the beat grid")
            Accessible.name: qsTr("Quantize")
        }
        ModeToggle {
            id: slipButton

            group: root.group
            key: "slip_enabled"
            label: qsTr("SLIP")
            visible: root.showModeButtons

            Accessible.description: qsTr("Keep the track running silently while you cue or loop")
            Accessible.name: qsTr("Slip mode")
        }
        ModeToggle {
            id: repeatButton

            group: root.group
            key: "repeat"
            label: qsTr("RPT")
            visible: root.showModeButtons

            Accessible.description: qsTr("Restart the track when it reaches the end")
            Accessible.name: qsTr("Repeat")
        }
        Item {
            Layout.fillWidth: true
            visible: !root.stacked
        }
        Skin.ControlButton {
            id: ejectButton

            Layout.fillHeight: true
            Layout.fillWidth: root.stacked
            Layout.preferredWidth: 24
            activeColor: Theme.textOnAccent
            group: root.group
            implicitHeight: 22
            implicitWidth: 24
            key: "eject"

            Accessible.name: qsTr("Eject")

            contentItem: Item {
                anchors.fill: parent

                Shape {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    antialiasing: true
                    height: 7
                    layer.enabled: true
                    layer.samples: 4
                    width: 12

                    ShapePath {
                        fillColor: ejectButton.highlight ? Theme.textOnAccent : (ejectButton.hovered ? Theme.textPrimary : root.buttonColor)
                        startX: 6
                        startY: 0
                        strokeColor: 'transparent'

                        PathLine {
                            x: 12
                            y: 7
                        }
                        PathLine {
                            x: 0
                            y: 7
                        }
                        PathLine {
                            x: 6
                            y: 0
                        }
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: ejectButton.highlight ? Theme.textOnAccent : (ejectButton.hovered ? Theme.textPrimary : root.buttonColor)
                    height: 2
                    width: 12
                }
            }
        }
    }

    /// Compact latching key for the deck modes (quantize, slip, repeat).
    ///
    /// The label sizes the key so the caps never elide, and the 22 px height
    /// keeps the row on the same rhythm as the icon keys next to it.
    component ModeToggle: Skin.ControlButton {
        id: modeToggle

        required property string label

        readonly property int labelWidth: Math.ceil(labelMetrics.width) + 8

        Layout.fillHeight: true
        Layout.fillWidth: root.stacked
        Layout.minimumWidth: labelWidth
        Layout.preferredWidth: labelWidth
        activeColor: Theme.textOnAccent
        implicitHeight: 22
        implicitWidth: labelWidth
        toggleable: true

        TextMetrics {
            id: labelMetrics

            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: modeToggle.label
        }
        contentItem: Text {
            anchors.fill: parent
            color: modeToggle.labelColor
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: modeToggle.label
            verticalAlignment: Text.AlignVCenter
        }
    }
}
