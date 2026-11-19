import "." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import "Theme"

Item {
    id: root

    enum MouseStatus {
        Normal,
        Bending,
        Scratching
    }

    required property string group
    property bool splitStemTracks: false
    readonly property int activeWaveformType: Mixxx.Config.waveformType === Mixxx.WaveformDisplay.Type.Simple || Mixxx.Config.waveformType === Mixxx.WaveformDisplay.Type.Filtered || Mixxx.Config.waveformType === Mixxx.WaveformDisplay.Type.HSV || Mixxx.Config.waveformType === Mixxx.WaveformDisplay.Type.RGB || Mixxx.Config.waveformType === Mixxx.WaveformDisplay.Type.Stacked ? Mixxx.Config.waveformType : Mixxx.WaveformDisplay.Type.RGB
    // Renderer factories are created once by QmlWaveformDisplay. Keep track
    // of the type that was used for the scene-graph stack so a change from
    // the legacy preferences dialog can recreate that stack explicitly.
    property int renderedWaveformType: -1
    readonly property string zoomGroup: Mixxx.Config.waveformZoomSynchronization ? "[Channel1]" : group
    /// True while a CPU-heavy background job (stem separation or a batch
    /// analysis) is running.
    readonly property bool backgroundWorkActive: Mixxx.StemService.busy
            || Mixxx.StemService.queuedCount > 0
            || Mixxx.Library.analysisActive
    /// Waveform update rate: the selected monitor rate, or the screen's own
    /// rate for "Auto". Never faster than the panel, so every update lands on a
    /// fresh present and the play head advances by one even step per frame.
    readonly property int refreshRateLimit: Mixxx.Config.waveformRefreshRate > 0
            ? Mixxx.Config.waveformRefreshRate
            : Math.max(1, Mixxx.Config.displayRefreshRate)

    Connections {
        target: Mixxx.Config

        function onWaveformTypeChanged() {
            // Defer until activeWaveformType has been reevaluated, then
            // rebuild the renderer list on the scene-graph thread.
            Qt.callLater(function() {
                if (root.activeWaveformType === root.renderedWaveformType) {
                    return;
                }
                root.renderedWaveformType = root.activeWaveformType;
                waveformDisplay.refreshRenderers();
            });
        }

        function onWaveformDefaultZoomChanged() {
            if (zoomControl.group === root.group) {
                zoomControl.value = Mixxx.Config.waveformDefaultZoom;
            }
        }
    }

    MixxxControls.WaveformDisplay {
        id: waveformDisplay

        anchors.fill: parent
        backgroundColor: "transparent"
        // Background analysis and stem separation fight the render thread for
        // CPU. While they run, lower the refresh rate so frames arrive
        // regularly instead of hitching; the play head is extrapolated to the
        // vsync either way, so this reads as a steadier scroll, not a slower
        // one.
        frameRate: root.backgroundWorkActive
                   ? Math.min(30, root.refreshRateLimit)
                   : root.refreshRateLimit
        group: root.group
        options: Mixxx.Config.waveformOptions
        visible: Mixxx.Config.waveformEnabled
        zoom: zoomControl.value

        Behavior on zoom {
            SmoothedAnimation {
                duration: 500
                velocity: -1
            }
        }

        Mixxx.WaveformRendererEndOfTrack {
            color: '#ff8872'
            endOfTrackWarningTime: Mixxx.Config.waveformEndOfTrackWarningTime
        }
        Mixxx.WaveformRendererMarkRange {
            // Loop
            Mixxx.WaveformMarkRange {
                color: '#00b400'
                disabledColor: '#FFFFFF'
                disabledOpacity: 0.6
                enabledControl: "loop_enabled"
                endControl: "loop_end_position"
                opacity: 0.7
                startControl: "loop_start_position"
            }
            // Intro
            Mixxx.WaveformMarkRange {
                color: '#2c5c9a'
                durationTextColor: '#ffffff'
                durationTextLocation: 'after'
                endControl: "intro_end_position"
                opacity: 0.6
                startControl: "intro_start_position"
            }
            // Outro
            Mixxx.WaveformMarkRange {
                color: '#2c5c9a'
                durationTextColor: '#ffffff'
                durationTextLocation: 'before'
                endControl: "outro_end_position"
                opacity: 0.6
                startControl: "outro_start_position"
            }
        }
        Mixxx.WaveformRendererFiltered {
            axesColor: '#a1a1a1a1'
            enabled: root.activeWaveformType === Mixxx.WaveformDisplay.Type.Filtered
            gainAll: Mixxx.Config.waveformVisualGainAll
            gainHigh: Mixxx.Config.waveformVisualGainHigh
            gainLow: Mixxx.Config.waveformVisualGainLow
            gainMid: Mixxx.Config.waveformVisualGainMedium
            highColor: '#D5C2A2'
            lowColor: '#2154D7'
            midColor: '#97632D'
        }
        Mixxx.WaveformRendererFiltered {
            axesColor: '#a1a1a1a1'
            enabled: root.activeWaveformType === Mixxx.WaveformDisplay.Type.Stacked
            gainAll: Mixxx.Config.waveformVisualGainAll
            gainHigh: Mixxx.Config.waveformVisualGainHigh
            gainLow: Mixxx.Config.waveformVisualGainLow
            gainMid: Mixxx.Config.waveformVisualGainMedium
            // Stacked spectrum: low frequencies at the bottom in red, high
            // frequencies at the top in purple, magenta between them.
            highColor: '#8B6BE0'
            lowColor: '#E24B3F'
            midColor: '#C45B9E'
            stacked: true
        }
        Mixxx.WaveformRendererSimple {
            axesColor: '#a1a1a1a1'
            color: '#D5C2A2'
            enabled: root.activeWaveformType === Mixxx.WaveformDisplay.Type.Simple
            gain: Mixxx.Config.waveformVisualGainAll
        }
        Mixxx.WaveformRendererHSV {
            axesColor: '#a1a1a1a1'
            color: '#D5C2A2'
            enabled: root.activeWaveformType === Mixxx.WaveformDisplay.Type.HSV
            gainAll: Mixxx.Config.waveformVisualGainAll
            gainHigh: Mixxx.Config.waveformVisualGainHigh
            gainLow: Mixxx.Config.waveformVisualGainLow
            gainMid: Mixxx.Config.waveformVisualGainMedium
        }
        Mixxx.WaveformRendererRGB {
            axesColor: '#a1a1a1a1'
            enabled: root.activeWaveformType === Mixxx.WaveformDisplay.Type.RGB
            gainAll: Mixxx.Config.waveformVisualGainAll
            gainHigh: Mixxx.Config.waveformVisualGainHigh
            gainLow: Mixxx.Config.waveformVisualGainLow
            gainMid: Mixxx.Config.waveformVisualGainMedium
            highColor: '#D5C2A2'
            lowColor: '#2154D7'
            midColor: '#97632D'
        }
        Mixxx.WaveformRendererStem {
            // Per-stem visual gain, multiplied on top of the automatic lane
            // fill (which already keeps every lane between 60% and 100% of its
            // height, relative to the loudest stem of the track). The default
            // of 1.0 for all four keeps the automatic fill; raise a value to
            // push one stem lane louder, lower it to hold it back.
            stemVisualGains: [1.0, 1.0, 1.0, 1.0]
            gainAll: root.splitStemTracks ? 2.0 : 1.0
            opacity: Mixxx.Config.waveformStemOpacity
            outlineOpacity: Mixxx.Config.waveformStemOutlineOpacity
            reorderOnChange: Mixxx.Config.waveformStemReorderOnChange
            splitStemTracks: root.splitStemTracks || Mixxx.Config.waveformStemSplitTracks
        }
        Mixxx.WaveformRendererBeat {
            color: Qt.rgba(161 / 255, 161 / 255, 161 / 255, Mixxx.Config.waveformBeatGridAlpha / 100)
        }
        Mixxx.WaveformRendererMark {
            playMarkerBackground: '#D9D9D9'
            playMarkerColor: '#D9D9D9'
            playMarkerPosition: Mixxx.Config.waveformPlayMarkerPosition
            untilMark.align: Mixxx.Config.waveformUntilMarkAlign === 0 ? Qt.AlignTop : Mixxx.Config.waveformUntilMarkAlign === 2 ? Qt.AlignBottom : Qt.AlignVCenter
            untilMark.showBeats: Mixxx.Config.waveformUntilMarkShowBeats
            untilMark.showTime: Mixxx.Config.waveformUntilMarkShowTime
            untilMark.textHeightLimit: Mixxx.Config.waveformUntilMarkTextHeightLimit
            untilMark.textSize: Mixxx.Config.waveformUntilMarkTextPointSize

            defaultMark: Mixxx.WaveformMark {
                align: "bottom|right"
                color: "#00d9ff"
                endIcon: Qt.resolvedUrl("images/jump_%1.svg")
                text: " %1 "
                textColor: "#1a1a1a"
            }

            Mixxx.WaveformMark {
                align: 'top|right'
                color: 'red'
                control: "cue_point"
                text: 'CUE'
                textColor: '#1a1a1a'
            }
            Mixxx.WaveformMark {
                align: 'top|left'
                color: 'green'
                control: "loop_start_position"
                text: '↻'
                textColor: '#FFFFFF'
            }
            Mixxx.WaveformMark {
                align: 'bottom|right'
                color: 'green'
                control: "loop_end_position"
                textColor: '#FFFFFF'
            }
            Mixxx.WaveformMark {
                align: 'top|right'
                color: 'blue'
                control: "intro_start_position"
                text: '◢'
                textColor: '#FFFFFF'
            }
            Mixxx.WaveformMark {
                align: 'top|left'
                color: 'blue'
                control: "intro_end_position"
                text: '◢'
                textColor: '#FFFFFF'
            }
            Mixxx.WaveformMark {
                align: 'top|right'
                color: 'blue'
                control: "outro_start_position"
                text: '◣'
                textColor: '#FFFFFF'
            }
            Mixxx.WaveformMark {
                align: 'top|left'
                color: 'blue'
                control: "outro_end_position"
                text: '◣'
                textColor: '#FFFFFF'
            }
        }
    }
    Mixxx.ControlProxy {
        id: scratchPositionEnableControl

        group: root.group
        key: "scratch_position_enable"
    }
    Mixxx.ControlProxy {
        id: scratchPositionControl

        group: root.group
        key: "scratch_position"
    }
    Mixxx.ControlProxy {
        id: wheelControl

        group: root.group
        key: "wheel"
    }
    Mixxx.ControlProxy {
        id: rateRatioControl

        group: root.group
        key: "rate_ratio"
    }
    Mixxx.ControlProxy {
        id: zoomControl

        group: root.zoomGroup
        key: "waveform_zoom"

        Component.onCompleted: {
            if (group == root.group) {
                value = Mixxx.Config.waveformDefaultZoom
            }
        }
    }
    MouseArea {
        property point mouseAnchor: Qt.point(0, 0)
        property int mouseStatus: WaveformDisplay.MouseStatus.Normal

        acceptedButtons: Qt.LeftButton | Qt.RightButton
        anchors.fill: parent

        onDoubleClicked: {
            if (mouse.button == Qt.RightButton) {
                root.splitStemTracks = !root.splitStemTracks;
            }
        }
        onPositionChanged: {
            const diff = mouse.x - mouseAnchor.x;
            switch (mouseStatus) {
            case WaveformDisplay.MouseStatus.Bending:
                {
                    // Start at the middle of [0.0, 1.0], and emit values based on how far
                    // the mouse has traveled horizontally. Note, for legacy (MIDI) reasons,
                    // this is tuned to 127.
                    const v = 0.5 + (diff / root.width);
                    // clamp to [0.0, 1.0]
                    wheelControl.parameter = Math.max(Math.min(v, 1), 0);
                    break;
                }
                ;
            case WaveformDisplay.MouseStatus.Scratching:
                scratchPositionControl.value = -mouse.x * waveformDisplay.audioSamplePerPixel * 2;
                break;
            }
        }
        onPressed: {
            mouseAnchor = Qt.point(mouse.x, mouse.y);
            if (mouse.button == Qt.LeftButton) {
                if (mouseStatus == WaveformDisplay.MouseStatus.Bending)
                    wheelControl.parameter = 0.5;

                mouseStatus = WaveformDisplay.MouseStatus.Scratching;
                scratchPositionControl.value = -mouse.x * waveformDisplay.audioSamplePerPixel * 2;
                scratchPositionEnableControl.value = 1;
            } else {
                if (mouseStatus == WaveformDisplay.MouseStatus.Scratching)
                    scratchPositionEnableControl.value = 0;

                wheelControl.parameter = 0.5;
                mouseStatus = WaveformDisplay.MouseStatus.Bending;
            }
        }
        onReleased: {
            switch (mouseStatus) {
            case WaveformDisplay.MouseStatus.Bending:
                wheelControl.parameter = 0.5;
                break;
            case WaveformDisplay.MouseStatus.Scratching:
                scratchPositionEnableControl.value = 0;
                scratchPositionControl.value = 0;
                break;
            }
            mouseStatus = WaveformDisplay.MouseStatus.Normal;
        }
        onWheel: mouse => {
            if (mouse.angleDelta.y < 0 && zoomControl.value > 1) {
                zoomControl.value -= 1;
            } else if (mouse.angleDelta.y > 0 && zoomControl.value < 10.0) {
                zoomControl.value += 1;
            }
        }
    }

    // High-contrast playhead for the four-lane stem picture. The regular mark
    // renderer is drawn underneath the stem texture on some render paths, so
    // keep an explicit line above the waveform whenever stems are displayed.
    Rectangle {
        id: stemPlayhead

        anchors.verticalCenter: parent.verticalCenter
        color: Theme.textPrimary
        height: waveformDisplay.height
        opacity: 0.92
        visible: stemCountControl.value > 0 && Mixxx.Config.waveformEnabled
        width: Math.max(1, Math.round(1 * Theme.uiScale))
        x: Math.round(waveformDisplay.width * Mixxx.Config.waveformPlayMarkerPosition) - width / 2
        z: 2

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.accent
            height: Math.max(6, Math.round(7 * Theme.uiScale))
            radius: width / 2
            width: Math.max(4, Math.round(5 * Theme.uiScale))
            y: -height
        }
    }

    // Per-lane labels and gain knobs, Traktor-style. Only shown while the stem
    // lanes are split; in the single-lane overlay there is nothing to label.
    readonly property var deckPlayer: Mixxx.PlayerManager.getPlayer(root.group)
    readonly property bool stemSplitActive: root.splitStemTracks
            || Mixxx.Config.waveformStemSplitTracks
    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }
    Skin.StemLaneStrip {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        group: root.group
        stemsModel: root.deckPlayer ? root.deckPlayer.currentTrack.stemsModel : null
        visible: root.stemSplitActive && stemCountControl.value > 0
        width: 168
    }
}
