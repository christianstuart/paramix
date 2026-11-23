import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Shapes
import QtQuick.Layouts
import "../Theme"

/// Beatjump pane: the MOVE tab of the deck pane stack.
///
/// One row: two jump targets around the editable size and a Traktor-style
/// size strip. Picking a size only
/// selects it (`beatjump_size`); the chevrons actually jump, which keeps a
/// stray click from moving the playhead.
Rectangle {
    id: root

    property color buttonColor: trackLoadedControl.value > 0 ? Theme.textSecondary : Theme.textMuted
    required property string group

    border.color: Theme.border
    border.width: 1
    color: Theme.surface1
    radius: Theme.radiusMd

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    BeatSizeSpinBoxBehavior {
        id: beatjumpSize

        decrementKey: "beatjump_size_halve"
        group: root.group
        incrementKey: "beatjump_size_double"
        key: "beatjump_size"
    }

    // One row: back, the editable size, the size strip, forward.
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.deckPaneTabGap
        spacing: Theme.space1

        Skin.ControlButton {
            id: jumpBackButton

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: 60
            group: root.group
            implicitHeight: 18
            key: "beatjump_backward"

            Accessible.description: qsTr("Jump back by the selected beat size")
            Accessible.name: qsTr("Beatjump backward")

            contentItem: Item {
                anchors.fill: parent

                Row {
                    anchors.centerIn: parent
                    spacing: Theme.space1

                    Shape {
                        anchors.verticalCenter: parent.verticalCenter
                        antialiasing: true
                        height: 16
                        layer.enabled: true
                        layer.samples: 4
                        width: 22

                        ShapePath {
                            fillColor: jumpBackButton.highlight ? Theme.textOnAccent : (jumpBackButton.hovered ? Theme.textPrimary : root.buttonColor)
                            startX: 10
                            startY: 1
                            strokeColor: 'transparent'

                            PathLine {
                                x: 0
                                y: 8
                            }
                            PathLine {
                                x: 10
                                y: 15
                            }
                            PathLine {
                                x: 10
                                y: 1
                            }
                        }
                        ShapePath {
                            fillColor: jumpBackButton.highlight ? Theme.textOnAccent : (jumpBackButton.hovered ? Theme.textPrimary : root.buttonColor)
                            startX: 21
                            startY: 1
                            strokeColor: 'transparent'

                            PathLine {
                                x: 11
                                y: 8
                            }
                            PathLine {
                                x: 21
                                y: 15
                            }
                            PathLine {
                                x: 21
                                y: 1
                            }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: jumpBackButton.highlight ? Theme.textOnAccent : (jumpBackButton.hovered ? Theme.textPrimary : root.buttonColor)
                        font.capitalization: Font.AllUppercase
                        font.family: Theme.fontFamily
                        font.letterSpacing: Theme.letterSpacingCaps
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Theme.fontWeightDemiBold
                        text: qsTr("Back")
                        visible: jumpBackButton.width > 96
                    }
                }
            }
        }
        // The size is still editable: typing 3 (or 1/3) sticks, which the
        // power-of-two size strip below cannot express on its own.
        Rectangle {
            id: sizeField

            Layout.fillHeight: true
            Layout.preferredWidth: 52
            border.color: sizeInput.activeFocus ? Theme.focusRing : Theme.border
            border.width: 1
            color: Theme.surface2
            radius: Theme.radiusSm

            TextInput {
                id: sizeInput

                function update() {
                    this.text = Qt.binding(function () {
                        return beatjumpSize.valueText;
                    });
                }

                Accessible.name: qsTr("Beatjump size")

                anchors.fill: parent
                color: Theme.textSecondary
                font.capitalization: Font.MixedCase
                font.family: Theme.fontFamilyMono
                font.pixelSize: Theme.fontSizeXs
                horizontalAlignment: TextInput.AlignHCenter
                leftPadding: Theme.space1
                rightPadding: Theme.space1
                selectByMouse: true
                text: beatjumpSize.valueText
                verticalAlignment: TextInput.AlignVCenter

                onAccepted: {
                    beatjumpSize.commitText(this.text);
                    this.focus = false;
                    update();
                }
            }
        }

        // Traktor-style size strip; picking a size only selects it, the
        // chevron keys at either end of the row actually jump.
        SizeStrip {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: implicitWidth
            decrementKey: "beatjump_size_halve"
            group: root.group
            incrementKey: "beatjump_size_double"
            key: "beatjump_size"
            maxKeys: 8
            title: qsTr("Beatjump size")
        }
        Skin.ControlButton {
            id: jumpForwardButton

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: 60
            group: root.group
            implicitHeight: 18
            key: "beatjump_forward"

            Accessible.description: qsTr("Jump forward by the selected beat size")
            Accessible.name: qsTr("Beatjump forward")

            contentItem: Item {
                anchors.fill: parent

                Row {
                    anchors.centerIn: parent
                    spacing: Theme.space1

                    Shape {
                        anchors.verticalCenter: parent.verticalCenter
                        antialiasing: true
                        height: 16
                        layer.enabled: true
                        layer.samples: 4
                        width: 22

                        ShapePath {
                            fillColor: jumpForwardButton.highlight ? Theme.textOnAccent : (jumpForwardButton.hovered ? Theme.textPrimary : root.buttonColor)
                            startX: 12
                            startY: 1
                            strokeColor: 'transparent'

                            PathLine {
                                x: 22
                                y: 8
                            }
                            PathLine {
                                x: 12
                                y: 15
                            }
                            PathLine {
                                x: 12
                                y: 1
                            }
                        }
                        ShapePath {
                            fillColor: jumpForwardButton.highlight ? Theme.textOnAccent : (jumpForwardButton.hovered ? Theme.textPrimary : root.buttonColor)
                            startX: 1
                            startY: 1
                            strokeColor: 'transparent'

                            PathLine {
                                x: 11
                                y: 8
                            }
                            PathLine {
                                x: 1
                                y: 15
                            }
                            PathLine {
                                x: 1
                                y: 1
                            }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: jumpForwardButton.highlight ? Theme.textOnAccent : (jumpForwardButton.hovered ? Theme.textPrimary : root.buttonColor)
                        font.capitalization: Font.AllUppercase
                        font.family: Theme.fontFamily
                        font.letterSpacing: Theme.letterSpacingCaps
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Theme.fontWeightDemiBold
                        text: qsTr("Fwd")
                        visible: jumpForwardButton.width > 96
                    }
                }
            }
        }
    }
}
