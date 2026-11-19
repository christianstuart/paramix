import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2
import QtQuick.Controls 2.12
import QtQuick.Layouts
import "../Theme"

/// Loop card: the full height of the deck transport band.
///
/// Four rows: a header with the editable beat size and the anchor, a scrolling
/// row of fixed beat sizes with halve and double pinned at the ends, the
/// In / Out / Loop / Rel / Stop transport row, and the loop-roll row. The
/// beat-size row scrolls with a visible scrollbar; every former right-click
/// action has its own visible key.
Rectangle {
    id: root

    property color buttonColor: root.loaded ? Theme.textSecondary : Theme.textMuted
    /// Sizes offered as fixed keys, 1/8 to 32. The size field and the wheel
    /// reach the full engine range (1/32 to 512 beats).
    readonly property var fixedSizes: [1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32]
    required property string group
    readonly property bool loaded: trackLoadedControl.value > 0
    readonly property var rollSizes: [0.5, 1, 2, 4]

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
    // 1 px border plus the 2 px inset on each side.
    ColumnLayout {
        anchors.bottomMargin: 1 + Theme.deckLoopInset
        anchors.fill: parent
        anchors.leftMargin: 1 + Theme.deckLoopInset
        anchors.rightMargin: 1 + Theme.deckLoopInset
        anchors.topMargin: 1 + Theme.deckLoopInset
        spacing: Theme.deckLoopRowGap

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.deckLoopHeaderHeight
            spacing: Theme.space1

            Text {
                Layout.preferredWidth: 30
                color: Theme.textMuted
                elide: Text.ElideRight
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Math.min(Theme.fontSizeXs, Theme.deckLoopHeaderHeight - 4)
                text: qsTr("Loop")
                verticalAlignment: Text.AlignVCenter
            }
            // The size is editable: 3 or 1/3 stick, which the power-of-two
            // fixed keys cannot express. Enter or focus loss commits.
            Rectangle {
                id: sizeField

                Layout.preferredHeight: Theme.deckLoopHeaderHeight
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

                    Accessible.description: qsTr("Select the size of the loop in beats to set with the Loop key")
                    Accessible.name: qsTr("Beatloop size")
                    anchors.fill: parent
                    color: root.loaded ? Theme.textSecondary : Theme.textMuted
                    enabled: root.loaded
                    font.capitalization: Font.MixedCase
                    font.family: Theme.fontFamilyMono
                    font.pixelSize: Math.min(Theme.fontSizeXs, Theme.deckLoopHeaderHeight - 4)
                    horizontalAlignment: TextInput.AlignHCenter
                    leftPadding: 2
                    rightPadding: 2
                    selectByMouse: true
                    text: beatloopSize.valueText
                    verticalAlignment: TextInput.AlignVCenter

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
                Layout.preferredHeight: Theme.deckLoopHeaderHeight
                Layout.preferredWidth: 18
                ToolTip.delay: 500
                ToolTip.text: qsTr("Beatloop Anchor")
                ToolTip.visible: loopAnchorButton.hovered
                enabled: root.loaded
                group: root.group
                implicitHeight: Theme.deckLoopHeaderHeight
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
        // Scrolling fixed beat sizes with halve and double pinned at the ends.
        // The scrollbar under the viewport is the visible affordance; wheel and
        // drag only make it faster.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.deckLoopBeatRowHeight

            RowLayout {
                id: beatKeysRow

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: parent.height - 6
                spacing: 2

                Skin.ControlButton {
                    id: loopSizeHalveButton

                    Accessible.description: qsTr("Halves the current loop's length by moving the end marker")
                    Accessible.name: qsTr("Loop halve")
                    Layout.fillHeight: true
                    Layout.preferredWidth: 16
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Loop Halve")
                    ToolTip.visible: loopSizeHalveButton.hovered
                    enabled: root.loaded
                    group: root.group
                    implicitHeight: beatKeysRow.height
                    implicitWidth: 16
                    key: "loop_halve"
                    text: qsTr("◀")
                }
                Flickable {
                    id: beatFlick

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true
                    contentHeight: height
                    contentWidth: beatContent.width
                    flickableDirection: Flickable.HorizontalFlick

                    Row {
                        id: beatContent

                        height: beatFlick.height
                        spacing: 2

                        Repeater {
                            model: root.fixedSizes

                            Skin.ControlButton {
                                id: fixedLoopButton

                                required property double modelData

                                Accessible.description: qsTr("Starts or exits a loop over %1 beats").arg(beatloopSize.formatBeatSize(fixedLoopButton.modelData))
                                Accessible.name: qsTr("Beatloop %1 beats").arg(beatloopSize.formatBeatSize(fixedLoopButton.modelData))
                                ToolTip.delay: 500
                                ToolTip.text: qsTr("Beatloop")
                                ToolTip.visible: fixedLoopButton.hovered
                                enabled: root.loaded
                                group: root.group
                                height: beatContent.height
                                highlight: fixedLoopEnabled.value > 0
                                key: `beatloop_${fixedLoopButton.modelData}_toggle`
                                text: beatloopSize.formatBeatSize(fixedLoopButton.modelData)
                                width: 36

                                contentItem: Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 1
                                    anchors.rightMargin: 1
                                    color: fixedLoopButton.labelColor
                                    elide: Text.ElideRight
                                    font.capitalization: Font.AllUppercase
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Math.min(Theme.fontSizeXs, fixedLoopButton.height - 8)
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
                    }
                }
                Skin.ControlButton {
                    id: loopSizeDoubleButton

                    Accessible.description: qsTr("Doubles the current loop's length by moving the end marker")
                    Accessible.name: qsTr("Loop double")
                    Layout.fillHeight: true
                    Layout.preferredWidth: 16
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Loop Double")
                    ToolTip.visible: loopSizeDoubleButton.hovered
                    enabled: root.loaded
                    group: root.group
                    implicitHeight: beatKeysRow.height
                    implicitWidth: 16
                    key: "loop_double"
                    text: qsTr("▶")
                }
            }
            // Visible scrollbar for the beat-size viewport, not a hidden wheel
            // gesture. Wheel and drag remain accelerators.
            Rectangle {
                id: scrollTrack

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.right: parent.right
                anchors.rightMargin: 18
                color: Theme.surface3
                height: 3
                radius: Theme.radiusXs
                visible: beatFlick.contentWidth > beatFlick.width

                Rectangle {
                    color: Theme.borderStrong
                    height: parent.height
                    radius: Theme.radiusXs
                    width: Math.max(12, scrollTrack.width * beatFlick.width
                            / Math.max(1, beatFlick.contentWidth))
                    x: Math.max(0, Math.min(scrollTrack.width - width,
                            beatFlick.contentX / Math.max(1, beatFlick.contentWidth) * scrollTrack.width))
                }
            }
            WheelHandler {
                onWheel: event => {
                    const max = Math.max(0, beatFlick.contentWidth - beatFlick.width);
                    beatFlick.contentX = Math.max(0, Math.min(max, beatFlick.contentX - event.angleDelta.y));
                    event.accepted = true;
                }
            }
        }
        RowLayout {
            Layout.fillHeight: false
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.deckLoopActionRowHeight
            spacing: 2

            // In and Out are split keys: the main face sets the point, the
            // visible chevron segment jumps to it.
            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1

                Skin.ControlButton {
                    id: loopInButton

                    Accessible.description: qsTr("Sets the loop in point. The arrow key beside it jumps there.")
                    Accessible.name: qsTr("Loop in")
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 28
                    activeColor: Theme.textOnAccent
                    enabled: root.loaded
                    group: root.group
                    implicitHeight: Theme.deckLoopActionRowHeight
                    key: "loop_in"
                    normalColor: root.buttonColor
                    text: qsTr("In")
                }
                Skin.ControlButton {
                    id: loopInGotoButton

                    Accessible.name: qsTr("Jump to loop in")
                    Layout.fillHeight: true
                    Layout.preferredWidth: 14
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Jump to loop in")
                    ToolTip.visible: loopInGotoButton.hovered
                    enabled: root.loaded
                    group: root.group
                    implicitHeight: Theme.deckLoopActionRowHeight
                    implicitWidth: 14
                    key: "loop_in_goto"
                    text: "^"
                }
            }
            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1

                Skin.ControlButton {
                    id: loopOutButton

                    Accessible.description: qsTr("Sets the loop out point. The arrow key beside it jumps there.")
                    Accessible.name: qsTr("Loop out")
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 28
                    activeColor: Theme.textOnAccent
                    enabled: root.loaded
                    group: root.group
                    implicitHeight: Theme.deckLoopActionRowHeight
                    key: "loop_out"
                    normalColor: root.buttonColor
                    text: qsTr("Out")
                }
                Skin.ControlButton {
                    id: loopOutGotoButton

                    Accessible.name: qsTr("Jump to loop out")
                    Layout.fillHeight: true
                    Layout.preferredWidth: 14
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Jump to loop out")
                    ToolTip.visible: loopOutGotoButton.hovered
                    enabled: root.loaded
                    group: root.group
                    implicitHeight: Theme.deckLoopActionRowHeight
                    implicitWidth: 14
                    key: "loop_out_goto"
                    text: "v"
                }
            }
            Skin.ControlButton {
                id: loopActivateButton

                Accessible.description: qsTr("Start a loop over the set number of beats")
                Accessible.name: qsTr("Beatloop")
                Layout.fillHeight: true
                Layout.preferredWidth: 42
                ToolTip.delay: 500
                ToolTip.text: qsTr("Beatloop")
                ToolTip.visible: loopActivateButton.hovered
                enabled: root.loaded
                group: root.group
                implicitHeight: Theme.deckLoopActionRowHeight
                implicitWidth: 42
                key: "beatloop_activate"
                text: qsTr("Loop")
            }
            Skin.ControlButton {
                id: loopRecallButton

                Accessible.description: qsTr("Exit the active loop, or reloop the last one")
                Accessible.name: loopEnabled.value ? qsTr("Exit loop") : qsTr("Reloop")
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 36
                ToolTip.delay: 500
                ToolTip.text: loopEnabled.value ? qsTr("Exit loop") : qsTr("Reloop")
                ToolTip.visible: loopRecallButton.hovered
                activeColor: Theme.textOnAccent
                enabled: root.loaded
                group: root.group
                implicitHeight: Theme.deckLoopActionRowHeight
                key: loopEnabled.value ? "loop_enabled" : "reloop_toggle"
                normalColor: root.buttonColor
                text: loopEnabled.value ? qsTr("Exit") : qsTr("Rel")
                toggleable: loopEnabled.value
            }
            Skin.ControlButton {
                id: reloopAndStopButton

                Accessible.description: qsTr("Enable the loop, jump to its in point and stop playback")
                Accessible.name: qsTr("Reloop and stop")
                Layout.fillHeight: true
                Layout.preferredWidth: 38
                ToolTip.delay: 500
                ToolTip.text: qsTr("Reloop and stop")
                ToolTip.visible: reloopAndStopButton.hovered
                enabled: root.loaded
                group: root.group
                implicitHeight: Theme.deckLoopActionRowHeight
                implicitWidth: 38
                key: "reloop_andstop"
                text: qsTr("Stop")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.deckLoopRollRowHeight
            spacing: 2

            Text {
                Layout.preferredWidth: 30
                color: Theme.textMuted
                elide: Text.ElideRight
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Math.min(Theme.fontSizeXs, Theme.deckLoopRollRowHeight - 4)
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
                    implicitHeight: Theme.deckLoopRollRowHeight
                    key: `beatlooproll_${rollButton.modelData}_activate`
                    text: beatloopSize.formatBeatSize(rollButton.modelData)
                }
            }
        }
    }
}
