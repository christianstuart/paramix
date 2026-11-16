import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Controls 2.12
import "../Theme"

/// Loop section: set in/out, toggle/reloop and pick a beat size.
///
/// Laid out as an eyebrow plus two 18 px control rows on a flat panel. A small
/// status dot reports whether the loop is active without turning the panel
/// into a coloured card.
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
    Mixxx.ControlProxy {
        id: loopEnabled

        group: root.group
        key: "loop_enabled"
    }
    BeatSizeSpinBoxBehavior {
        id: beatloopSize

        group: root.group
        key: "beatloop_size"
        beatSizes: [
            1 / 32,
            1 / 16,
            1 / 8,
            1 / 4,
            1 / 2,
            1,
            2,
            4,
            8,
            16,
            32,
            64,
            128,
            256,
            512
        ]
    }
    Mixxx.ControlProxy {
        id: beatloopActivate

        group: root.group
        key: "beatloop_activate"
    }
    Mixxx.ControlProxy {
        id: loopHalve

        group: root.group
        key: "loop_halve"
    }
    Mixxx.ControlProxy {
        id: loopDouble

        group: root.group
        key: "loop_double"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space1
        spacing: Theme.space1

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 10

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textMuted
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                text: qsTr("Loop")
            }
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                border.color: loopEnabled.value ? Theme.accent : Theme.borderStrong
                border.width: 1
                color: loopEnabled.value ? Theme.accent : "transparent"
                height: 6
                radius: width / 2
                width: 6

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.motionFast
                    }
                }
                Behavior on border.color {
                    ColorAnimation {
                        duration: Theme.motionFast
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            spacing: Theme.space1

            Skin.ControlButton {
                id: loopInButton

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 28
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 18
                key: "loop_in"
                normalColor: root.buttonColor
                text: qsTr("In")

                Accessible.description: qsTr("Right click jumps to the loop in point")
                Accessible.name: qsTr("Loop in")

                Skin.ControlProxyButtonBehavior {
                    id: loopInGotoBehavior

                    group: root.group
                    key: "loop_in"
                    rightClickKey: "loop_in_goto"
                    handlePointerInput: false
                }
                MouseArea {
                    acceptedButtons: Qt.RightButton
                    anchors.fill: parent

                    onCanceled: {
                        loopInGotoBehavior.releaseSecondary();
                    }
                    onPressed: function(mouse) {
                        loopInGotoBehavior.pressSecondary(mouse.x, mouse.y);
                    }
                    onReleased: {
                        loopInGotoBehavior.releaseSecondary();
                    }
                }
            }
            Skin.ControlButton {
                id: loopOutButton

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 28
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 18
                key: "loop_out"
                normalColor: root.buttonColor
                text: qsTr("Out")

                Accessible.description: qsTr("Right click jumps to the loop out point")
                Accessible.name: qsTr("Loop out")

                Skin.ControlProxyButtonBehavior {
                    id: loopOutGotoBehavior

                    group: root.group
                    key: "loop_out"
                    rightClickKey: "loop_out_goto"
                    handlePointerInput: false
                }
                MouseArea {
                    acceptedButtons: Qt.RightButton
                    anchors.fill: parent

                    onCanceled: {
                        loopOutGotoBehavior.releaseSecondary();
                    }
                    onPressed: function(mouse) {
                        loopOutGotoBehavior.pressSecondary(mouse.x, mouse.y);
                    }
                    onReleased: {
                        loopOutGotoBehavior.releaseSecondary();
                    }
                }
            }
            Skin.ControlButton {
                id: loopRecallButton

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 18
                key: loopEnabled.value ? "loop_enabled" : "reloop_toggle"
                normalColor: root.buttonColor
                // "Rel" is the hardware shorthand; the full word elides to
                // "REL…" at this panel width.
                text: loopEnabled.value ? qsTr("Exit") : qsTr("Rel")
                toggleable: loopEnabled.value

                Accessible.name: loopEnabled.value ? qsTr("Exit loop") : qsTr("Reloop")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            spacing: Theme.space1

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => {
                    if (event.angleDelta.y < 0) {
                        loopSizeRepeater.adjustSelectedIndex(-1);
                    } else if (event.angleDelta.y > 0) {
                        loopSizeRepeater.adjustSelectedIndex(1);
                    }
                }
            }
            Skin.Button {
                id: loopSizeHalfButton

                Layout.fillHeight: true
                Layout.preferredWidth: 22
                implicitHeight: 18
                implicitWidth: 22

                Accessible.name: qsTr("Halve loop")

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
                            fillColor: loopSizeHalfButton.hovered ? Theme.textPrimary : root.buttonColor
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

                MouseArea {
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    anchors.fill: parent

                    onPressed: mouse => {
                        if (loopEnabled.value ^ mouse.button == Qt.RightButton) {
                            loopHalve.trigger();
                        }
                        loopSizeRepeater.adjustSelectedIndex(-1);
                    }
                }
            }
            Repeater {
                id: loopSizeRepeater

                property int selectedIndex: 0
                property int valueCount: Math.min(Math.max(1, parseInt((root.width - 56) / 40)), 4)
                property list<double> values: beatloopSize.beatSizes


                function adjustSelectedIndex(delta){
                    loopSizeRepeater.selectedIndex = Math.min(Math.max(0, loopSizeRepeater.selectedIndex + delta), loopSizeRepeater.values.length - 1)
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
                    selectedIndex = values.indexOf(beatloopSize.value);
                    if (selectedIndex < 0) {
                        selectedIndex = values.indexOf(4);
                    }
                }
                onSelectedIndexChanged: update()
                onValueCountChanged: update()

                Connections {
                    function onValueChanged() {
                        if (loopEnabled.value) {
                            parent.selectedIndex = parent.values.indexOf(beatloopSize.value);
                        }
                        parent.update();
                    }

                    target: beatloopSize
                }
                DeckButton {
                    id: loopSizeOpt1Button

                    property double currentSize: modelData
                    required property int index
                    required property var modelData

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 28
                    activeColor: Theme.textOnAccent
                    highlight: currentSize == beatloopSize.value && trackLoadedControl.value > 0
                    implicitHeight: 18
                    implicitWidth: 33
                    text: beatloopSize.formatBeatSize(currentSize)

                    Accessible.name: qsTr("Loop size %1 beats").arg(beatloopSize.formatBeatSize(currentSize))

                    onPressed: {
                        if (loopEnabled.value) {
                            beatloopSize.commitText(currentSize.toString());
                        } else {
                            sizedBeatloopActivate.trigger();
                        }
                    }

                    Mixxx.ControlProxy {
                        id: sizedBeatloopActivate

                        group: root.group
                        key: `beatloop_${currentSize}_activate`
                    }
                }
            }
            Skin.Button {
                id: loopSizeDoubleButton

                Layout.fillHeight: true
                Layout.preferredWidth: 22
                implicitHeight: 18
                implicitWidth: 22

                Accessible.name: qsTr("Double loop")

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
                            fillColor: loopSizeDoubleButton.hovered ? Theme.textPrimary : root.buttonColor
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

                MouseArea {
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    anchors.fill: parent

                    onPressed: mouse => {
                        if (loopEnabled.value ^ mouse.button == Qt.RightButton) {
                            loopDouble.trigger();
                        }
                        loopSizeRepeater.adjustSelectedIndex(1);
                    }
                }
            }
        }
    }

    component DeckButton: Skin.Button {
        implicitHeight: 18
    }
}
