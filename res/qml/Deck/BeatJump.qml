import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Shapes
import QtQuick.Layouts
import "../Theme"

/// Beatjump pane: the MOVE tab of the deck pane stack.
///
/// Two big jump targets over a Traktor-style size strip. Picking a size only
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space1

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            spacing: Theme.space1

            Text {
                Layout.fillWidth: true
                color: Theme.textMuted
                elide: Text.ElideRight
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                text: qsTr("Beatjump")
                verticalAlignment: Text.AlignVCenter
            }
            // The size is still editable: typing 3 (or 1/3) sticks, which the
            // power-of-two size strip below cannot express on its own.
            Rectangle {
                id: sizeField

                Layout.preferredHeight: 16
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
        }
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 24
            spacing: Theme.space1

            Skin.ControlButton {
                id: jumpBackButton

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 90
                group: root.group
                implicitHeight: 24
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
            Skin.ControlButton {
                id: jumpForwardButton

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 90
                group: root.group
                implicitHeight: 24
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

            // Size strip: the same power-of-two sizes as before, gathered on a
            // plate to the right of the two jump targets so Back/Fwd keep the
            // full height of the pane.
            Rectangle {
                id: sizeStripCard

                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                Layout.maximumWidth: 340
                Layout.minimumWidth: 200
                Layout.preferredHeight: 32
                Layout.preferredWidth: 300
                border.color: Theme.border
                border.width: 1
                color: Theme.surface2
                radius: Theme.radiusSm

                RowLayout {
                    id: sizeStrip

                    anchors.fill: parent
                    anchors.margins: Theme.space1
                    spacing: Theme.space1

                    WheelHandler {
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: event => {
                            if (event.angleDelta.y < 0) {
                                beatjumpSizeRepeater.adjustSelectedIndex(-1);
                            } else if (event.angleDelta.y > 0) {
                                beatjumpSizeRepeater.adjustSelectedIndex(1);
                            }
                        }
                    }
                    Skin.Button {
                        id: jumpSizeHalfButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 22
                        implicitHeight: 24
                        implicitWidth: 22

                        Accessible.name: qsTr("Halve beatjump size")

                        onPressed: {
                            beatjumpSize.step(-1);
                        }

                        contentItem: Item {
                            anchors.fill: parent

                            Shape {
                                anchors.centerIn: parent
                                antialiasing: true
                                height: 10
                                layer.enabled: true
                                layer.samples: 4
                                width: 10

                                ShapePath {
                                    fillColor: jumpSizeHalfButton.hovered ? Theme.textPrimary : root.buttonColor
                                    startX: 0
                                    startY: 5
                                    strokeColor: 'transparent'

                                    PathLine {
                                        x: 10
                                        y: 0
                                    }
                                    PathLine {
                                        x: 10
                                        y: 10
                                    }
                                    PathLine {
                                        x: 0
                                        y: 5
                                    }
                                }
                            }
                        }
                    }
                    Repeater {
                        id: beatjumpSizeRepeater

                        property int selectedIndex: 0
                        property int valueCount: Math.min(Math.max(1, parseInt((sizeStrip.width - 56) / 40)), 5)
                        property list<double> values: beatjumpSize.beatSizes

                        function adjustSelectedIndex(delta) {
                            beatjumpSizeRepeater.selectedIndex = Math.min(Math.max(0, beatjumpSizeRepeater.selectedIndex + delta), beatjumpSizeRepeater.values.length - 1);
                        }

                        function update() {
                            let values = [this.values[selectedIndex]];
                            let appendMode = values[0] <= this.values[0];
                            while (values.length < valueCount) {
                                if (appendMode) {
                                    values.push(values[values.length - 1] * 2);
                                } else {
                                    values = [values[0] / 2, ...values];
                                }
                                if (values[0] == this.values[0]) {
                                    appendMode = true;
                                } else if (values[values.length - 1] == this.values[this.values.length - 1]) {
                                    appendMode = false;
                                } else {
                                    appendMode = !appendMode;
                                }
                            }
                            model = values;
                        }

                        Component.onCompleted: {
                            update();
                            selectedIndex = values.indexOf(beatjumpSize.value);
                            if (selectedIndex < 0) {
                                selectedIndex = values.indexOf(4);
                            }
                        }
                        onSelectedIndexChanged: update()
                        onValueCountChanged: update()

                        Connections {
                            function onValueChanged() {
                                parent.selectedIndex = Math.max(0, parent.values.indexOf(beatjumpSize.value));
                                parent.update();
                            }

                            target: beatjumpSize
                        }
                        Skin.Button {
                            id: beatjumpSizeButton

                            property double currentSize: modelData
                            required property int index
                            required property var modelData

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.minimumWidth: 28
                            activeColor: Theme.textOnAccent
                            highlight: currentSize == beatjumpSize.value && trackLoadedControl.value > 0
                            implicitHeight: 24
                            implicitWidth: 30
                            text: beatjumpSize.formatBeatSize(currentSize)

                            Accessible.name: qsTr("Beatjump size %1 beats").arg(beatjumpSize.formatBeatSize(currentSize))

                            onPressed: {
                                beatjumpSize.commitText(currentSize.toString());
                            }
                        }
                    }
                    Skin.Button {
                        id: jumpSizeDoubleButton

                        Layout.fillHeight: true
                        Layout.preferredWidth: 22
                        implicitHeight: 24
                        implicitWidth: 22

                        Accessible.name: qsTr("Double beatjump size")

                        onPressed: {
                            beatjumpSize.step(1);
                        }

                        contentItem: Item {
                            anchors.fill: parent

                            Shape {
                                anchors.centerIn: parent
                                antialiasing: true
                                height: 10
                                layer.enabled: true
                                layer.samples: 4
                                width: 10

                                ShapePath {
                                    fillColor: jumpSizeDoubleButton.hovered ? Theme.textPrimary : root.buttonColor
                                    startX: 0
                                    startY: 0
                                    strokeColor: 'transparent'

                                    PathLine {
                                        x: 10
                                        y: 5
                                    }
                                    PathLine {
                                        x: 0
                                        y: 10
                                    }
                                    PathLine {
                                        x: 0
                                        y: 0
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
