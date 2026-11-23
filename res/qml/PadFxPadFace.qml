import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import "Theme"

/// One momentary Pad FX pad face. The assignment is shared through [PadFX];
/// the trigger and active state belong to the deck's [ChannelN_PadFX] group.
Skin.Button {
    id: root

    required property string padFxGroup
    required property int bank
    required property int pad
    property string presetGroup: "[PadFX]"
    /// One-line face ("N name", elided) for the compact 4 x 2 deck grid.
    property bool oneLine: false
    /// Face text size: larger once the pad is tall enough to read it.
    readonly property int faceFontSize: root.height >= 34 ? Theme.fontSizeMd : Theme.fontSizeXs
    signal editRequested(int pad, Item anchor)

    readonly property var padEffects: Mixxx.EffectsManager.padEffectsModel
    readonly property string deckGroup: root.padFxGroup.replace("_PadFX]", "]")
    readonly property int stemCount: Math.max(0, Math.round(stemCountControl.value))
    readonly property int effectIndex: Math.max(0, Math.round(effectControl.value))
    readonly property int targetMask: Math.max(0, Math.min(15, Math.round(targetControl.value)))
    readonly property bool isActive: padActive.value > 0
    readonly property bool targetNeedsStems: root.targetMask !== 0
    readonly property bool stemTargetUnavailable: root.targetNeedsStems && root.stemCount === 0
    readonly property var stemNames: [qsTr("Drums"), qsTr("Bass"), qsTr("Other"), qsTr("Vocals")]
    readonly property string effectText: root.effectName(root.effectIndex)
    readonly property string targetText: root.targetWords(root.targetMask)
    readonly property string descriptionText: qsTr("Pad %1: %2, %3")
            .arg(root.pad)
            .arg(root.effectText)
            .arg(root.targetText)

    function effectName(index) {
        if (index <= 0)
            return qsTr("Off");
        const row = root.padEffects.get(index);
        return row && row.display ? row.display : qsTr("Off");
    }
    function targetWords(mask) {
        const value = Math.round(mask);
        if (value === 0)
            return qsTr("Deck");
        if (value === 15)
            return qsTr("All");
        if (value === 7)
            return qsTr("Instrumental");
        if (value === 8)
            return qsTr("Vocals");
        let parts = [];
        for (let bit = 0; bit < 4; ++bit) {
            if (value & (1 << bit))
                parts.push(root.stemNames[bit]);
        }
        return parts.join(qsTr(", "));
    }

    Accessible.description: {
        let parts = [root.descriptionText,
                     modeControl.value > 0 ? qsTr("Tap to toggle") : qsTr("Hold to engage")];
        if (root.isActive)
            parts.push(qsTr("Active"));
        if (root.stemTargetUnavailable)
            parts.push(qsTr("No stems on this deck; choose Deck to use this pad"));
        parts.push(qsTr("Right click edits this pad on every deck"));
        return parts.join(". ");
    }
    Accessible.name: root.descriptionText
    implicitHeight: Theme.padfxPadHeight
    implicitWidth: Theme.padfxHitHeight * 2
    opacity: root.effectIndex === 0 || root.stemTargetUnavailable ? 0.55 : 1.0
    highlight: root.isActive

    ToolTip.delay: 500
    ToolTip.text: {
        let text = (modeControl.value > 0
                    ? qsTr("Pad %1 - %2 (%3). Tap to engage, tap again to stop.")
                    : qsTr("Pad %1 - %2 (%3). Hold to engage, release to stop."))
                .arg(root.pad)
                .arg(root.effectText)
                .arg(root.targetText);
        if (root.stemTargetUnavailable)
            text += " " + qsTr("This deck has no stems; right click and choose Deck to use it.");
        return text;
    }
    ToolTip.visible: padHover.hovered

    onPressedChanged: padControl.value = root.pressed ? 1 : 0

    background: Rectangle {
        border.color: root.highlight
                      ? Qt.lighter(Theme.accent, 1.12)
                      : Theme.border
        border.width: 1
        color: root.highlight ? Theme.accent : Theme.surface3
        radius: Theme.radiusSm

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
    contentItem: Item {
        Text {
            id: effectNameText

            anchors.bottomMargin: root.oneLine ? 0 : Theme.space1
            anchors.fill: parent
            anchors.leftMargin: root.oneLine
                                ? padNumberText.width + 2 * Theme.space1
                                : Theme.space1
            anchors.rightMargin: Theme.space1
            anchors.topMargin: root.oneLine ? 0 : Theme.space1
            color: root.highlight
                   ? Theme.textOnAccent
                   : root.effectIndex === 0
                     ? Theme.textMuted
                     : Theme.textPrimary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: root.faceFontSize
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: root.oneLine ? Text.AlignLeft : Text.AlignHCenter
            // Narrow 4 x 2 faces wrap names onto two tight lines rather than
            // eliding them; ElideRight is only the last resort.
            lineHeight: root.oneLine ? 0.85 : 1
            maximumLineCount: 2
            text: root.effectText
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
        Text {
            id: padNumberText

            anchors.left: parent.left
            anchors.leftMargin: Theme.space1
            anchors.top: root.oneLine ? undefined : parent.top
            anchors.topMargin: Theme.space1
            anchors.verticalCenter: root.oneLine ? parent.verticalCenter : undefined
            color: root.highlight ? Theme.textOnAccent : Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: root.faceFontSize
            font.weight: Theme.fontWeightDemiBold
            text: root.pad
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton

        onPressed: root.editRequested(root.pad, root)
    }
    HoverHandler {
        id: padHover
    }

    Mixxx.ControlProxy {
        id: effectControl

        group: root.presetGroup
        key: "fx" + root.bank + "_effect" + root.pad
    }
    Mixxx.ControlProxy {
        id: targetControl

        group: root.presetGroup
        key: "fx" + root.bank + "_target" + root.pad
    }
    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.deckGroup
        key: "stem_count"
    }
    Mixxx.ControlProxy {
        id: padControl

        group: root.padFxGroup
        key: "pad" + root.pad
    }
    Mixxx.ControlProxy {
        id: modeControl

        group: root.presetGroup
        key: "fx" + root.bank + "_mode" + root.pad
    }
    Mixxx.ControlProxy {
        id: padActive

        group: root.padFxGroup
        key: "pad" + root.pad + "_active"
    }
}
