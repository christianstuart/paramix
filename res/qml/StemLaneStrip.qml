pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import "Theme"

/// Label strip for the split stem waveform, drawn over the left edge of the
/// deck waveform: one row per stem with its colour key, name and gain knob.
///
/// Rows are a quarter of the display height, matching the lanes the stem
/// renderer draws, so the labels always line up with their waveforms.
Item {
    id: root

    required property string group
    /// `QmlStemsModel` of the loaded track, or null when it has no stems.
    required property var stemsModel

    readonly property int laneHeight: Math.round(height / 4)

    /// `[ChannelN]` -> `[ChannelN_StemX]`
    function stemGroupFor(index) {
        return `${root.group.substr(0, root.group.length - 1)}_Stem${index + 1}]`;
    }
    /// Classify a stem from its model label. Demucs emits drums, bass, other,
    /// vocals in that order, so an unhelpful label falls back to the index.
    function stemKindFor(label, index) {
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
        return ["drums", "bass", "other", "vocals"][index] ?? "other";
    }
    /// Canonical Traktor-like stem palette, the same colours the waveform lanes
    /// use. Labels in the stored stem set can predate the palette, so the colour
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

    // Solid behind the labels, fading out to the right so the waveform appears
    // to start where the strip ends instead of being cut off by a hard edge.
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
    // waveform never starts underneath the labels.
    MouseArea {
        acceptedButtons: Qt.AllButtons
        anchors.fill: parent

        onWheel: wheel => wheel.accepted = true
    }
    Column {
        anchors.fill: parent

        Repeater {
            model: root.stemsModel

            delegate: Item {
                id: lane

                required property color color
                required property int index
                required property string label

                readonly property string displayLabel: label && label.length > 0
                        ? label
                        : qsTr("Stem %1").arg(index + 1)
                readonly property string kind: root.stemKindFor(lane.label, lane.index)
                readonly property color laneColor: root.stemColorFor(lane.kind)
                readonly property bool muted: muteControl.value != 0
                readonly property string stemGroup: root.stemGroupFor(lane.index)

                height: root.laneHeight
                visible: lane.index < 4
                width: root.width

                Mixxx.ControlProxy {
                    id: muteControl

                    group: lane.stemGroup
                    key: "mute"
                }
                // Colour key doubles as the mute toggle: lit while the stem is
                // audible, hollow while it is muted.
                Rectangle {
                    id: colorKey

                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 4
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    border.color: lane.muted ? Theme.textMuted : lane.laneColor
                    border.width: lane.muted ? 1 : 0
                    color: lane.muted ? "transparent" : lane.laneColor
                    radius: 1.5
                    width: 3

                    Accessible.checkable: true
                    Accessible.checked: lane.muted
                    Accessible.name: qsTr("Mute %1").arg(lane.displayLabel)
                    Accessible.role: Accessible.CheckBox

                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        onTapped: muteControl.value = lane.muted ? 0 : 1
                    }
                }
                Text {
                    anchors.left: colorKey.right
                    anchors.leftMargin: 8
                    anchors.right: volumeKnob.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    color: lane.muted ? Theme.textMuted : Theme.textPrimary
                    elide: Text.ElideRight
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.letterSpacing: Theme.letterSpacingCaps
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Theme.fontWeightDemiBold
                    text: lane.displayLabel
                }
                Skin.ControlKnob {
                    id: volumeKnob

                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    color: lane.muted ? Theme.textMuted : lane.laneColor
                    group: lane.stemGroup
                    height: Math.min(26, Math.max(14, lane.height - 12))
                    key: "volume"
                    width: height

                    Accessible.name: qsTr("%1 volume").arg(lane.displayLabel)
                }
            }
        }
    }
}
