import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import "../Theme"

/// Master tempo box for the main toolbar.
///
/// Shows the tempo every synced deck follows and chooses where that tempo
/// comes from:
///  - AUTO: a playing track is the tempo master (the deck sync leader).
///  - MASTER: the software master (Mixxx's internal clock) is the tempo
///    master, and the box becomes an editor for its BPM.
///
/// The internal clock mirrors the active leader's tempo, so the readout is the
/// same control in both modes and switching to MASTER does not change the
/// tempo the decks are running at.
Item {
    id: root

    /// True while the internal clock is the sync leader, i.e. the software
    /// master owns the tempo. Read from the engine rather than remembered
    /// locally so it stays correct when leadership moves for other reasons.
    readonly property bool softwareMaster: leaderClock.value > 0
    property bool editing: false

    readonly property var deckPlaying: [play1.value, play2.value, play3.value, play4.value]
    readonly property var deckSync: [sync1.value, sync2.value, sync3.value, sync4.value]
    readonly property var deckBpm: [bpm1.value, bpm2.value, bpm3.value, bpm4.value]
    readonly property var deckLeaderRequests: [leader1, leader2, leader3, leader4]

    implicitHeight: 38
    implicitWidth: 128

    function beginEdit() {
        if (!root.softwareMaster) {
            return;
        }
        root.editing = true;
        editor.text = root.shownBpm.toFixed(2);
        editor.forceActiveFocus(Qt.MouseFocusReason);
        editor.selectAll();
    }
    function cancelEdit() {
        root.editing = false;
        editor.focus = false;
    }
    function commitEdit() {
        const parsed = Number(editor.text.replace(",", "."));
        if (isFinite(parsed) && parsed >= 1 && parsed <= 200) {
            clockBpm.value = parsed;
        }
        root.editing = false;
        editor.focus = false;
    }
    function engageAuto() {
        leaderClock.value = 0;
        // The engine hangs on to the internal clock as a soft leader when it is
        // asked to step down, so hand the master back to a track explicitly:
        // prefer a playing synced deck, otherwise any synced deck.
        let playing = -1;
        let anySynced = -1;
        for (let i = 0; i < root.deckSync.length; ++i) {
            if (root.deckSync[i] <= 0 || root.deckBpm[i] <= 0) {
                continue;
            }
            if (anySynced < 0) {
                anySynced = i;
            }
            if (root.deckPlaying[i] > 0) {
                playing = i;
                break;
            }
        }
        const target = playing >= 0 ? playing : anySynced;
        if (target >= 0) {
            root.deckLeaderRequests[target].value = 1;
        }
    }
    function engageMaster() {
        leaderClock.value = 1;
    }
    function formatBpm(value) {
        if (!(value > 0)) {
            return qsTr("--");
        }
        return value.toFixed(2);
    }

    /// Tempo of the current sync leader. The internal clock is kept in sync
    /// with a deck leader by EngineSync, so one control covers both modes.
    readonly property real shownBpm: clockBpm.value

    Mixxx.ControlProxy {
        id: clockBpm

        group: "[InternalClock]"
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: leaderClock

        group: "[InternalClock]"
        key: "sync_leader"
    }
    Mixxx.ControlProxy {
        id: play1

        group: "[Channel1]"
        key: "play"
    }
    Mixxx.ControlProxy {
        id: play2

        group: "[Channel2]"
        key: "play"
    }
    Mixxx.ControlProxy {
        id: play3

        group: "[Channel3]"
        key: "play"
    }
    Mixxx.ControlProxy {
        id: play4

        group: "[Channel4]"
        key: "play"
    }
    Mixxx.ControlProxy {
        id: sync1

        group: "[Channel1]"
        key: "sync_mode"
    }
    Mixxx.ControlProxy {
        id: sync2

        group: "[Channel2]"
        key: "sync_mode"
    }
    Mixxx.ControlProxy {
        id: sync3

        group: "[Channel3]"
        key: "sync_mode"
    }
    Mixxx.ControlProxy {
        id: sync4

        group: "[Channel4]"
        key: "sync_mode"
    }
    Mixxx.ControlProxy {
        id: bpm1

        group: "[Channel1]"
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: bpm2

        group: "[Channel2]"
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: bpm3

        group: "[Channel3]"
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: bpm4

        group: "[Channel4]"
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: leader1

        group: "[Channel1]"
        key: "sync_leader"
    }
    Mixxx.ControlProxy {
        id: leader2

        group: "[Channel2]"
        key: "sync_leader"
    }
    Mixxx.ControlProxy {
        id: leader3

        group: "[Channel3]"
        key: "sync_leader"
    }
    Mixxx.ControlProxy {
        id: leader4

        group: "[Channel4]"
        key: "sync_leader"
    }

    // ── BPM readout / editor ───────────────────────────────────────────────
    Rectangle {
        id: bpmBox

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        border.color: root.editing ? Theme.focusRing : root.softwareMaster ? Theme.borderStrong : Theme.border
        border.width: 1
        color: Theme.surface0
        height: 22
        radius: Theme.radiusSm

        Text {
            id: valueLabel

            anchors.left: parent.left
            anchors.leftMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            color: root.softwareMaster ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeLg
            font.weight: Theme.fontWeightDemiBold
            text: root.formatBpm(root.shownBpm)
            visible: !root.editing
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: qsTr("BPM")
            visible: !root.editing
        }
        TextInput {
            id: editor

            anchors.fill: parent
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            color: Theme.textPrimary
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeLg
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: TextInput.AlignLeft
            selectByMouse: true
            selectedTextColor: Theme.textOnAccent
            selectionColor: Theme.accent
            validator: DoubleValidator {
                bottom: 1
                decimals: 2
                notation: DoubleValidator.StandardNotation
                top: 200
            }
            verticalAlignment: TextInput.AlignVCenter
            visible: root.editing

            onActiveFocusChanged: {
                if (!activeFocus && root.editing) {
                    root.cancelEdit();
                }
            }
            onAccepted: root.commitEdit()

            Keys.onEscapePressed: root.cancelEdit()
        }
        MouseArea {
            id: bpmMouse

            anchors.fill: parent
            cursorShape: root.softwareMaster ? Qt.IBeamCursor : Qt.ArrowCursor
            enabled: !root.editing

            Accessible.name: qsTr("Tempo master BPM")
            Accessible.role: Accessible.EditableText

            onClicked: root.beginEdit()
        }
    }

    // ── Tempo source switch ────────────────────────────────────────────────
    Rectangle {
        id: modeSwitch

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        border.color: Theme.border
        border.width: 1
        color: Theme.surface2
        height: 14
        radius: Theme.radiusSm

        Row {
            anchors.fill: parent
            anchors.margins: 1

            Repeater {
                model: [qsTr("Auto"), qsTr("Master")]

                Item {
                    required property int index
                    required property string modelData

                    height: parent.height
                    width: parent.width / 2

                    Rectangle {
                        anchors.fill: parent
                        color: root.softwareMaster === (index === 1) ? Theme.accent : "transparent"
                        radius: Theme.radiusXs

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.motionFast
                            }
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        color: root.softwareMaster === (index === 1) ? Theme.textOnAccent : Theme.textMuted
                        font.capitalization: Font.AllUppercase
                        font.family: Theme.fontFamily
                        font.letterSpacing: Theme.letterSpacingCaps
                        font.pixelSize: Theme.fontSizeXs - 1
                        font.weight: Theme.fontWeightDemiBold
                        text: modelData
                    }
                    MouseArea {
                        anchors.fill: parent

                        Accessible.name: index === 1
                                ? qsTr("Use the software master tempo")
                                : qsTr("Use a track as tempo master")
                        Accessible.role: Accessible.Button

                        onClicked: index === 1 ? root.engageMaster() : root.engageAuto()
                    }
                }
            }
        }
    }
}
