pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

/// Control strip for the split stem waveform, drawn over the left edge of the
/// deck waveform: one row per stem with its lane color, name, mute key, solo
/// key and a horizontal volume slider.
///
/// Rows are a quarter of the display height, matching the lanes the stem
/// renderer draws, so the controls always line up with their waveforms. A lane
/// is about 20 px tall when the library is visible, so every control is sized
/// to fit inside that row.
///
/// Solo is implemented in QML only: activating it mutes every lane that is not
/// in the solo set and the previous mute states are restored when the set is
/// cleared. A native engine solo control (e.g. `[ChannelN_StemX], solo`) is a
/// later change; once it exists the bookkeeping below can go away.
Item {
    id: root

    required property string group
    /// `QmlStemsModel` of the loaded track, or null when it has no stems.
    required property var stemsModel

    /// Indexes of the lanes that are currently soloed. Empty means solo is off.
    property var soloLanes: []
    /// Mute state of every lane captured when the first lane was soloed. It is
    /// only read when the last solo is cleared, so a mute changed from a
    /// controller during solo cannot overwrite the state to restore.
    property var savedMuteStates: []

    readonly property int laneHeight: Math.round(height / 4)
    /// Width a row needs for the label, M, S and a usable volume slider. Below
    /// it the slider stays hidden until the row is hovered.
    readonly property real fullRowWidth: Math.round(Theme.stemLaneComfortWidth + Theme.space7)
    readonly property bool narrow: root.width < root.fullRowWidth

    // The strip overlays the waveform: never let a squeezed row paint outside
    // the deck card.
    clip: true

    /// `[ChannelN]` -> `[ChannelN_StemX]`
    function stemGroupFor(index) {
        return `${root.group.substr(0, root.group.length - 1)}_Stem${index + 1}]`;
    }
    /// Canonical lane name. Lane order is fixed to drums, bass, other, vocals,
    /// so the name comes from the position rather than from the model label.
    function canonicalNameFor(index) {
        return [qsTr("Drums"), qsTr("Bass"), qsTr("Other"), qsTr("Vocals")][index] ?? qsTr("Stem %1").arg(index + 1);
    }
    function canonicalKindFor(index) {
        return ["drums", "bass", "other", "vocals"][index] ?? "other";
    }
    /// Classify a declared stem label. An empty result means the label is not a
    /// stem kind, so it must be a name the stem file chose itself.
    function knownKindFor(label) {
        const value = String(label ?? "").toLowerCase();
        if (value.indexOf("vocal") >= 0 || value.indexOf("acapella") >= 0
                || value.indexOf("voice") >= 0)
            return "vocals";
        if (value.indexOf("drum") >= 0)
            return "drums";
        if (value.indexOf("bass") >= 0)
            return "bass";
        if (value.indexOf("other") >= 0)
            return "other";
        return "";
    }
    function laneKindFor(label, index) {
        return root.knownKindFor(label) || root.canonicalKindFor(index);
    }
    /// The file's own label is shown only when it declares something other than
    /// the canonical stem for the lane; an unlabeled set keeps positional names.
    function laneLabelFor(label, index) {
        const declared = String(label ?? "").trim();
        if (declared.length === 0)
            return root.canonicalNameFor(index);
        const known = root.knownKindFor(declared);
        if (known.length === 0)
            return declared;
        return known === root.canonicalKindFor(index)
                ? root.canonicalNameFor(index)
                : declared;
    }
    /// Canonical Traktor-like stem palette, the same colors the waveform lanes
    /// use. Labels in the stored stem set can predate the palette, so the color
    /// is derived from the stem kind rather than from the model.
    function stemColorFor(kind) {
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

    function laneAt(index) {
        return laneRepeater.itemAt(index);
    }
    function isSoloed(index) {
        return root.soloLanes.indexOf(index) >= 0;
    }
    function captureMuteStates() {
        const saved = [];
        for (let i = 0; i < laneRepeater.count; ++i) {
            const lane = laneRepeater.itemAt(i);
            saved.push(lane ? lane.muted : false);
        }
        root.savedMuteStates = saved;
    }
    function restoreMuteStates() {
        const saved = root.savedMuteStates;
        for (let i = 0; i < laneRepeater.count; ++i) {
            const lane = laneRepeater.itemAt(i);
            if (lane)
                lane.setMuted(saved[i] === true);
        }
        root.savedMuteStates = [];
    }
    function applySoloMutes() {
        for (let i = 0; i < laneRepeater.count; ++i) {
            const lane = laneRepeater.itemAt(i);
            if (lane)
                lane.setMuted(!root.isSoloed(i));
        }
    }
    /// Add a lane to the solo set, or remove it. Entering solo saves the mute
    /// state once; clearing the set restores exactly what was saved, whatever a
    /// controller did to the mutes in the meantime.
    function toggleSolo(index) {
        const lanes = root.soloLanes.slice();
        const position = lanes.indexOf(index);
        if (position >= 0) {
            lanes.splice(position, 1);
        } else {
            if (lanes.length === 0)
                root.captureMuteStates();
            lanes.push(index);
        }
        root.soloLanes = lanes;
        if (lanes.length === 0)
            root.restoreMuteStates();
        else
            root.applySoloMutes();
    }
    function toggleMute(index) {
        // A manual mute click wins over solo: drop solo first so the saved mute
        // states are restored before the user's edit, instead of storing a
        // solo-forced mute as the previous state.
        if (root.soloLanes.length > 0)
            root.clearSolo();
        const lane = root.laneAt(index);
        if (lane)
            lane.setMuted(!lane.muted);
    }
    function clearSolo() {
        if (root.soloLanes.length === 0)
            return;
        root.soloLanes = [];
        root.restoreMuteStates();
    }

    onStemsModelChanged: {
        root.soloLanes = [];
        root.savedMuteStates = [];
    }

    // A new track can bring fewer stem lanes than the one that was soloed;
    // drop solo indexes that no longer have a lane instead of leaving every
    // remaining lane muted with no way to see why.
    Connections {
        target: laneRepeater

        function onCountChanged() {
            const soloed = root.soloLanes.filter(index => index < laneRepeater.count);
            if (soloed.length === root.soloLanes.length)
                return;
            root.soloLanes = soloed;
            if (soloed.length === 0)
                root.restoreMuteStates();
            else
                root.applySoloMutes();
        }
    }

    // Solid behind the controls, fading out to the right so the waveform
    // appears to start where the strip ends instead of being cut off by a hard
    // edge.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                color: Qt.alpha(Theme.surface0, 0.92)
                position: 0
            }
            GradientStop {
                color: Qt.alpha(Theme.surface0, 0.92)
                position: 0.86
            }
            GradientStop {
                color: Qt.alpha(Theme.surface0, 0)
                position: 1
            }
        }
    }
    // The strip is not a seek area: swallow presses and wheel so scrubbing the
    // waveform never starts underneath the controls. The lanes draw above this
    // so their keys and slider still receive their own input.
    MouseArea {
        acceptedButtons: Qt.AllButtons
        anchors.fill: parent

        onWheel: wheel => wheel.accepted = true
    }
    Column {
        anchors.fill: parent

        Repeater {
            id: laneRepeater

            model: root.stemsModel

            delegate: Item {
                id: lane

                required property int index
                required property string label

                readonly property string kind: root.laneKindFor(lane.label, lane.index)
                readonly property string displayLabel: root.laneLabelFor(lane.label, lane.index)
                readonly property color laneColor: root.stemColorFor(lane.kind)
                readonly property bool muted: muteControl.value != 0
                readonly property bool soloed: root.isSoloed(lane.index)
                readonly property string stemGroup: root.stemGroupFor(lane.index)
                /// Above the narrow threshold the slider is always visible; below
                /// it the row shows label, M and S until the pointer arrives.
                readonly property bool revealVolume: !root.narrow || rowHover.hovered
                        || volumeSlider.pressed || volumeSlider.activeFocus
                /// M/S keys stay a key-sized target at 20 px but stop growing
                /// once the lanes are tall (library hidden), and shrink with a
                /// squeezed lane so they never spill into the next row.
                readonly property real keySize: Math.max(7, Math.min(Theme.space4, Math.round(lane.height * 0.7), lane.height - 4))

                function setMuted(value) {
                    muteControl.value = value ? 1 : 0;
                }

                height: root.laneHeight
                visible: lane.index < 4
                width: root.width

                HoverHandler {
                    id: rowHover
                }
                Mixxx.ControlProxy {
                    id: muteControl

                    group: lane.stemGroup
                    key: "mute"
                }
                RowLayout {
                    anchors.bottomMargin: Theme.space1 / 2
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space1
                    anchors.rightMargin: Theme.space1
                    anchors.topMargin: Theme.space1 / 2
                    spacing: Theme.space1

                    // Lane color key. It is a status light now that M owns the
                    // mute toggle: lit while the stem is audible, hollow and
                    // gray while it is muted, accent-framed while soloed.
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 3
                        border.color: lane.soloed ? Theme.accent : (lane.muted ? Theme.textMuted : lane.laneColor)
                        border.width: lane.muted || lane.soloed ? 1 : 0
                        color: lane.muted ? "transparent" : lane.laneColor
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
                    Text {
                        Layout.fillWidth: true
                        color: lane.muted ? Theme.textMuted : Theme.textPrimary
                        elide: Text.ElideRight
                        font.capitalization: Font.AllUppercase
                        font.family: Theme.fontFamily
                        font.letterSpacing: Theme.letterSpacingCaps
                        font.pixelSize: Math.min(Theme.fontSizeXs, Math.max(7, lane.height - 4))
                        font.weight: Theme.fontWeightDemiBold
                        text: lane.displayLabel
                        verticalAlignment: Text.AlignVCenter
                    }
                    StemToggleButton {
                        id: muteKey

                        Accessible.checkable: true
                        Accessible.checked: lane.muted
                        Accessible.description: qsTr("Silences the %1 lane").arg(lane.displayLabel)
                        Accessible.name: qsTr("Mute %1").arg(lane.displayLabel)
                        Accessible.role: Accessible.CheckBox
                        Layout.preferredHeight: lane.keySize
                        Layout.preferredWidth: lane.keySize
                        active: lane.muted
                        activeBackground: Theme.danger
                        text: "M"

                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Mute %1").arg(lane.displayLabel)
                        ToolTip.visible: muteKey.hovered

                        onClicked: root.toggleMute(lane.index)
                    }
                    StemToggleButton {
                        id: soloKey

                        Accessible.checkable: true
                        Accessible.checked: lane.soloed
                        Accessible.description: qsTr("Mutes the other stem lanes and keeps %1 audible").arg(lane.displayLabel)
                        Accessible.name: qsTr("Solo %1").arg(lane.displayLabel)
                        Accessible.role: Accessible.CheckBox
                        Layout.preferredHeight: lane.keySize
                        Layout.preferredWidth: lane.keySize
                        active: lane.soloed
                        activeBackground: Theme.accent
                        text: "S"

                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Solo %1").arg(lane.displayLabel)
                        ToolTip.visible: soloKey.hovered

                        onClicked: root.toggleSolo(lane.index)
                    }
                    // Horizontal gain slider. The stem volume control is a
                    // linear 0..1 gain, so 100 % (unity) is the right end of
                    // the range; double-click restores that default.
                    Skin.ControlFader {
                        id: volumeSlider

                        Accessible.description: qsTr("Double-click to reset to 100 percent")
                        Accessible.name: qsTr("%1 volume").arg(lane.displayLabel)
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: Math.round(Theme.stemLaneMinWidth * 0.6)
                        background: Rectangle {
                            color: "transparent"

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.surface3
                                height: Math.max(2, Math.round(Theme.uiScale * 2))
                                radius: Theme.radiusXs
                            }
                        }
                        // The strip swallows wheel events so the track never
                        // scrubs; keep the slider from consuming them first.
                        bar.color: lane.muted ? Theme.textMuted : lane.laneColor
                        bar.enabled: true
                        bar.margin: 2
                        bar.start: 0
                        bar.width: Math.max(2, Math.round(Theme.uiScale * 2))
                        from: 0
                        group: lane.stemGroup
                        handle: Rectangle {
                            border.color: Theme.borderStrong
                            border.width: 1
                            color: lane.muted ? Theme.textMuted : Theme.textPrimary
                            height: Math.max(8, volumeSlider.height - 6)
                            radius: Theme.radiusXs
                            width: Math.max(4, Theme.space1)
                            x: Math.round(volumeSlider.visualPosition * (volumeSlider.width - width))
                            y: Math.round((volumeSlider.height - height) / 2)
                        }
                        key: "volume"
                        maximum: 1
                        orientation: Qt.Horizontal
                        to: 1
                        visible: lane.revealVolume
                        wheelEnabled: false

                        HoverHandler {
                            id: volumeHover
                        }
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("%1 volume, double-click to reset").arg(lane.displayLabel)
                        ToolTip.visible: volumeHover.hovered
                    }
                }
            }
        }
    }
    /// Compact M/S key used by the stem rows.
    component StemToggleButton: Skin.Button {
        id: control

        property bool active: false
        property color activeBackground: Theme.accent
        property color activeTextColor: Theme.textOnAccent

        checkable: false
        implicitHeight: 14
        implicitWidth: 14

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
            font.pixelSize: Math.min(Theme.fontSizeXs, Math.max(7, control.height - 4))
            horizontalAlignment: Text.AlignHCenter
            text: control.text
            verticalAlignment: Text.AlignVCenter
        }
    }
}
