pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts 1.12
import "Theme"

/// One sampler slot: a flat card with the track colour as a thin accent,
/// play/cue, waveform, rate and the optional hotcue / FX assignments.
Rectangle {
    id: root

    property var currentTrack: deckPlayer.currentTrack
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    property int fxUnitCount: 2
    required property string group
    property int hotcueCount: 8
    readonly property bool loaded: trackLoadedControl.value > 0
    property bool minimized: false
    readonly property bool playing: playControl.value > 0
    property bool showFxAssignments: true
    property bool showHotcues: true
    property bool showRateControl: true
    readonly property var trackColor: root.currentTrack?.color
    readonly property real visualBpm: visualBpmControl.value

    signal fxAssignmentChanged(int unitNumber, bool enabled)

    Drag.active: dragArea.drag.active
    Drag.dragType: Drag.Automatic
    Drag.mimeData: {
        let data = {
            "mixxx/player": root.group
        };
        const trackLocationUrl = root.currentTrack?.trackLocationUrl;
        if (trackLocationUrl)
            data["text/uri-list"] = trackLocationUrl;
        return data;
    }
    Drag.supportedActions: Qt.CopyAction
    border.color: dragArea.containsMouse ? Theme.borderStrong : Theme.border
    border.width: 1
    color: Theme.surface1
    implicitHeight: root.minimized ? 50 : 190
    implicitWidth: 230
    radius: Theme.radiusMd

    Behavior on border.color {
        ColorAnimation {
            duration: Theme.motionFast
        }
    }

    // Track colour as a thin accent instead of tinting the whole card.
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Theme.radiusSm
        anchors.right: parent.right
        anchors.rightMargin: Theme.radiusSm
        anchors.top: parent.top
        anchors.topMargin: 1
        color: root.trackColor && root.trackColor.valid ? root.trackColor : "transparent"
        height: 2
        radius: 1
        visible: !!root.trackColor && root.trackColor.valid
    }
    MouseArea {
        id: dragArea

        anchors.fill: parent
        drag.target: root
        hoverEnabled: true
    }
    Item {
        id: summary

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 50

        Skin.EmbeddedBackground {
            id: embedded

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: Theme.space1
            anchors.right: vuMeter.left
            anchors.top: parent.top
        }
        Skin.ControlMiniKnob {
            id: gainKnob

            anchors.margins: Theme.space1
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.textPrimary
            group: root.group
            height: 36
            key: "pregain"
            width: 36
        }
        Skin.ControlButton {
            id: playButton

            activeColor: Theme.textOnAccent
            anchors.left: embedded.left
            anchors.top: embedded.top
            group: root.group
            height: 36
            highlight: playLatchedControl.value > 0 || root.playing
            key: "cue_gotoandplay"
            text: qsTr("Play")
            width: 36

            MouseArea {
                acceptedButtons: Qt.RightButton
                anchors.fill: parent

                onCanceled: cueDefaultControl.value = 0
                onPressed: cueDefaultControl.value = 1
                onReleased: cueDefaultControl.value = 0
            }
        }
        Text {
            id: label

            anchors.left: playButton.right
            anchors.leftMargin: Theme.space1
            anchors.right: bpmLabel.left
            anchors.rightMargin: Theme.space1
            anchors.top: embedded.top
            color: root.currentTrack ? Theme.textPrimary : Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            font.weight: Theme.fontWeightDemiBold
            text: root.currentTrack?.title || qsTr("Empty")
        }
        Text {
            id: bpmLabel

            anchors.right: embedded.right
            anchors.top: embedded.top
            color: Theme.textSecondary
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeXs
            text: root.loaded && root.visualBpm > 0 ? root.visualBpm.toFixed(1) : ""
        }
        Skin.WaveformOverview {
            anchors.bottom: embedded.bottom
            anchors.left: playButton.right
            anchors.leftMargin: Theme.space1
            anchors.right: embedded.right
            anchors.top: label.bottom
            anchors.topMargin: 2
            group: root.group
        }
        Skin.VuMeter {
            id: vuMeter

            anchors.bottom: parent.bottom
            anchors.margins: Theme.space1
            anchors.right: gainKnob.left
            anchors.top: parent.top
            group: root.group
            key: "vu_meter"
            width: 3
        }
    }
    ColumnLayout {
        id: expandedControls

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Theme.space1
        anchors.right: parent.right
        anchors.top: summary.bottom
        spacing: Theme.space1
        visible: !root.minimized

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space1

            Skin.ControlButton {
                Layout.fillWidth: true
                activeColor: Theme.textOnAccent
                group: root.group
                key: "sync_enabled"
                text: qsTr("Sync")
                toggleable: true
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                activeColor: Theme.textOnAccent
                group: root.group
                key: "keylock"
                text: qsTr("Key")
                toggleable: true
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                activeColor: Theme.textOnAccent
                group: root.group
                key: "repeat"
                text: qsTr("Loop")
                toggleable: true
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                activeColor: Theme.textOnAccent
                group: root.group
                key: "pfl"
                text: qsTr("PFL")
                toggleable: true
            }
            Skin.OrientationToggleButton {
                Layout.fillWidth: true
                color: Theme.textMuted
                group: root.group
                key: "orientation"
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                group: root.group
                key: "eject"
                text: qsTr("Eject")
            }
        }
        Skin.ControlFader {
            Layout.fillWidth: true
            bar.color: Theme.accent
            bar.start: 0.5
            bg: Theme.imgBpmSliderBackground
            group: root.group
            key: "rate"
            orientation: Qt.Horizontal
            visible: root.showRateControl

            handleImage {
                height: 6
                width: 20
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columnSpacing: Theme.space1
            columns: 4
            rowSpacing: Theme.space1
            visible: root.showHotcues && root.hotcueCount > 0

            Repeater {
                model: Math.min(8, Math.max(0, root.hotcueCount))

                Skin.HotcueButton {
                    required property int index

                    Layout.fillWidth: true
                    group: root.group
                    hotcueNumber: index + 1
                    implicitHeight: 22
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space1
            visible: root.showFxAssignments && root.fxUnitCount > 0

            Repeater {
                model: Math.max(0, root.fxUnitCount)

                Skin.ControlButton {
                    id: fxButton

                    required property int index

                    Layout.fillWidth: true
                    activeColor: Theme.textOnAccent
                    group: "[EffectRack1_EffectUnit" + (index + 1) + "]"
                    implicitHeight: 22
                    key: "group_" + root.group + "_enable"
                    text: qsTr("FX%1").arg(index + 1)
                    toggleable: true

                    onHighlightChanged: root.fxAssignmentChanged(index + 1, highlight)
                }
            }
        }
    }
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
        id: playControl

        group: root.group
        key: "play"
    }
    Mixxx.ControlProxy {
        id: playLatchedControl

        group: root.group
        key: "play_latched"
    }
    Mixxx.ControlProxy {
        id: cueDefaultControl

        group: root.group
        key: "cue_default"
    }
    Mixxx.ControlProxy {
        id: ejectControl

        group: root.group
        key: "eject"
    }
    TapHandler {
        onDoubleTapped: {
            if (root.playing)
                playControl.value = 0;
            else
                ejectControl.trigger();
        }
    }
    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: root.group
    }
}
