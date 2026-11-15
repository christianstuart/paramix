import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import QtQuick.Shapes
import "../Theme"

/// Deck hotcue grid plus a four-lane stem mixer strip.
///
/// Solo is implemented in QML only: activating it mutes every other stem and
/// the previous mute states are restored when it is cleared. A native engine
/// solo control (e.g. `[ChannelN_StemX], solo`) is a later change; once it
/// exists the `soloLane` / `savedMuteStates` bookkeeping below can go away.
Rectangle {
    id: root

    required property var currentTrack
    required property string group

    /// Index of the lane that is currently soloed, or -1 when solo is off.
    property int soloLane: -1
    /// Mute state of every lane captured when solo was activated.
    property var savedMuteStates: []

    readonly property int stemCount: Math.max(0, Math.round(stemCountControl.value))
    readonly property int laneCount: stemLaneRepeater.count
    readonly property bool showHotcues: Mixxx.Config.paramixShowHotcues
    readonly property bool showStems: Mixxx.Config.paramixShowStems && stemCount > 0
    /// True when the deck pane is squeezed by the compact deck layout. The
    /// stem lanes drop the QuickFX preset selector so the fader, mute/solo and
    /// knob stay usable instead of being clipped.
    readonly property bool dense: root.height < 112

    /// `[ChannelN]` -> `[ChannelN_StemX]`
    function stemGroupFor(index) {
        return `${root.group.substr(0, root.group.length - 1)}_Stem${index + 1}]`;
    }
    /// `[ChannelN_StemX]` -> `[QuickEffectRack1_[ChannelN_StemX]]`
    function quickEffectGroupFor(index) {
        return `[QuickEffectRack1_${root.stemGroupFor(index)}]`;
    }
    function labelIsVocal(label) {
        const value = String(label ?? "").toLowerCase();
        return value.indexOf("vocal") >= 0
                || value.indexOf("acapella") >= 0
                || value.indexOf("voice") >= 0;
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
    /// Choose dark or light text for a hotcue color so every pad stays legible.
    function hotcueTextColorFor(color) {
        if (!color || color.a <= 0)
            return Theme.textMuted;
        const luminance = 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
        return luminance > 0.6 ? "#08101C" : "#FFFFFF";
    }
    function resetSoloState() {
        root.soloLane = -1;
        root.savedMuteStates = [];
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
    function applyAcapella() {
        root.setComplementMute(false);
    }
    function applyInstrumental() {
        root.setComplementMute(true);
    }

    color: Theme.surface1
    radius: Theme.radiusMd
    border.width: 1
    border.color: Theme.border

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
    // column does and cannot drift under the pane tabs.
    Item {
        id: emptyState

        anchors.fill: parent
        visible: trackLoadedControl.value == 0
                 || (!root.showHotcues && !root.showStems)

        Text {
            anchors.centerIn: parent
            color: Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: (!root.showHotcues && !root.showStems)
                  ? qsTr("Cue pads are hidden in the interface settings")
                  : qsTr("Load a track to use hotcues and stems")
            width: Math.max(80, Math.min(parent.width - 2 * Theme.space4, 260))
            wrapMode: Text.WordWrap
        }
    }

    states: [
        State {
            name: "inactive"
            when: trackLoadedControl.value == 0

            PropertyChanges {
                checked: false
                opacity: 0.75
                target: hotcueTabButton
            }
            PropertyChanges {
                checked: false
                opacity: 0.75
                target: stemTabButton
            }
            PropertyChanges {
                target: stemTab
                visible: false
            }
            PropertyChanges {
                target: hotcueTab
                visible: false
            }
        },
        State {
            name: "hotcue"
            when: trackLoadedControl.value != 0 && root.showHotcues
                  && (hotcueTabButton.checked || !root.showStems)

            PropertyChanges {
                target: hotcueTab
                visible: true
            }
            PropertyChanges {
                target: stemTab
                visible: false
            }
        },
        State {
            name: "stem"
            when: trackLoadedControl.value != 0 && root.showStems
                  && (stemTabButton.checked || !root.showHotcues)

            PropertyChanges {
                target: stemTab
                visible: true
            }
            PropertyChanges {
                target: hotcueTab
                visible: false
            }
        }
    ]

    Item {
        id: hotcueTab

        anchors.fill: parent
        anchors.right: tabs.left
        anchors.rightMargin: Theme.space1
        visible: false

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

                Item {
                    required property int index

                    Layout.column: index % 4
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.row: parseInt(index / 4)

                    Skin.Hotcue {
                        id: hotcue

                        readonly property var label: isSet ? root.currentTrack.hotcuesModel.get(index).label : null

                        activate: activator.pressedButtons == Qt.LeftButton
                        group: root.group
                        hotcueNumber: index + 1
                    }
                    Rectangle {
                        id: hotcueBackground

                        anchors.fill: parent
                        border.color: hotcue.isSet
                                ? Qt.lighter(hotcue.color, 1.2)
                                : Theme.border
                        border.width: hotcue.isSet ? 1 : 1
                        color: hotcue.isSet ? hotcue.color : Theme.surface1
                        radius: Theme.radiusSm

                        // Lit top edge for the pad face.
                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 1
                            anchors.right: parent.right
                            anchors.rightMargin: 1
                            anchors.top: parent.top
                            anchors.topMargin: 1
                            color: hotcue.isSet ? Qt.rgba(255, 255, 255, 0.28) : Theme.topHighlight
                            height: 1
                        }

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.motionFast
                            }
                        }
                    }
                    MouseArea {
                        id: activator

                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        anchors.fill: parent
                    }
                    ColumnLayout {
                        anchors.centerIn: hotcueBackground
                        spacing: 0

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            color: hotcue.isSet
                                    ? root.hotcueTextColorFor(hotcueBackground.color)
                                    : Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLg
                            font.weight: Theme.fontWeightDemiBold
                            horizontalAlignment: Text.AlignHCenter
                            text: `${index + 1}`
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            color: hotcue.isSet
                                    ? root.hotcueTextColorFor(hotcueBackground.color)
                                    : Theme.textMuted
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            text: hotcue.label ?? ""
                            visible: !!hotcue.label
                        }
                    }
                }
            }
        }
    }
    Item {
        id: stemTab

        anchors.fill: parent
        anchors.right: tabs.left
        anchors.rightMargin: Theme.space1
        visible: false

        Skin.FadeBehavior on visible {
            fadeTarget: stemTab
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space2
            spacing: Theme.space1

            RowLayout {
                id: stemHeader

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
                    Layout.preferredHeight: 16
                    Accessible.name: qsTr("Acapella: mute all non-vocal stems")
                    implicitWidth: 74
                    text: qsTr("Acapella")

                    onClicked: root.applyAcapella()
                }
                Skin.Button {
                    Layout.preferredHeight: 16
                    Accessible.name: qsTr("Instrumental: mute the vocal stem")
                    implicitWidth: 94
                    text: qsTr("Instrumental")

                    onClicked: root.applyInstrumental()
                }
            }
            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: Theme.space1

                Repeater {
                    id: stemLaneRepeater

                    delegate: StemLane {
                    }
                    model: root.currentTrack ? root.currentTrack.stemsModel : null
                }
            }
        }
    }
    ColumnLayout {
        id: tabs

        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.rightMargin: Theme.space1
        anchors.top: parent.top
        spacing: Theme.space2
        width: (root.showHotcues || root.showStems) ? 36 : 0

        Behavior on width {
            SpringAnimation {
                damping: 0.2
                duration: 500
                spring: 2
            }
        }

        Skin.Button {
            id: hotcueTabButton

            Layout.fillWidth: true
            visible: root.showHotcues
            Layout.margins: 0
            activeColor: Theme.textOnAccent
            checked: true
            implicitHeight: 30

            Accessible.name: qsTr("Hotcues")

            contentItem: Shape {
                anchors.fill: parent
                antialiasing: true
                layer.enabled: true
                layer.samples: 4

                ShapePath {
                    fillColor: hotcueTabButton.checked ? Theme.textOnAccent : Theme.textSecondary
                    startX: 10
                    startY: 4

                    PathLine {
                        x: 26
                        y: 4
                    }
                    PathLine {
                        x: 19.5
                        y: 10
                    }
                    PathLine {
                        x: 19.5
                        y: 24
                    }
                    PathLine {
                        x: 16.5
                        y: 24
                    }
                    PathLine {
                        x: 16.5
                        y: 10
                    }
                    PathLine {
                        x: 10
                        y: 4
                    }
                }
            }

            onClicked: {
                stemTabButton.checked = false;
                hotcueTabButton.checked = trackLoadedControl.value == 1;
            }
        }
        Skin.Button {
            id: stemTabButton

            Layout.fillWidth: true
            visible: root.showStems
            Layout.margins: 0
            activeColor: Theme.textOnAccent
            implicitHeight: 30

            Accessible.name: qsTr("Stems")

            contentItem: Item {
                readonly property color iconColor: stemTabButton.checked ? Theme.textOnAccent : Theme.textSecondary

                Rectangle {
                    color: parent.iconColor
                    height: 1

                    anchors {
                        left: parent.left
                        leftMargin: 5
                        right: parent.right
                        rightMargin: 5
                        top: parent.top
                        topMargin: 6
                    }
                }
                Rectangle {
                    color: parent.iconColor
                    height: 1

                    anchors {
                        left: parent.left
                        leftMargin: 5
                        right: parent.right
                        rightMargin: 14
                        top: parent.top
                        topMargin: 12
                    }
                }
                Rectangle {
                    color: parent.iconColor
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 12
                        left: parent.left
                        leftMargin: 5
                        right: parent.right
                        rightMargin: 22
                    }
                }
                Rectangle {
                    color: parent.iconColor
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 13
                        left: parent.left
                        leftMargin: 22
                        right: parent.right
                        rightMargin: 5
                    }
                }
                Rectangle {
                    color: parent.iconColor
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 6
                        left: parent.left
                        leftMargin: 5
                        right: parent.right
                        rightMargin: 26
                    }
                }
                Rectangle {
                    color: parent.iconColor
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 6
                        left: parent.left
                        leftMargin: 14
                        right: parent.right
                        rightMargin: 15
                    }
                }
                Rectangle {
                    color: parent.iconColor
                    height: 1

                    anchors {
                        bottom: parent.bottom
                        bottomMargin: 6
                        left: parent.left
                        leftMargin: 26
                        right: parent.right
                        rightMargin: 5
                    }
                }
            }

            onClicked: {
                stemTabButton.checked = trackLoadedControl.value == 1 && stemCountControl.value != 0;
                hotcueTabButton.checked = !stemTabButton.checked;
            }
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

            Behavior on color {
                ColorAnimation {
                    duration: Theme.motionFast
                }
            }
            Behavior on border.color {
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

    /// One stem lane: colour chip + label + status dot, mute/solo, vertical
    /// volume fader, QuickFX knob with enable toggle and chain preset.
    component StemLane: Item {
        id: lane

        required property color color
        required property int index
        required property string label

        readonly property string displayLabel: label && label.length > 0 ? label : qsTr("Stem %1").arg(index + 1)
        readonly property bool fxEnabled: fxEnabledControl.value != 0
        readonly property string fxGroup: root.quickEffectGroupFor(index)
        readonly property string kind: root.stemKindFor(label, index, root.laneCount)
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

            // Vertical volume fader.
            Skin.ControlFader {
                id: volumeFader

                Layout.fillHeight: true
                Layout.preferredWidth: 20
                group: lane.stemGroup
                implicitHeight: Theme.stemLaneHeight
                implicitWidth: 20
                key: "volume"
                showDefaultHandle: false

                Accessible.name: qsTr("%1 volume").arg(lane.displayLabel)

                bar.color: lane.laneColor
                bar.enabled: true
                bar.margin: 2
                bar.start: 0
                bar.width: 3

                background: Rectangle {
                    anchors.fill: parent
                    color: "transparent"

                    Rectangle {
                        anchors.centerIn: parent
                        color: Theme.surface3
                        height: parent.height
                        radius: 2
                        width: 4
                    }
                }
                handle: Rectangle {
                    border.color: Theme.borderStrong
                    border.width: 1
                    color: lane.muted ? Theme.textMuted : Theme.textPrimary
                    height: 6
                    radius: 3
                    width: 18
                    x: Math.round((volumeFader.width - width) / 2)
                    y: Math.round(volumeFader.visualPosition * (volumeFader.height - height))

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
                spacing: 2

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

                        Layout.fillWidth: true
                        active: lane.muted
                        activeBackground: Theme.danger
                        text: qsTr("M")

                        Accessible.name: qsTr("Mute %1").arg(lane.displayLabel)

                        onClicked: lane.toggleMute()
                    }
                    StemToggleButton {
                        id: soloButton

                        Layout.fillWidth: true
                        active: lane.soloed
                        activeBackground: Theme.accent
                        text: qsTr("S")

                        Accessible.name: qsTr("Solo %1").arg(lane.displayLabel)

                        onClicked: root.toggleSolo(lane.index)
                    }
                }
                RowLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: Theme.space1

                    Skin.ControlMiniKnob {
                        id: fxKnob

                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredHeight: 24
                        Layout.preferredWidth: 24
                        color: lane.fxEnabled ? Theme.accent : Theme.textMuted
                        group: lane.fxGroup
                        key: "super1"

                        Accessible.name: qsTr("%1 QuickFX").arg(lane.displayLabel)
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Item {
                        id: fxEnableButton

                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredHeight: 12
                        Layout.preferredWidth: 12

                        Accessible.checkable: true
                        Accessible.checked: lane.fxEnabled
                        Accessible.name: qsTr("%1 QuickFX enabled").arg(lane.displayLabel)
                        Accessible.role: Accessible.CheckBox

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
                    visible: !root.dense

                    Accessible.name: qsTr("%1 QuickFX preset").arg(lane.displayLabel)

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
}
