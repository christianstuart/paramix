import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2
import QtQuick.Controls 2.12
import QtQuick.Layouts
import "../Theme"

/// Loop row (Traktor style): the beatloop size strip, the anchor, and In /
/// Out / Loop / Reloop. Labels are measured and never elided; the jump-to and
/// reloop-and-stop actions sit on the right button of their key. Reloop turns
/// into a lit Exit while a loop plays, so the loop state stays visible.
Item {
    id: root

    property color buttonColor: root.loaded ? Theme.textSecondary : Theme.textMuted
    required property string group
    readonly property bool loaded: trackLoadedControl.value > 0

    /// A key's label: the button's own text, centred and never elided.
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

    /// One loop key, as wide as its label.
    component LoopKey: Skin.ControlButton {
        id: loopKey

        Layout.fillHeight: true
        Layout.minimumWidth: loopKey.keyWidth
        Layout.preferredWidth: loopKey.keyWidth
        activeColor: Theme.textOnAccent
        contentItem: KeyLabel {
            id: keyText

            color: loopKey.labelColor
            text: loopKey.text
        }
        enabled: root.loaded
        group: root.group
        implicitWidth: loopKey.keyWidth
        normalColor: root.buttonColor
        ToolTip.delay: 500
        ToolTip.visible: loopKey.hovered

        property real keyWidth: Math.max(Theme.deckLoopKeyMinWidth,
                keyText.implicitWidth + 4 * Theme.deckLoopKeyPadding)
    }

    /// RELOOP is the wider of Reloop / Exit, so swapping the label never
    /// reflows the row.
    KeyLabel {
        id: reloopMeasure

        text: qsTr("Reloop")
        visible: false
    }

    implicitWidth: row.implicitWidth

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
    RowLayout {
        id: row

        anchors.fill: parent
        spacing: Theme.deckLoopKeyGap

        SizeStrip {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumWidth: implicitWidth
            beatSizes: [1 / 32, 1 / 16, 1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512]
            decrementKey: "loop_halve"
            group: root.group
            incrementKey: "loop_double"
            key: "beatloop_size"
            maxKeys: 6
            title: qsTr("Beatloop size")
        }
        LoopKey {
            Accessible.description: qsTr("Define whether the loop is created and adjusted from its starting point or ending point")
            Accessible.name: qsTr("Beatloop anchor")
            ToolTip.text: qsTr("Beatloop Anchor")
            key: "loop_anchor"
            text: qsTr("A")
            toggleable: true
        }
        LoopKey {
            Accessible.description: qsTr("Set loop in. Right-click: jump to loop in.")
            Accessible.name: qsTr("Loop in")
            ToolTip.text: qsTr("Set loop in. Right-click: jump to loop in.")
            key: "loop_in"
            text: qsTr("In")

            SecondaryKeyArea {
                controlGroup: root.group
                controlKey: "loop_in_goto"
            }
        }
        LoopKey {
            Accessible.description: qsTr("Set loop out. Right-click: jump to loop out.")
            Accessible.name: qsTr("Loop out")
            ToolTip.text: qsTr("Set loop out. Right-click: jump to loop out.")
            key: "loop_out"
            text: qsTr("Out")

            SecondaryKeyArea {
                controlGroup: root.group
                controlKey: "loop_out_goto"
            }
        }
        LoopKey {
            Accessible.description: qsTr("Start a beatloop. Right-click: reloop and stop.")
            Accessible.name: qsTr("Beatloop")
            ToolTip.text: qsTr("Start a beatloop. Right-click: reloop and stop.")
            key: "beatloop_activate"
            text: qsTr("Loop")

            SecondaryKeyArea {
                controlGroup: root.group
                controlKey: "reloop_andstop"
            }
        }
        LoopKey {
            id: loopRecallButton

            Accessible.description: loopEnabled.value ? qsTr("Exit the loop. Right-click: reloop and stop.") : qsTr("Reloop the last loop. Right-click: reloop and stop.")
            Accessible.name: loopEnabled.value ? qsTr("Exit loop") : qsTr("Reloop")
            ToolTip.text: loopEnabled.value ? qsTr("Exit the loop. Right-click: reloop and stop.") : qsTr("Reloop the last loop. Right-click: reloop and stop.")
            key: loopEnabled.value ? "loop_enabled" : "reloop_toggle"
            keyWidth: reloopMeasure.implicitWidth + 4 * Theme.deckLoopKeyPadding
            text: loopEnabled.value ? qsTr("Exit") : qsTr("Reloop")
            toggleable: loopEnabled.value

            SecondaryKeyArea {
                controlGroup: root.group
                controlKey: "reloop_andstop"
            }
        }
    }
}
