import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import "../Theme"

/// Beatgrid pane: BPM readout with tap, grid alignment and nudge, the
/// intro/outro cue markers and the cue-timing shift keys.
///
/// This is the GRID tab of the deck pane stack. It replaces the toolbar's
/// Beatgrid key (which is gone) with explicit Align / Match / Lock / Undo
/// keys, the BPM halve and double actions, the four intro/outro markers and
/// the beatgrid nudge keys, so nothing about the grid is hidden behind a
/// right-click any more.
Rectangle {
    id: root

    readonly property color buttonColor: root.loaded ? Theme.textSecondary : Theme.textMuted
    /// True when the pane is squeezed by the compact deck layout. The BPM
    /// readout drops a size so the readout row + three key rows still fit the
    /// 112 px deck pane.
    readonly property bool dense: root.height < 104
    /// FINE latch: while active the six grid nudge keys send their fine
    /// control (half-beat translate, 1 ms cue shift) instead of the coarse one.
    property bool fineAdjust: false
    required property string group
    readonly property bool loaded: trackLoadedControl.value > 0
    /// CLR latch: while active the four intro/outro marker keys clear instead
    /// of set.
    property bool markerClear: false
    readonly property real visualBpm: visualBpmControl.value

    border.color: Theme.border
    border.width: 1
    clip: true
    color: Theme.surface1
    radius: Theme.radiusMd

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: visualBpmControl

        group: root.group
        key: "visual_bpm"
    }
    Mixxx.ControlProxy {
        id: beatsUndoPossible

        group: root.group
        key: "beats_undo_possible"
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 2
        spacing: 2

        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 26
            spacing: Theme.space1

            ColumnLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                Text {
                    color: Theme.textMuted
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.letterSpacing: Theme.letterSpacingCaps
                    font.pixelSize: Theme.fontSizeXs
                    text: qsTr("BPM")
                }
                Text {
                    color: root.loaded ? Theme.textPrimary : Theme.textMuted
                    font.family: Theme.fontFamilyMono
                    font.pixelSize: root.dense ? Theme.fontSizeLg : Theme.fontSizeXl
                    font.weight: Theme.fontWeightMedium
                    text: root.visualBpm > 0 ? root.visualBpm.toFixed(2) : "--"
                }
            }
            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
            }
            Skin.ControlButton {
                id: bpmHalveButton

                Accessible.description: qsTr("Halve the track BPM, e.g. 174 to 87")
                Accessible.name: qsTr("Halve BPM")
                Layout.alignment: Qt.AlignVCenter
                Layout.minimumWidth: 0
                Layout.preferredHeight: 18
                Layout.preferredWidth: 26
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                implicitWidth: 26
                key: "beats_set_halve"
                text: qsTr("½")
            }
            Skin.ControlButton {
                id: bpmDoubleButton

                Accessible.description: qsTr("Double the track BPM, e.g. 87 to 174")
                Accessible.name: qsTr("Double BPM")
                Layout.alignment: Qt.AlignVCenter
                Layout.minimumWidth: 0
                Layout.preferredHeight: 18
                Layout.preferredWidth: 26
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                implicitWidth: 26
                key: "beats_set_double"
                text: qsTr("×2")
            }
            Skin.ControlButton {
                id: bpmTapButton

                Accessible.description: qsTr("Tap along with the beat to set the BPM")
                Accessible.name: qsTr("Tap tempo")
                Layout.alignment: Qt.AlignVCenter
                Layout.minimumWidth: 0
                Layout.preferredHeight: 18
                Layout.preferredWidth: 40
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                implicitWidth: 40
                key: "bpm_tap"
                text: qsTr("Tap")
            }
            Skin.Button {
                id: fineLatch

                Accessible.checkable: true
                Accessible.checked: root.fineAdjust
                Accessible.description: qsTr("While active, the grid nudge keys shift by half a beat and the cue keys by 1 millisecond")
                Accessible.name: qsTr("Fine grid adjust")
                Accessible.role: Accessible.CheckBox
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 18
                Layout.preferredWidth: 14
                ToolTip.delay: 500
                ToolTip.text: qsTr("Fine adjust: half-beat grid, 1 ms cue shifts")
                ToolTip.visible: fineLatch.hovered
                enabled: root.loaded
                highlight: root.fineAdjust
                implicitHeight: 18
                implicitWidth: 14
                text: qsTr("F", "fine adjust key")

                onClicked: root.fineAdjust = !root.fineAdjust
            }
            Skin.Button {
                id: markerClearLatch

                Accessible.checkable: true
                Accessible.checked: root.markerClear
                Accessible.description: qsTr("While active, the four intro/outro marker keys clear the marker instead of setting it")
                Accessible.name: qsTr("Clear markers")
                Accessible.role: Accessible.CheckBox
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 18
                Layout.preferredWidth: 14
                ToolTip.delay: 500
                ToolTip.text: qsTr("Clear markers: marker keys clear instead of set")
                ToolTip.visible: markerClearLatch.hovered
                enabled: root.loaded
                highlight: root.markerClear
                implicitHeight: 18
                implicitWidth: 14
                text: qsTr("C", "clear marker key")

                onClicked: root.markerClear = !root.markerClear
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            spacing: Theme.space1

            Skin.ControlButton {
                id: alignButton

                Accessible.description: qsTr("Move the beatgrid so a beat lands on the current position")
                Accessible.name: qsTr("Align beatgrid to current position")
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                key: "beats_translate_curpos"
                text: qsTr("Align")
            }
            Skin.ControlButton {
                id: matchButton

                Accessible.description: qsTr("Align the grid to the nearest beat of another playing deck")
                Accessible.name: qsTr("Match beatgrid to other deck")
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                key: "beats_translate_match_alignment"
                text: qsTr("Match")
            }
            Skin.ControlButton {
                id: lockButton

                Accessible.description: qsTr("Prevent accidental BPM and grid changes")
                Accessible.name: qsTr("BPM lock")
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                key: "bpmlock"
                text: qsTr("Lock")
                toggleable: true
            }
            Skin.ControlButton {
                id: undoButton

                Accessible.description: qsTr("Revert the last BPM or beatgrid adjustment")
                Accessible.name: qsTr("Undo beatgrid adjustment")
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                activeColor: Theme.textOnAccent
                enabled: root.loaded && beatsUndoPossible.value > 0
                group: root.group
                implicitHeight: 24
                key: "beats_undo_adjustment"
                text: qsTr("Undo")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            spacing: Theme.space1

            MarkerKey {
                cue: "intro_start"
                text: qsTr("|◢")
            }
            MarkerKey {
                cue: "intro_end"
                text: qsTr("◢|")
            }
            MarkerKey {
                cue: "outro_start"
                text: qsTr("|◣")
            }
            MarkerKey {
                cue: "outro_end"
                text: qsTr("◣|")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 16
            spacing: Theme.space1

            GridKey {
                Accessible.description: qsTr("Moves the beatgrid left by a small amount. With FINE lit, shifts by exactly one half beat.")
                Accessible.name: qsTr("Adjust beats earlier")
                ToolTip.text: qsTr("Adjust Beats Earlier (FINE: half beat)")
                fineKey: "beats_translate_half"
                key: "beats_translate_earlier"
                text: qsTr("◀")
            }
            GridKey {
                Accessible.description: qsTr("Moves the beatgrid right by a small amount. With FINE lit, shifts by exactly one half beat.")
                Accessible.name: qsTr("Adjust beats later")
                ToolTip.text: qsTr("Adjust Beats Later (FINE: half beat)")
                fineKey: "beats_translate_half"
                key: "beats_translate_later"
                text: qsTr("▶")
            }
            GridKey {
                Accessible.description: qsTr("Adjusts the average BPM down by a small amount")
                Accessible.name: qsTr("Adjust BPM down")
                ToolTip.text: qsTr("Adjust BPM Down")
                key: "beats_adjust_slower"
                text: qsTr("B−")
            }
            GridKey {
                Accessible.description: qsTr("Adjusts the average BPM up by a small amount")
                Accessible.name: qsTr("Adjust BPM up")
                ToolTip.text: qsTr("Adjust BPM Up")
                key: "beats_adjust_faster"
                text: qsTr("B+")
            }
            GridKey {
                Accessible.description: qsTr("Shift cues imported from Serato or Rekordbox if they are slightly off time. Click: 10 milliseconds earlier. With FINE lit: 1 millisecond earlier.")
                Accessible.name: qsTr("Shift cues earlier")
                ToolTip.text: qsTr("Shift cues earlier (FINE: 1 ms)")
                fineKey: "shift_cues_earlier_small"
                key: "shift_cues_earlier"
                text: qsTr("C◀")
            }
            GridKey {
                Accessible.description: qsTr("Shift cues imported from Serato or Rekordbox if they are slightly off time. Click: 10 milliseconds later. With FINE lit: 1 millisecond later.")
                Accessible.name: qsTr("Shift cues later")
                ToolTip.text: qsTr("Shift cues later (FINE: 1 ms)")
                fineKey: "shift_cues_later_small"
                key: "shift_cues_later"
                text: qsTr("C▶")
            }
        }
    }

    /// One beatgrid nudge key. `fineKey` is its fine control (half-beat
    /// translate, 1 ms cue shift) while the FINE latch is active.
    component GridKey: Skin.Button {
        id: gridKey

        property string fineKey: ""
        required property string key

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        ToolTip.delay: 500
        ToolTip.visible: gridKey.hovered
        activeFocusOnTab: true
        enabled: root.loaded
        implicitHeight: 16

        onCanceled: controlBehavior.releasePrimary()
        onPressed: {
            if (root.fineAdjust && gridKey.fineKey.length > 0)
                fineControl.trigger();
            else
                controlBehavior.pressPrimary();
        }
        onReleased: {
            if (!(root.fineAdjust && gridKey.fineKey.length > 0))
                controlBehavior.releasePrimary();
        }

        Skin.ControlProxyButtonBehavior {
            id: controlBehavior

            group: root.group
            handlePointerInput: false
            key: gridKey.key
        }
        Mixxx.ControlProxy {
            id: fineControl

            group: root.group
            key: gridKey.fineKey.length > 0 ? gridKey.fineKey : gridKey.key
        }
    }

    /// One intro/outro marker key: click sets the marker at the current play
    /// position; with the CLR latch lit, click clears it. The face lights while
    /// the marker is set, so the pane reads the same state the waveform draws.
    component MarkerKey: Skin.Button {
        id: marker

        required property string cue
        readonly property bool set: markerEnabled.value > 0
        readonly property string title: {
            switch (marker.cue) {
            case "intro_start":
                return qsTr("Intro Start Marker");
            case "intro_end":
                return qsTr("Intro End Marker");
            case "outro_start":
                return qsTr("Outro Start Marker");
            default:
                return qsTr("Outro End Marker");
            }
        }

        Accessible.description: qsTr("Click to set the marker at the current play position. With CLR lit, click clears it.")
        Accessible.name: marker.title
        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        ToolTip.delay: 500
        ToolTip.text: root.markerClear ? qsTr("Clear %1").arg(marker.title) : marker.title
        ToolTip.visible: marker.hovered
        activeFocusOnTab: true
        enabled: root.loaded
        // A marker at 0:00 is valid, so the lamp follows the enabled control
        // instead of the position value.
        highlight: root.loaded && marker.set
        implicitHeight: 16
        normalColor: root.buttonColor

        // Keyboard activation (Space/Enter) arrives as clicked, not pressed.
        onClicked: {
            if (root.markerClear)
                markerClearControl.trigger();
            else
                markerSet.trigger();
        }

        Mixxx.ControlProxy {
            id: markerEnabled

            group: root.group
            key: `${marker.cue}_enabled`
        }
        Mixxx.ControlProxy {
            id: markerSet

            group: root.group
            key: `${marker.cue}_set`
        }
        Mixxx.ControlProxy {
            id: markerClearControl

            group: root.group
            key: `${marker.cue}_clear`
        }
    }
}
