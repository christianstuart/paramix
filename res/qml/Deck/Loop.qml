import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2
import QtQuick.Controls 2.12
import QtQuick.Layouts
import "../Theme"

/// Loop card: the full height of the deck transport band.
///
/// Four rows: a header with the editable beat size and the anchor, the eight
/// fixed beat sizes, the In / Out / Loop / Reloop transport row, and the
/// loop-roll row. Every label is measured against the card's inner width and
/// never elided; the jump-to and reloop-and-stop actions sit on the right
/// button of their key so the rows fit whole words.
Rectangle {
    id: root

    property color buttonColor: root.loaded ? Theme.textSecondary : Theme.textMuted
    /// Sizes offered as fixed keys, 1/8 to 16. Halve, double and the editable
    /// size field stay reachable across the full engine range (1/32 to 512).
    readonly property var fixedSizes: [1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16]
    required property string group
    readonly property bool loaded: trackLoadedControl.value > 0
    readonly property var rollSizes: [0.5, 1, 2, 4]

    /// Width between the border and the insets, and the key widths derived from
    /// it. Each key also takes its own label's measured width as a floor, so
    /// the rows can only overflow if the labels no longer fit the card at all.
    readonly property real innerWidth: Math.max(0, width - 2 * (1 + Theme.deckLoopInset))
    readonly property real sizeKeyWidth: Math.max(0, (innerWidth
            - (fixedSizes.length - 1) * Theme.deckLoopKeyGap) / fixedSizes.length)
    /// In, Out, Loop and Reloop share the action row in four equal keys.
    readonly property real actionKeyWidth: Math.max(0, (innerWidth
            - 3 * Theme.deckLoopKeyGap) / 4)
    /// RELOOP keeps its measured width when a quarter of the row is too narrow
    /// for it; IN, OUT and LOOP then share what is left.
    readonly property real reloopWidth: Math.max(actionKeyWidth,
            reloopMeasure.implicitWidth + 2 * Theme.deckLoopKeyPadding)
    readonly property real otherActionWidth: Math.max(0, (innerWidth
            - 3 * Theme.deckLoopKeyGap - reloopWidth) / 3)
    readonly property real rollKeyWidth: Math.max(0, (innerWidth
            - rollCaption.implicitWidth - rollSizes.length * Theme.deckLoopKeyGap)
            / rollSizes.length)

    /// A key's label: the button's own text, centred and never elided. Keys take
    /// their width from this label's implicitWidth plus Theme.deckLoopKeyPadding.
    component KeyLabel: Text {
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    /// Right-click half of an action key: presses the secondary control for the
    /// duration of the press. Accepting only the right button lets the left
    /// click fall through to the key underneath. Disabled parents disable it.
    component SecondaryKeyArea: MouseArea {
        id: secondaryArea

        required property string controlGroup
        required property string controlKey

        acceptedButtons: Qt.RightButton
        anchors.fill: parent
        onCanceled: secondaryControl.value = 0
        onPressed: secondaryControl.value = 1
        onReleased: secondaryControl.value = 0

        Mixxx.ControlProxy {
            id: secondaryControl

            group: secondaryArea.controlGroup
            key: secondaryArea.controlKey
        }
    }

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
    /// Longest value the size field can show, measured for the field's width.
    TextMetrics {
        id: sizeFieldMetrics

        font.family: Theme.fontFamilyMono
        font.pixelSize: Theme.fontSizeXs
        text: "1/32"
    }
    /// Longest action label, always RELOOP: it is the only key that may need
    /// more than a quarter of the action row.
    Text {
        id: reloopMeasure

        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        text: qsTr("Reloop")
        visible: false
    }
    // 1 px border plus the inset on each side.
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
            spacing: Theme.deckLoopKeyGap

            Text {
                Layout.fillHeight: true
                Layout.minimumWidth: implicitWidth
                color: Theme.textMuted
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                text: qsTr("Loop")
                verticalAlignment: Text.AlignVCenter
            }
            // Halve, editable size and double joined as one control.
            Rectangle {
                id: sizeStepper

                Layout.preferredHeight: Theme.deckLoopHeaderHeight
                Layout.preferredWidth: stepperRow.implicitWidth
                border.color: sizeInput.activeFocus ? Theme.focusRing : Theme.border
                border.width: 1
                color: Theme.surface2
                radius: Theme.radiusSm

                ToolTip.delay: 500
                ToolTip.text: qsTr("Beatloop Size")
                ToolTip.visible: sizeHover.hovered

                HoverHandler {
                    id: sizeHover
                }
                RowLayout {
                    id: stepperRow

                    anchors.fill: parent
                    spacing: 0

                    Skin.ControlButton {
                        id: loopSizeHalveButton

                        readonly property real keyWidth: Math.max(
                                contentItem.implicitWidth + 2 * Theme.deckLoopKeyPadding,
                                Theme.deckLoopHeaderHeight)

                        Accessible.description: qsTr("Halves the current loop's length by moving the end marker")
                        Accessible.name: qsTr("Loop halve")
                        Layout.fillHeight: true
                        Layout.preferredWidth: loopSizeHalveButton.keyWidth
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Loop Halve")
                        ToolTip.visible: loopSizeHalveButton.hovered
                        contentItem: KeyLabel {
                            color: loopSizeHalveButton.labelColor
                            text: loopSizeHalveButton.text
                        }
                        enabled: root.loaded
                        flat: true
                        group: root.group
                        implicitHeight: Theme.deckLoopHeaderHeight
                        implicitWidth: loopSizeHalveButton.keyWidth
                        key: "loop_halve"
                        text: qsTr("◀")
                    }
                    // The size is editable: 3 or 1/3 stick, which the power-of-two
                    // fixed keys cannot express. Enter or focus loss commits.
                    TextInput {
                        id: sizeInput

                        function update() {
                            this.text = Qt.binding(function () {
                                return beatloopSize.valueText;
                            });
                        }

                        Accessible.description: qsTr("Select the size of the loop in beats to set with the Loop key")
                        Accessible.name: qsTr("Beatloop size")
                        Layout.fillHeight: true
                        Layout.preferredWidth: Math.ceil(sizeFieldMetrics.width)
                                + 2 * Theme.deckLoopKeyPadding + Theme.space1
                        color: root.loaded ? Theme.textSecondary : Theme.textMuted
                        enabled: root.loaded
                        font.capitalization: Font.MixedCase
                        font.family: Theme.fontFamilyMono
                        font.pixelSize: Theme.fontSizeXs
                        horizontalAlignment: TextInput.AlignHCenter
                        leftPadding: Theme.deckLoopKeyPadding
                        rightPadding: Theme.deckLoopKeyPadding
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
                    Skin.ControlButton {
                        id: loopSizeDoubleButton

                        readonly property real keyWidth: Math.max(
                                contentItem.implicitWidth + 2 * Theme.deckLoopKeyPadding,
                                Theme.deckLoopHeaderHeight)

                        Accessible.description: qsTr("Doubles the current loop's length by moving the end marker")
                        Accessible.name: qsTr("Loop double")
                        Layout.fillHeight: true
                        Layout.preferredWidth: loopSizeDoubleButton.keyWidth
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Loop Double")
                        ToolTip.visible: loopSizeDoubleButton.hovered
                        contentItem: KeyLabel {
                            color: loopSizeDoubleButton.labelColor
                            text: loopSizeDoubleButton.text
                        }
                        enabled: root.loaded
                        flat: true
                        group: root.group
                        implicitHeight: Theme.deckLoopHeaderHeight
                        implicitWidth: loopSizeDoubleButton.keyWidth
                        key: "loop_double"
                        text: qsTr("▶")
                    }
                }
            }
            Skin.ControlButton {
                id: loopAnchorButton

                readonly property real keyWidth: Math.max(
                        contentItem.implicitWidth + 2 * Theme.deckLoopKeyPadding,
                        Theme.deckLoopHeaderHeight)

                Accessible.description: qsTr("Define whether the loop is created and adjusted from its starting point or ending point")
                Accessible.name: qsTr("Beatloop anchor")
                Layout.fillHeight: true
                Layout.preferredWidth: loopAnchorButton.keyWidth
                ToolTip.delay: 500
                ToolTip.text: qsTr("Beatloop Anchor")
                ToolTip.visible: loopAnchorButton.hovered
                contentItem: KeyLabel {
                    color: loopAnchorButton.labelColor
                    text: loopAnchorButton.text
                }
                enabled: root.loaded
                group: root.group
                implicitHeight: Theme.deckLoopHeaderHeight
                implicitWidth: loopAnchorButton.keyWidth
                key: "loop_anchor"
                text: qsTr("A")
                toggleable: true
            }
            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }
            Rectangle {
                Layout.preferredHeight: Theme.deckLoopStatusDotSize
                Layout.preferredWidth: Theme.deckLoopStatusDotSize
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
        // Eight equal-width fixed sizes. Larger and smaller sizes stay reachable
        // through halve / double and the editable size field.
        Row {
            id: fixedSizeRow

            Layout.fillWidth: true
            Layout.preferredHeight: Theme.deckLoopKeyRowHeight
            spacing: Theme.deckLoopKeyGap

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
                    contentItem: KeyLabel {
                        color: fixedLoopButton.labelColor
                        text: fixedLoopButton.text
                    }
                    enabled: root.loaded
                    group: root.group
                    height: fixedSizeRow.height
                    highlight: fixedLoopEnabled.value > 0
                    implicitWidth: width
                    key: `beatloop_${fixedLoopButton.modelData}_toggle`
                    text: beatloopSize.formatBeatSize(fixedLoopButton.modelData)
                    width: Math.max(root.sizeKeyWidth,
                            contentItem.implicitWidth + 2 * Theme.deckLoopKeyPadding)

                    Mixxx.ControlProxy {
                        id: fixedLoopEnabled

                        group: root.group
                        key: `beatloop_${fixedLoopButton.modelData}_enabled`
                    }
                }
            }
        }
        Row {
            id: actionRow

            Layout.fillWidth: true
            Layout.preferredHeight: Theme.deckLoopKeyRowHeight
            spacing: Theme.deckLoopKeyGap

            Skin.ControlButton {
                id: loopInButton

                Accessible.description: qsTr("Set loop in. Right-click: jump to loop in.")
                Accessible.name: qsTr("Loop in")
                ToolTip.delay: 500
                ToolTip.text: qsTr("Set loop in. Right-click: jump to loop in.")
                ToolTip.visible: loopInButton.hovered
                activeColor: Theme.textOnAccent
                contentItem: KeyLabel {
                    color: loopInButton.labelColor
                    text: loopInButton.text
                }
                enabled: root.loaded
                group: root.group
                height: actionRow.height
                implicitWidth: width
                key: "loop_in"
                normalColor: root.buttonColor
                text: qsTr("In")
                width: Math.max(root.otherActionWidth,
                        contentItem.implicitWidth + 2 * Theme.deckLoopKeyPadding)

                SecondaryKeyArea {
                    controlGroup: root.group
                    controlKey: "loop_in_goto"
                }
            }
            Skin.ControlButton {
                id: loopOutButton

                Accessible.description: qsTr("Set loop out. Right-click: jump to loop out.")
                Accessible.name: qsTr("Loop out")
                ToolTip.delay: 500
                ToolTip.text: qsTr("Set loop out. Right-click: jump to loop out.")
                ToolTip.visible: loopOutButton.hovered
                activeColor: Theme.textOnAccent
                contentItem: KeyLabel {
                    color: loopOutButton.labelColor
                    text: loopOutButton.text
                }
                enabled: root.loaded
                group: root.group
                height: actionRow.height
                implicitWidth: width
                key: "loop_out"
                normalColor: root.buttonColor
                text: qsTr("Out")
                width: Math.max(root.otherActionWidth,
                        contentItem.implicitWidth + 2 * Theme.deckLoopKeyPadding)

                SecondaryKeyArea {
                    controlGroup: root.group
                    controlKey: "loop_out_goto"
                }
            }
            Skin.ControlButton {
                id: loopActivateButton

                Accessible.description: qsTr("Start a beatloop. Right-click: reloop and stop.")
                Accessible.name: qsTr("Beatloop")
                ToolTip.delay: 500
                ToolTip.text: qsTr("Start a beatloop. Right-click: reloop and stop.")
                ToolTip.visible: loopActivateButton.hovered
                contentItem: KeyLabel {
                    color: loopActivateButton.labelColor
                    text: loopActivateButton.text
                }
                enabled: root.loaded
                group: root.group
                height: actionRow.height
                implicitWidth: width
                key: "beatloop_activate"
                text: qsTr("Loop")
                width: Math.max(root.otherActionWidth,
                        contentItem.implicitWidth + 2 * Theme.deckLoopKeyPadding)

                SecondaryKeyArea {
                    controlGroup: root.group
                    controlKey: "reloop_andstop"
                }
            }
            Skin.ControlButton {
                id: loopRecallButton

                Accessible.description: loopEnabled.value ? qsTr("Exit the loop. Right-click: reloop and stop.") : qsTr("Reloop the last loop. Right-click: reloop and stop.")
                Accessible.name: loopEnabled.value ? qsTr("Exit loop") : qsTr("Reloop")
                ToolTip.delay: 500
                ToolTip.text: loopEnabled.value ? qsTr("Exit the loop. Right-click: reloop and stop.") : qsTr("Reloop the last loop. Right-click: reloop and stop.")
                ToolTip.visible: loopRecallButton.hovered
                activeColor: Theme.textOnAccent
                contentItem: KeyLabel {
                    color: loopRecallButton.labelColor
                    text: loopRecallButton.text
                }
                enabled: root.loaded
                group: root.group
                height: actionRow.height
                implicitWidth: width
                key: loopEnabled.value ? "loop_enabled" : "reloop_toggle"
                normalColor: root.buttonColor
                text: loopEnabled.value ? qsTr("Exit") : qsTr("Reloop")
                toggleable: loopEnabled.value
                width: root.reloopWidth

                SecondaryKeyArea {
                    controlGroup: root.group
                    controlKey: "reloop_andstop"
                }
            }
        }
        Row {
            id: rollRow

            Layout.fillWidth: true
            Layout.preferredHeight: Theme.deckLoopRollRowHeight
            spacing: Theme.deckLoopKeyGap

            Text {
                id: rollCaption

                color: Theme.textMuted
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                height: rollRow.height
                text: qsTr("Roll")
                verticalAlignment: Text.AlignVCenter
                width: implicitWidth
            }
            Repeater {
                model: root.rollSizes

                Skin.ControlButton {
                    id: rollButton

                    required property double modelData

                    Accessible.description: qsTr("Hold to roll a loop over %1 beats. Playback resumes where the track would have been if it had not entered the loop.").arg(beatloopSize.formatBeatSize(rollButton.modelData))
                    Accessible.name: qsTr("Loop roll %1 beats").arg(beatloopSize.formatBeatSize(rollButton.modelData))
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Loop roll")
                    ToolTip.visible: rollButton.hovered
                    contentItem: KeyLabel {
                        color: rollButton.labelColor
                        text: rollButton.text
                    }
                    enabled: root.loaded
                    group: root.group
                    height: rollRow.height
                    implicitWidth: width
                    key: `beatlooproll_${rollButton.modelData}_activate`
                    text: beatloopSize.formatBeatSize(rollButton.modelData)
                    width: Math.max(root.rollKeyWidth,
                            contentItem.implicitWidth + 2 * Theme.deckLoopKeyPadding)
                }
            }
        }
    }
}
