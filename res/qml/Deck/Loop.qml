import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2
import QtQuick.Controls 2.12
import QtQuick.Layouts
import "../Theme"

/// Loop section: set in/out, toggle/reloop, fixed beatloops, a loop-roll row
/// and an editable beatloop size.
///
/// Laid out as a header row with the editable size and the Loop key, then
/// three 15 px rows on a flat panel. A small status dot reports whether the
/// loop is active without turning the panel into a coloured card. Fixed size
/// keys use `beatloop_<size>_toggle`; the roll row is momentary and uses
/// `beatlooproll_<size>_activate`.
Rectangle {
    id: root

    property color buttonColor: root.loaded ? Theme.textSecondary : Theme.textMuted
    /// Sizes offered as fixed keys. The size field and the wheel reach the
    /// full engine range (1/32 to 512 beats).
    readonly property var fixedSizes: [0.5, 1, 2, 4, 8]
    required property string group
    readonly property bool loaded: trackLoadedControl.value > 0
    readonly property var rollSizes: [0.5, 1, 2, 4]
    /// 1 + 12 + 3 * 15 + 3 * 1 + 2 = 62, the height the deck gives the card.
    readonly property int rowHeight: 15

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

        beatSizes: [1 / 32, 1 / 16, 1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512]
        group: root.group
        key: "beatloop_size"
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 1

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 12
            spacing: 2

            // Starts a loop of the size in the field, so a typed 3 (or 1/3)
            // is actually reachable without a matching fixed key. A plain
            // Button with clicked() works for both pointer and keyboard.
            Skin.Button {
                id: loopActivateButton

                Accessible.description: qsTr("Start a loop over the set number of beats")
                Accessible.name: qsTr("Beatloop")
                Layout.preferredHeight: 12
                Layout.preferredWidth: 30
                ToolTip.delay: 500
                ToolTip.text: qsTr("Beatloop")
                ToolTip.visible: loopActivateButton.hovered
                enabled: root.loaded
                implicitHeight: 12
                implicitWidth: 30
                text: qsTr("Loop")

                contentItem: Text {
                    anchors.fill: parent
                    anchors.leftMargin: 2
                    anchors.rightMargin: 2
                    color: loopActivateButton.labelColor
                    elide: Text.ElideRight
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.pixelSize: Math.min(Theme.fontSizeXs, 10)
                    font.weight: Theme.fontWeightDemiBold
                    horizontalAlignment: Text.AlignHCenter
                    text: loopActivateButton.text
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: beatloopActivateControl.trigger()

                Mixxx.ControlProxy {
                    id: beatloopActivateControl

                    group: root.group
                    key: "beatloop_activate"
                }
            }
            // The size is editable: 3 or 1/3 stick, which the power-of-two
            // fixed keys below cannot express. Enter or focus loss commits.
            Rectangle {
                id: sizeField

                Layout.preferredHeight: 12
                Layout.preferredWidth: 46
                ToolTip.delay: 500
                ToolTip.text: qsTr("Beatloop Size")
                ToolTip.visible: sizeHover.hovered
                border.color: sizeInput.activeFocus ? Theme.focusRing : Theme.border
                border.width: 1
                color: Theme.surface2
                radius: Theme.radiusSm

                HoverHandler {
                    id: sizeHover
                }
                TextInput {
                    id: sizeInput

                    function update() {
                        this.text = Qt.binding(function () {
                            return beatloopSize.valueText;
                        });
                    }

                    Accessible.description: qsTr("Select the size of the loop in beats to set with the Beatloop button")
                    Accessible.name: qsTr("Beatloop size")
                    anchors.fill: parent
                    color: root.loaded ? Theme.textSecondary : Theme.textMuted
                    enabled: root.loaded
                    font.capitalization: Font.MixedCase
                    font.family: Theme.fontFamilyMono
                    font.pixelSize: Theme.fontSizeXs
                    horizontalAlignment: TextInput.AlignHCenter
                    leftPadding: 2
                    rightPadding: 2
                    selectByMouse: true
                    text: beatloopSize.valueText
                    verticalAlignment: TextInput.AlignVCenter

                    // Esc restores the control value before focus leaves, so
                    // the focus-loss commit below is a no-op.
                    Keys.onEscapePressed: {
                        this.update();
                        this.focus = false;
                    }
                    onAccepted: {
                        beatloopSize.commitText(this.text);
                        this.focus = false;
                        this.update();
                    }
                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            beatloopSize.commitText(this.text);
                            this.update();
                        }
                    }
                }
            }
            Skin.ControlButton {
                id: loopAnchorButton

                Accessible.description: qsTr("Define whether the loop is created and adjusted from its starting point or ending point")
                Accessible.name: qsTr("Beatloop anchor")
                Layout.preferredHeight: 12
                Layout.preferredWidth: 18
                ToolTip.delay: 500
                ToolTip.text: qsTr("Beatloop Anchor")
                ToolTip.visible: loopAnchorButton.hovered
                enabled: root.loaded
                group: root.group
                implicitHeight: 12
                implicitWidth: 18
                key: "loop_anchor"
                text: qsTr("A")
                toggleable: true
            }
            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }
            Rectangle {
                Layout.preferredHeight: 6
                Layout.preferredWidth: 6
                border.color: loopEnabled.value ? Theme.accent : Theme.borderStrong
                border.width: 1
                color: loopEnabled.value ? Theme.accent : "transparent"
                radius: width / 2

                Behavior on border.color {
                    ColorAnimation {
                        duration: Theme.motionFast
                    }
                }
                Behavior on color {
                    ColorAnimation {
                        duration: Theme.motionFast
                    }
                }
            }
        }
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 13
            Layout.preferredHeight: root.rowHeight
            spacing: 2

            Skin.ControlButton {
                id: loopInButton

                Accessible.description: qsTr("Right click jumps to the loop in point")
                Accessible.name: qsTr("Loop in")
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 28
                activeColor: Theme.textOnAccent
                enabled: root.loaded
                group: root.group
                implicitHeight: root.rowHeight
                key: "loop_in"
                normalColor: root.buttonColor
                text: qsTr("In")

                Skin.ControlProxyButtonBehavior {
                    id: loopInGotoBehavior

                    group: root.group
                    handlePointerInput: false
                    key: "loop_in"
                    rightClickKey: "loop_in_goto"
                }
                MouseArea {
                    acceptedButtons: Qt.RightButton
                    anchors.fill: parent
                    enabled: root.loaded

                    onCanceled: {
                        loopInGotoBehavior.releaseSecondary();
                    }
                    onPressed: function (mouse) {
                        loopInGotoBehavior.pressSecondary(mouse.x, mouse.y);
                    }
                    onReleased: {
                        loopInGotoBehavior.releaseSecondary();
                    }
                }
            }
            Skin.ControlButton {
                id: loopOutButton

                Accessible.description: qsTr("Right click jumps to the loop out point")
                Accessible.name: qsTr("Loop out")
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 28
                activeColor: Theme.textOnAccent
                enabled: root.loaded
                group: root.group
                implicitHeight: root.rowHeight
                key: "loop_out"
                normalColor: root.buttonColor
                text: qsTr("Out")

                Skin.ControlProxyButtonBehavior {
                    id: loopOutGotoBehavior

                    group: root.group
                    handlePointerInput: false
                    key: "loop_out"
                    rightClickKey: "loop_out_goto"
                }
                MouseArea {
                    acceptedButtons: Qt.RightButton
                    anchors.fill: parent
                    enabled: root.loaded

                    onCanceled: {
                        loopOutGotoBehavior.releaseSecondary();
                    }
                    onPressed: function (mouse) {
                        loopOutGotoBehavior.pressSecondary(mouse.x, mouse.y);
                    }
                    onReleased: {
                        loopOutGotoBehavior.releaseSecondary();
                    }
                }
            }
            Skin.ControlButton {
                id: loopRecallButton

                Accessible.description: qsTr("Right click enables the loop, jumps to the loop in point and stops playback")
                Accessible.name: loopEnabled.value ? qsTr("Exit loop") : qsTr("Reloop")
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 40
                ToolTip.delay: 500
                ToolTip.text: loopEnabled.value ? qsTr("Exit loop") : qsTr("Reloop")
                ToolTip.visible: loopRecallButton.hovered
                activeColor: Theme.textOnAccent
                enabled: root.loaded
                group: root.group
                implicitHeight: root.rowHeight
                key: loopEnabled.value ? "loop_enabled" : "reloop_toggle"
                normalColor: root.buttonColor
                // "Rel" is the hardware shorthand; the full word elides to
                // "REL…" at this panel width.
                text: loopEnabled.value ? qsTr("Exit") : qsTr("Rel")
                toggleable: loopEnabled.value

                // Right click reloops and stops, the classic gesture.
                MouseArea {
                    acceptedButtons: Qt.RightButton
                    anchors.fill: parent
                    enabled: root.loaded

                    onPressed: reloopAndStopControl.trigger()

                    Mixxx.ControlProxy {
                        id: reloopAndStopControl

                        group: root.group
                        key: "reloop_andstop"
                    }
                }
            }
        }
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 13
            Layout.preferredHeight: root.rowHeight
            spacing: 1

            Skin.ControlButton {
                id: loopSizeHalveButton

                Accessible.description: qsTr("Halves the current loop's length by moving the end marker")
                Accessible.name: qsTr("Loop halve")
                Layout.fillHeight: true
                Layout.preferredWidth: 14
                ToolTip.delay: 500
                ToolTip.text: qsTr("Loop Halve")
                ToolTip.visible: loopSizeHalveButton.hovered
                enabled: root.loaded
                group: root.group
                implicitHeight: root.rowHeight
                implicitWidth: 14
                key: "loop_halve"
                text: qsTr("◀")
            }
            Repeater {
                model: root.fixedSizes

                Skin.ControlButton {
                    id: fixedLoopButton

                    required property double modelData

                    Accessible.description: qsTr("Starts or exits a loop over %1 beats").arg(beatloopSize.formatBeatSize(fixedLoopButton.modelData))
                    Accessible.name: qsTr("Beatloop %1 beats").arg(beatloopSize.formatBeatSize(fixedLoopButton.modelData))
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Beatloop")
                    ToolTip.visible: fixedLoopButton.hovered
                    enabled: root.loaded
                    group: root.group
                    highlight: fixedLoopEnabled.value > 0
                    implicitHeight: root.rowHeight
                    key: `beatloop_${fixedLoopButton.modelData}_toggle`
                    text: beatloopSize.formatBeatSize(fixedLoopButton.modelData)

                    // Tight face: at fontScale 1.35 the default 6 px label
                    // margins elide "1/2" in a 29 px key.
                    contentItem: Text {
                        anchors.fill: parent
                        anchors.leftMargin: 1
                        anchors.rightMargin: 1
                        color: fixedLoopButton.labelColor
                        elide: Text.ElideRight
                        font.capitalization: Font.AllUppercase
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Theme.fontWeightDemiBold
                        horizontalAlignment: Text.AlignHCenter
                        text: fixedLoopButton.text
                        verticalAlignment: Text.AlignVCenter
                    }

                    Mixxx.ControlProxy {
                        id: fixedLoopEnabled

                        group: root.group
                        key: `beatloop_${fixedLoopButton.modelData}_enabled`
                    }
                }
            }
            Skin.ControlButton {
                id: loopSizeDoubleButton

                Accessible.description: qsTr("Doubles the current loop's length by moving the end marker")
                Accessible.name: qsTr("Loop double")
                Layout.fillHeight: true
                Layout.preferredWidth: 14
                ToolTip.delay: 500
                ToolTip.text: qsTr("Loop Double")
                ToolTip.visible: loopSizeDoubleButton.hovered
                enabled: root.loaded
                group: root.group
                implicitHeight: root.rowHeight
                implicitWidth: 14
                key: "loop_double"
                text: qsTr("▶")
            }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                onWheel: event => {
                    beatloopSize.step(event.angleDelta.y < 0 ? -1 : 1);
                }
            }
        }
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 13
            Layout.preferredHeight: root.rowHeight
            spacing: 2

            Text {
                Layout.preferredWidth: 30
                color: Theme.textMuted
                elide: Text.ElideRight
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Math.min(Theme.fontSizeXs, 10)
                text: qsTr("Roll")
                verticalAlignment: Text.AlignVCenter
            }
            Repeater {
                model: root.rollSizes

                Skin.ControlButton {
                    id: rollButton

                    required property double modelData

                    Accessible.description: qsTr("Hold to roll a loop over %1 beats. Playback resumes where the track would have been if it had not entered the loop.").arg(beatloopSize.formatBeatSize(rollButton.modelData))
                    Accessible.name: qsTr("Loop roll %1 beats").arg(beatloopSize.formatBeatSize(rollButton.modelData))
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Loop roll")
                    ToolTip.visible: rollButton.hovered
                    enabled: root.loaded
                    group: root.group
                    implicitHeight: root.rowHeight
                    key: `beatlooproll_${rollButton.modelData}_activate`
                    text: beatloopSize.formatBeatSize(rollButton.modelData)
                }
            }
        }
    }
}
