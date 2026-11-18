import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import QtQuick.Shapes
import "../Theme"

/// Deck hotcue grid and the four-lane stem mixer.
///
/// Both live in one component so the pane host stays at a constant height:
/// `paneMode` 1 shows hotcues only (the Cue pane) and 2 shows stems only
/// (the Stems pane).
///
/// Solo is implemented in QML only: activating it mutes every other stem and
/// the previous mute states are restored when it is cleared. A native engine
/// solo control (e.g. `[ChannelN_StemX], solo`) is a later change; once it
/// exists the `soloLane` / `savedMuteStates` bookkeeping below can go away.
Rectangle {
    id: root

    required property var currentTrack

    /// True when the deck pane is shorter than its design height, or when
    /// the stem lanes wrap onto more than one row and each lane keeps only part
    /// of the pane height. Either way the QuickFX preset selector is dropped so
    /// the fader, mute/solo and knob stay usable instead of being clipped.
    readonly property bool dense: root.height < 112 || root.stemColumns < root.laneCount
    /// Wording for the empty pane, per tab: each pane now says what it is for
    /// instead of advertising both hotcues and stems.
    readonly property string emptyText: {
        if (root.paneMode === 1) {
            return Mixxx.Config.paramixShowHotcues ? qsTr("Load a track to use hotcues") : qsTr("Hotcues are hidden in the interface settings");
        }
        if (root.paneMode === 2) {
            if (!Mixxx.Config.paramixShowStems) {
                return qsTr("Stem controls are hidden in the interface settings");
            }
            if (trackLoadedControl.value == 0) {
                return qsTr("Load a track to separate its stems");
            }
            return qsTr("Separate this track's stems to use this pane");
        }
        if (!root.showHotcues && !root.showStems) {
            return qsTr("Cue pads are hidden in the interface settings");
        }
        return qsTr("Load a track to use hotcues and stems");
    }
    required property string group
    readonly property int laneCount: stemLaneRepeater.count

    /// 1 = hotcues only, 2 = stems only.
    property int paneMode: 1
    /// Mute state of every lane captured when solo was activated.
    property var savedMuteStates: []
    readonly property bool showHotcues: Mixxx.Config.paramixShowHotcues && root.paneMode !== 2
    readonly property bool showStems: Mixxx.Config.paramixShowStems && stemCount > 0 && root.paneMode !== 1

    /// Index of the lane that is currently soloed, or -1 when solo is off.
    property int soloLane: -1

    /// How many stem lanes fit in one row. Sized by the comfortable label width
    /// rather than the hard 58 px minimum, otherwise four lanes stay side by side
    /// at ~68 px each and every label degrades to "Dru...". Wrapping to two
    /// columns is always allowed: one column would stack four lanes into the
    /// 112 px pane and every lane would be a sliver.
    readonly property int stemColumns: {
        const perRow = Math.floor(stemTab.width / (Theme.stemLaneComfortWidth + Theme.space1));
        if (perRow >= root.laneCount)
            return root.laneCount;
        return Math.min(root.laneCount, 2);
    }
    readonly property int stemCount: Math.max(0, Math.round(stemCountControl.value))

    function applyAcapella() {
        root.setComplementMute(false);
    }
    function applyInstrumental() {
        root.setComplementMute(true);
    }
    function clearSolo() {
        if (root.soloLane < 0)
            return;
        for (let i = 0; i < root.laneCount; ++i) {
            const lane = stemLaneRepeater.itemAt(i);
            if (lane)
                lane.setMuted(root.savedMuteStates[i] === true);
        }
        root.resetSoloState();
    }
    /// Choose dark or light text for a hotcue color so every pad stays legible.
    function hotcueTextColorFor(color) {
        if (!color || color.a <= 0)
            return Theme.textMuted;
        const luminance = 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
        return luminance > 0.6 ? "#08101C" : "#FFFFFF";
    }
    function labelIsVocal(label) {
        const value = String(label ?? "").toLowerCase();
        return value.indexOf("vocal") >= 0 || value.indexOf("acapella") >= 0 || value.indexOf("voice") >= 0;
    }
    /// `[ChannelN_StemX]` -> `[QuickEffectRack1_[ChannelN_StemX]]`
    function quickEffectGroupFor(index) {
        return `[QuickEffectRack1_${root.stemGroupFor(index)}]`;
    }
    function resetSoloState() {
        root.soloLane = -1;
        root.savedMuteStates = [];
    }
    /// Acapella keeps the vocals and mutes everything else; instrumental does
    /// the opposite. Both leave solo first so the user's mute edit sticks.
    function setComplementMute(muteVocals) {
        root.clearSolo();
        for (let i = 0; i < root.laneCount; ++i) {
            const lane = stemLaneRepeater.itemAt(i);
            if (lane)
                lane.setMuted(muteVocals ? lane.kind === "vocals" : lane.kind !== "vocals");
        }
    }
    function stemColorFor(kind, modelColor) {
        if (modelColor !== undefined && modelColor.a > 0)
            return modelColor;
        switch (kind) {
        case "drums":
            return Theme.stemDrums;
        case "bass":
            return Theme.stemBass;
        case "vocals":
            return Theme.stemVocals;
        default:
            return Theme.stemOther;
        }
    }

    /// `[ChannelN]` -> `[ChannelN_StemX]`
    function stemGroupFor(index) {
        return `${root.group.substr(0, root.group.length - 1)}_Stem${index + 1}]`;
    }
    /// Classify a stem from its model label. Demucs/HTDemucs emit
    /// drums, bass, other, vocals in that order, so a four-stem set with
    /// unhelpful labels falls back to that order (last lane = vocals).
    function stemKindFor(label, index, count) {
        const value = String(label ?? "").toLowerCase();
        if (root.labelIsVocal(label))
            return "vocals";
        if (value.indexOf("drum") >= 0)
            return "drums";
        if (value.indexOf("bass") >= 0)
            return "bass";
        if (value.indexOf("other") >= 0)
            return "other";
        if (count === 4)
            return ["drums", "bass", "other", "vocals"][index];
        return "other";
    }
    function toggleSolo(index) {
        if (root.soloLane === index) {
            root.clearSolo();
            return;
        }
        if (root.soloLane < 0) {
            const saved = [];
            for (let i = 0; i < root.laneCount; ++i) {
                const lane = stemLaneRepeater.itemAt(i);
                saved.push(lane ? lane.muted : false);
            }
            root.savedMuteStates = saved;
        }
        root.soloLane = index;
        for (let i = 0; i < root.laneCount; ++i) {
            const lane = stemLaneRepeater.itemAt(i);
            if (lane)
                lane.setMuted(i !== index);
        }
    }

    border.color: Theme.border
    border.width: 1
    // Nothing in a pane may paint over the loop card beside it.
    clip: true
    color: Theme.surface1
    radius: Theme.radiusMd

    onCurrentTrackChanged: root.resetSoloState()

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"

        onValueChanged: root.resetSoloState()
    }
    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }

    // Empty state: without a track the tab bodies are hidden, so name what
    // the panel is for instead of leaving a blank plate. The host fills the
    // whole band, so the message stays centred whatever the stem/quickFX
    // column does.
    Item {
        id: emptyState

        anchors.fill: parent
        visible: trackLoadedControl.value == 0 || (!root.showHotcues && !root.showStems)

        Text {
            anchors.centerIn: parent
            color: Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: root.emptyText
            width: Math.max(80, Math.min(parent.width - 2 * Theme.space4, 260))
            wrapMode: Text.WordWrap
        }
    }
    Item {
        id: hotcueTab

        anchors.fill: parent
        visible: trackLoadedControl.value != 0 && root.showHotcues

        Skin.FadeBehavior on visible {
            fadeTarget: hotcueTab
        }

        GridLayout {
            anchors.bottomMargin: Theme.space2
            anchors.fill: parent
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            anchors.topMargin: Theme.space2
            columnSpacing: Theme.space1
            rowSpacing: Theme.space1

            Repeater {
                model: 8

                // HotcueButton owns the press-to-activate and the right-click
                // colour/clear popup; the face is redrawn here so a stored cue
                // colour shows through and the pad keeps its number plus label.
                Skin.HotcueButton {
                    id: hotcueButton

                    required property int index
                    /// hotcuesModel holds only the set cues, and its
                    /// hotcueNumber is 0-based, so match on the number rather
                    /// than the row index.
                    readonly property string label: {
                        const model = root.currentTrack ? root.currentTrack.hotcuesModel : null;
                        if (!padCue.isSet || !model)
                            return "";
                        for (let row = 0; row < model.rowCount(); ++row) {
                            const cue = model.get(row);
                            if (cue && cue.hotcueNumber === hotcueButton.hotcueNumber - 1)
                                return cue.label ?? "";
                        }
                        return "";
                    }
                    /// Read-only view of the cue for the pad face; HotcueButton
                    /// keeps its own copy for the popup.
                    readonly property color textColor: padCue.isSet ? root.hotcueTextColorFor(padCue.color) : Theme.textMuted

                    Accessible.description: qsTr("Left click jumps to the hotcue, right click opens the color and clear menu")
                    Accessible.name: padCue.isSet ? qsTr("Hotcue %1").arg(index + 1) : qsTr("Hotcue %1, empty").arg(index + 1)
                    Layout.column: index % 4
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.row: parseInt(index / 4)
                    group: root.group
                    hotcueNumber: index + 1

                    background: Rectangle {
                        border.color: padCue.isSet ? Qt.lighter(padCue.color, 1.2) : Theme.border
                        border.width: 1
                        color: padCue.isSet ? padCue.color : Theme.surface1
                        radius: Theme.radiusSm

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.motionFast
                            }
                        }

                        // Lit top edge for the pad face.
                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 1
                            anchors.right: parent.right
                            anchors.rightMargin: 1
                            anchors.top: parent.top
                            anchors.topMargin: 1
                            color: padCue.isSet ? Qt.rgba(255, 255, 255, 0.28) : Theme.topHighlight
                            height: 1
                        }
                    }
                    contentItem: Item {
                        Column {
                            anchors.centerIn: parent
                            spacing: 0
                            width: parent.width

                            Text {
                                color: hotcueButton.textColor
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLg
                                font.weight: Theme.fontWeightDemiBold
                                horizontalAlignment: Text.AlignHCenter
                                text: `${hotcueButton.hotcueNumber}`
                                width: parent.width
                            }
                            Text {
                                color: hotcueButton.textColor
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                horizontalAlignment: Text.AlignHCenter
                                text: hotcueButton.label
                                visible: hotcueButton.label.length > 0
                                width: parent.width
                            }
                        }
                    }

                    Skin.Hotcue {
                        id: padCue

                        group: root.group
                        hotcueNumber: hotcueButton.hotcueNumber
                        useStoredColor: true
                    }
                }
            }
        }
    }
    Item {
        id: stemTab

        anchors.fill: parent
        visible: trackLoadedControl.value != 0 && root.showStems

        Skin.FadeBehavior on visible {
            fadeTarget: stemTab
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space2
            spacing: Theme.space1

            RowLayout {
                id: stemHeader

                /// With the jog wheel shown the pane is ~196 px, and the badge
                /// plus two full-width keys (~260 px) ran under the loop card.
                readonly property bool narrow: stemTab.width < 280

                Layout.fillWidth: true
                Layout.preferredHeight: 18
                spacing: Theme.space1

                // Separation status badge. The "STEMS · N" count comes from
                // `stem_count`; there is no separation-progress ControlObject
                // yet, so this badge will bind to it once Stems 4 (the
                // background separation queue) lands.
                Rectangle {
                    Layout.preferredHeight: 18
                    Layout.preferredWidth: badgeContent.implicitWidth + Theme.space3
                    border.color: Theme.border
                    border.width: 1
                    color: Theme.surface1
                    radius: Theme.radiusSm

                    RowLayout {
                        id: badgeContent

                        anchors.centerIn: parent
                        spacing: Theme.space1

                        Rectangle {
                            Layout.preferredHeight: 6
                            Layout.preferredWidth: 6
                            color: root.stemCount > 0 ? Theme.positive : Theme.textMuted
                            radius: width / 2
                        }
                        Text {
                            color: root.stemCount > 0 ? Theme.textSecondary : Theme.textMuted
                            font.bold: true
                            font.family: Theme.fontFamily
                            font.letterSpacing: Theme.letterSpacingCaps
                            font.pixelSize: Theme.fontSizeXs
                            text: root.stemCount > 0 ? qsTr("STEMS · %1").arg(root.stemCount) : qsTr("NO STEMS")
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Skin.Button {
                    Accessible.name: qsTr("Acapella: mute all non-vocal stems")
                    Layout.preferredHeight: 16
                    implicitWidth: stemHeader.narrow ? 40 : 74
                    text: stemHeader.narrow ? qsTr("Aca") : qsTr("Acapella")

                    onClicked: root.applyAcapella()
                }
                Skin.Button {
                    Accessible.name: qsTr("Instrumental: mute the vocal stem")
                    Layout.preferredHeight: 16
                    implicitWidth: stemHeader.narrow ? 40 : 94
                    text: stemHeader.narrow ? qsTr("Inst") : qsTr("Instrumental")

                    onClicked: root.applyInstrumental()
                }
            }
            // The pane can shrink to 200 px, which is narrower than four lanes
            // at the comfortable label width. Wrap into two columns instead of
            // letting the row spill out of the deck column.
            GridLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                columnSpacing: Theme.space1
                columns: root.stemColumns
                rowSpacing: Theme.space1

                Repeater {
                    id: stemLaneRepeater

                    model: root.currentTrack ? root.currentTrack.stemsModel : null

                    delegate: StemLane {
                    }
                }
            }
        }
    }

    /// One stem lane: colour chip + label + status dot, mute/solo, vertical
    /// volume fader, QuickFX knob with enable toggle and chain preset.
    component StemLane: Item {
        id: lane

        required property color color
        /// True when the lane is too short for the QuickFX row (a wrapped
        /// 2 x 2 pane gives each lane about 39 px).
        readonly property bool compact: lane.height < 64
        readonly property string displayLabel: label && label.length > 0 ? label : qsTr("Stem %1").arg(index + 1)
        readonly property bool fxEnabled: fxEnabledControl.value != 0
        readonly property string fxGroup: root.quickEffectGroupFor(index)
        required property int index
        readonly property string kind: root.stemKindFor(label, index, root.laneCount)
        required property string label
        readonly property color laneColor: root.stemColorFor(kind, color)
        readonly property bool muted: stemMute.value != 0
        readonly property bool soloed: root.soloLane === index
        readonly property string stemGroup: root.stemGroupFor(index)

        function setMuted(value) {
            stemMute.value = value ? 1 : 0;
        }
        function toggleMute() {
            // A manual mute click wins over solo: drop solo first so the saved
            // mute states are restored before applying the user's edit.
            if (root.soloLane >= 0)
                root.clearSolo();
            setMuted(!muted);
        }

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumWidth: Theme.stemLaneMinWidth

        Rectangle {
            id: card

            anchors.fill: parent
            border.color: lane.soloed ? Theme.accent : (lane.muted ? Theme.border : Theme.borderStrong)
            border.width: 1
            color: lane.soloed ? Qt.alpha(Theme.accent, 0.14) : (lane.muted ? Theme.surface1 : Theme.surface2)
            radius: Theme.radiusMd

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
        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.space1
            spacing: Theme.space1

            // Vertical volume fader. The stem volume control is a linear
            // 0..1 gain, so 100 % (unity) is the top of the range; `maximum`
            // keeps the fader from ever asking the engine for more than that.
            // 16 px wide with a 5 px handle keeps the whole fader inside the
            // 54 px lane card at the compact pane height.
            Skin.ControlFader {
                id: volumeFader

                Accessible.name: qsTr("%1 volume").arg(lane.displayLabel)
                Layout.fillHeight: true
                Layout.preferredWidth: 16
                bar.color: lane.laneColor
                bar.enabled: true
                bar.margin: 2
                bar.start: 0
                bar.width: 3
                from: 0
                group: lane.stemGroup
                implicitWidth: 16
                key: "volume"
                maximum: 1.0
                showDefaultHandle: false
                to: 1

                background: Rectangle {
                    anchors.fill: parent
                    color: "transparent"

                    Rectangle {
                        anchors.centerIn: parent
                        color: Theme.surface3
                        height: parent.height
                        radius: 1.5
                        width: 3
                    }
                }
                handle: Rectangle {
                    border.color: Theme.borderStrong
                    border.width: 1
                    color: lane.muted ? Theme.textMuted : Theme.textPrimary
                    height: 5
                    radius: 2
                    width: 14
                    x: Math.round((volumeFader.width - width) / 2)
                    // Keep the handle inside the coloured part of the groove:
                    // at 100 % its top lines up with the top of the bar instead
                    // of overshooting it, so the fader visibly stops at unity.
                    y: Math.round(volumeFader.bar.margin + volumeFader.visualPosition * (volumeFader.height - height - 2 * volumeFader.bar.margin))

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.motionFast
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: 1

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 10
                    spacing: Theme.space1

                    Rectangle {
                        Layout.preferredHeight: 8
                        Layout.preferredWidth: 8
                        color: lane.laneColor
                        radius: 2
                    }
                    Text {
                        Layout.fillWidth: true
                        color: lane.muted ? Theme.textMuted : Theme.textPrimary
                        elide: Text.ElideRight
                        font.capitalization: Font.MixedCase
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Theme.fontWeightMedium
                        text: lane.displayLabel
                        verticalAlignment: Text.AlignVCenter
                    }
                    Rectangle {
                        Layout.preferredHeight: 6
                        Layout.preferredWidth: 6
                        border.color: lane.muted ? Theme.textMuted : lane.laneColor
                        border.width: 1
                        color: lane.muted ? "transparent" : lane.laneColor
                        radius: width / 2
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 14
                    spacing: Theme.space1

                    StemToggleButton {
                        id: muteButton

                        Accessible.name: qsTr("Mute %1").arg(lane.displayLabel)
                        Layout.fillWidth: true
                        active: lane.muted
                        activeBackground: Theme.danger
                        text: qsTr("M")

                        onClicked: lane.toggleMute()
                    }
                    StemToggleButton {
                        id: soloButton

                        Accessible.name: qsTr("Solo %1").arg(lane.displayLabel)
                        Layout.fillWidth: true
                        active: lane.soloed
                        activeBackground: Theme.accent
                        text: qsTr("S")

                        onClicked: root.toggleSolo(lane.index)
                    }
                }
                RowLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: 3
                    // A wrapped 2 x 2 pane leaves a lane ~39 px. The QuickFX
                    // row needs 20 more, so drop it and keep label, M/S and
                    // fader usable; the mixer has the same QuickFX control.
                    visible: !lane.compact

                    Skin.ControlMiniKnob {
                        id: fxKnob

                        Accessible.name: qsTr("%1 QuickFX").arg(lane.displayLabel)
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredHeight: 20
                        Layout.preferredWidth: 20
                        color: lane.fxEnabled ? Theme.accent : Theme.textMuted
                        group: lane.fxGroup
                        key: "super1"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Item {
                        id: fxEnableButton

                        Accessible.checkable: true
                        Accessible.checked: lane.fxEnabled
                        Accessible.name: qsTr("%1 QuickFX enabled").arg(lane.displayLabel)
                        Accessible.role: Accessible.CheckBox
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredHeight: 12
                        Layout.preferredWidth: 12

                        Rectangle {
                            anchors.fill: parent
                            border.color: lane.fxEnabled ? Theme.accent : Theme.borderStrong
                            border.width: 1
                            color: lane.fxEnabled ? Theme.accent : "transparent"
                            radius: width / 2

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.motionFast
                                }
                            }
                        }
                        TapHandler {
                            onTapped: fxEnabledControl.value = lane.fxEnabled ? 0 : 1
                        }
                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                }
                Skin.ComboBox {
                    id: fxPreset

                    Accessible.name: qsTr("%1 QuickFX preset").arg(lane.displayLabel)
                    Layout.fillWidth: true
                    Layout.preferredHeight: 14
                    clip: true
                    currentIndex: fxSelect.value == -1 ? 0 : fxSelect.value
                    font.pixelSize: Theme.fontSizeXs
                    indicator.width: 0
                    model: Mixxx.EffectsManager.quickChainPresetModel
                    popupMaxItem: 8
                    popupWidth: 120
                    textRole: "display"
                    visible: !root.dense && !lane.compact

                    onActivated: index => {
                        fxSelect.value = index;
                    }

                    Mixxx.ControlProxy {
                        id: fxSelect

                        group: lane.fxGroup
                        key: "loaded_chain_preset"
                    }
                }
            }
        }
        Mixxx.ControlProxy {
            id: stemMute

            group: lane.stemGroup
            key: "mute"
        }
        Mixxx.ControlProxy {
            id: fxEnabledControl

            group: lane.fxGroup
            key: "enabled"
        }
    }
    /// Compact M/S style toggle used by the stem lanes.
    component StemToggleButton: Skin.Button {
        id: control

        property bool active: false
        property color activeBackground: Theme.accent
        property color activeTextColor: Theme.textOnAccent

        checkable: false
        implicitHeight: 14
        implicitWidth: 22

        background: Rectangle {
            border.color: control.active ? control.activeBackground : (control.hovered ? Theme.borderStrong : Theme.border)
            border.width: 1
            color: control.active ? control.activeBackground : (control.hovered ? Theme.surface3 : Theme.surface1)
            radius: Theme.radiusXs

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
        contentItem: Text {
            color: control.active ? control.activeTextColor : Theme.textSecondary
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            horizontalAlignment: Text.AlignHCenter
            text: control.text
            verticalAlignment: Text.AlignVCenter
        }
    }
}
