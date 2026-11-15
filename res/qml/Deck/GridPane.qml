import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "../Theme"

/// Beatgrid pane: BPM readout with tap, grid alignment and key readout.
///
/// This is the GRID tab of the deck pane stack. It replaces the toolbar's
/// Beatgrid key (which is gone) with explicit Align / Match / Lock keys plus
/// the BPM halve and double actions, so nothing about the grid is hidden
/// behind a right-click any more.
Rectangle {
    id: root

    readonly property bool loaded: trackLoadedControl.value > 0
    readonly property real visualBpm: visualBpmControl.value
    /// True when the pane is squeezed by the compact deck layout. The BPM
    /// readout drops a size so the header + BPM line + action line still fit
    /// the 96 px compact strip.
    readonly property bool dense: root.height < 104
    required property string group

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
        id: keyControl

        group: root.group
        key: "key"
    }
    Mixxx.ControlProxy {
        id: beatsUndoPossible

        group: root.group
        key: "beats_undo_possible"
    }
    Mixxx.ControlProxy {
        id: beatsUndo

        group: root.group
        key: "beats_undo_adjustment"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space1

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 14
            spacing: Theme.space1

            Text {
                Layout.fillWidth: true
                color: Theme.textMuted
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                text: qsTr("Beatgrid")
                verticalAlignment: Text.AlignVCenter
            }
            Skin.Button {
                id: undoButton

                Layout.preferredHeight: 14
                Layout.preferredWidth: 44
                enabled: root.loaded && beatsUndoPossible.value > 0
                implicitHeight: 14
                implicitWidth: 44
                normalColor: Theme.textMuted
                text: qsTr("Undo")

                Accessible.description: qsTr("Revert the last beatgrid adjustment")
                Accessible.name: qsTr("Undo beatgrid adjustment")

                onClicked: beatsUndo.trigger()
            }
        }
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 28
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
            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                Layout.minimumWidth: 0
                Layout.preferredHeight: 24
                Layout.preferredWidth: 48
                border.color: Theme.border
                border.width: 1
                color: Theme.surface2
                radius: Theme.radiusSm

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 0

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.textMuted
                        font.capitalization: Font.AllUppercase
                        font.family: Theme.fontFamily
                        font.letterSpacing: Theme.letterSpacingCaps
                        font.pixelSize: Theme.fontSizeXs
                        text: qsTr("Key")
                    }
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.loaded && Mixxx.KeyUtils.keyIsValid(keyControl.value)
                               ? Theme.textPrimary
                               : Theme.textMuted
                        font.family: Theme.fontFamilyMono
                        font.pixelSize: Theme.fontSizeMd
                        text: root.loaded && Mixxx.KeyUtils.keyIsValid(keyControl.value)
                              ? Mixxx.KeyUtils.keyToString(keyControl.value)
                              : "--"
                    }
                }
            }
            Skin.ControlButton {
                id: bpmTapButton

                Layout.alignment: Qt.AlignVCenter
                Layout.minimumWidth: 0
                Layout.preferredHeight: 24
                Layout.preferredWidth: 42
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                implicitWidth: 42
                key: "bpm_tap"
                text: qsTr("Tap")

                Accessible.description: qsTr("Tap along with the beat to set the BPM")
                Accessible.name: qsTr("Tap tempo")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            spacing: Theme.space1

            Skin.ControlButton {
                id: alignButton

                Layout.fillHeight: true
                Layout.fillWidth: true
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                key: "beats_translate_curpos"
                text: qsTr("Align")

                Accessible.description: qsTr("Move the beatgrid so a beat lands on the current position")
                Accessible.name: qsTr("Align beatgrid to current position")
            }
            Skin.ControlButton {
                id: matchButton

                Layout.fillHeight: true
                Layout.fillWidth: true
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                key: "beats_translate_match_alignment"
                text: qsTr("Match")

                Accessible.description: qsTr("Align the grid to the nearest beat of another playing deck")
                Accessible.name: qsTr("Match beatgrid to other deck")
            }
            Skin.ControlButton {
                id: lockButton

                Layout.fillHeight: true
                Layout.fillWidth: true
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                key: "bpmlock"
                text: qsTr("Lock")
                toggleable: true

                Accessible.description: qsTr("Prevent accidental BPM and grid changes")
                Accessible.name: qsTr("BPM lock")
            }
            Skin.ControlButton {
                id: bpmHalveButton

                Layout.fillHeight: true
                Layout.preferredWidth: 30
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                implicitWidth: 30
                key: "beats_set_halve"
                text: qsTr("½")

                Accessible.description: qsTr("Halve the track BPM, e.g. 174 to 87")
                Accessible.name: qsTr("Halve BPM")
            }
            Skin.ControlButton {
                id: bpmDoubleButton

                Layout.fillHeight: true
                Layout.preferredWidth: 30
                activeColor: Theme.textOnAccent
                group: root.group
                implicitHeight: 24
                implicitWidth: 30
                key: "beats_set_double"
                text: qsTr("×2")

                Accessible.description: qsTr("Double the track BPM, e.g. 87 to 174")
                Accessible.name: qsTr("Double BPM")
            }
        }
    }
}
