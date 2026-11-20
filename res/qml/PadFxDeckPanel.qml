pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

/// One deck column inside the Pad FX window.
///
/// Perform view is the default: a large lit pad, the effect name, a mode badge
/// and labelled stem chips with All / Instrumental / Vocals shortcuts. The
/// pencil key on a pad expands that pad in place and collapses its siblings to
/// one-line summaries, which is where the effect/mode selectors and the effect
/// parameters live. A deck without stems shows one empty-state panel with a
/// "Separate stems" action instead of four disabled pad cards.
Rectangle {
    id: root

    required property string group

    /// 0 = perform view. 1..4 = that pad is expanded in edit view.
    property int editingPad: 0
    property int separationStatus: -1
    property string separationMessage: ""

    readonly property int deckNumber: Number(root.group.replace(/[^0-9]/g, ""))
    readonly property string padFxGroup: root.group.substr(0, root.group.length - 1) + "_PadFX]"
    readonly property int stemCount: Math.max(0, Math.round(stemCountControl.value))
    readonly property var effectNames: [qsTr("Off"), qsTr("Vinyl Brake"), qsTr("Echo Out")]
    readonly property var modeNames: [qsTr("Momentary"), qsTr("Toggle"), qsTr("One Shot")]
    readonly property var modeGlyphs: ["H", "L", "1×"]
    readonly property var modeTips: [
        qsTr("Momentary - engages only while the pad is held."),
        qsTr("Toggle - press to engage; press again to release."),
        qsTr("One shot - fires once, then holds the target stems muted until pressed again.")
    ]
    /// Pad card = padding + pad face + chips + shortcuts + two gaps.
    readonly property int padCardHeight: 2 * Theme.space2 + Theme.padfxPadHeight
            + 2 * Theme.space1 + 2 * Theme.padfxHitHeight

    readonly property var deckPlayer: Mixxx.PlayerManager.getPlayer(root.group)
    readonly property var currentTrack: root.deckPlayer ? root.deckPlayer.currentTrack : null

    readonly property bool canSeparate: root.currentTrack !== null
            && Mixxx.StemService.available
            && Mixxx.StemService.hasModel
            && (root.separationStatus === Mixxx.StemService.NotSeparated
                || root.separationStatus === Mixxx.StemService.MissingStemFile
                || root.separationStatus === Mixxx.StemService.Failed)

    readonly property string emptyTitle: {
        if (root.currentTrack === null)
            return qsTr("Load a track to use Pad FX.");
        if (!Mixxx.StemService.available)
            return qsTr("Stem separation is unavailable.");
        if (!Mixxx.StemService.hasModel)
            return qsTr("Stem separation model not installed.");
        if (root.separationStatus === Mixxx.StemService.Queued)
            return qsTr("Queued for stem separation.");
        if (root.separationStatus === Mixxx.StemService.Separating)
            return qsTr("Separating stems — %1%").arg(Mixxx.StemService.progress);
        if (root.separationStatus === Mixxx.StemService.Ready)
            return qsTr("Stems are ready; waiting for the deck to reload them.");
        return qsTr("This deck has no stems.");
    }
    readonly property string emptyBody: {
        if (root.currentTrack === null
                || !Mixxx.StemService.available
                || !Mixxx.StemService.hasModel)
            return "";
        if (root.separationStatus === Mixxx.StemService.Failed)
            return root.separationMessage;
        if (root.separationStatus === Mixxx.StemService.Queued)
            return qsTr("Waiting for the previous separation job to finish.");
        if (root.separationStatus === Mixxx.StemService.Separating)
            return qsTr("The deck keeps playing while the stems are prepared.");
        if (root.separationStatus === Mixxx.StemService.Ready)
            return qsTr("The Pad FX grid appears as soon as the deck loads them.");
        const title = root.currentTrack.title || "";
        return title.length > 0
                ? qsTr("\"%1\" is loaded, but it has no separated stems yet.").arg(title)
                : qsTr("The loaded track has no separated stems yet.");
    }

    function stemLabel(bit) {
        return [qsTr("Drums"), qsTr("Bass"), qsTr("Other"), qsTr("Vocals")][bit] || "";
    }
    function stemColor(bit) {
        return [Theme.stemDrums, Theme.stemBass, Theme.stemOther, Theme.stemVocals][bit]
                || Theme.stemMuted;
    }
    /// Full words, so a pad never reads as "D+B".
    function targetWords(mask) {
        const value = Math.round(mask);
        if (value === 15)
            return qsTr("All");
        if (value === 7)
            return qsTr("Instrumental");
        if (value === 8)
            return qsTr("Vocals");
        let parts = [];
        for (let bit = 0; bit < 4; ++bit) {
            if (value & (1 << bit))
                parts.push(root.stemLabel(bit));
        }
        return parts.join(qsTr(", "));
    }
    function targetSummary(mask) {
        const value = Math.round(mask);
        if (value === 15)
            return qsTr("All");
        if (value === 7)
            return qsTr("Instrumental");
        if (value === 8)
            return qsTr("Vocals");
        let parts = [];
        for (let bit = 0; bit < 4; ++bit) {
            if (value & (1 << bit))
                parts.push(root.stemLabel(bit));
        }
        return parts.join("+");
    }
    function paramLabel(effect, index) {
        if (effect === 1)
            return [qsTr("Wind-down"), qsTr("Curve"), qsTr("Wind-up"), "", ""][index] || "";
        if (effect === 2)
            return [qsTr("Echo Time"), qsTr("Feedback"), qsTr("Tail"), qsTr("Reverb"), qsTr("Rev Time")][index] || "";
        return "";
    }
    function toggleTarget(targetControl, bit) {
        const current = Math.round(targetControl.value);
        const next = current ^ (1 << bit);
        targetControl.value = next === 0 ? (1 << bit) : next;
    }
    function effectName(effect) {
        return root.effectNames[Math.max(0, Math.min(2, Math.round(effect)))];
    }
    function modeName(mode) {
        return root.modeNames[Math.max(0, Math.min(2, Math.round(mode)))];
    }
    function padDescription(pad, effect, mode, mask) {
        return qsTr("Pad %1: %2, %3, %4")
                .arg(pad)
                .arg(root.effectName(effect))
                .arg(root.modeName(mode))
                .arg(root.targetWords(mask));
    }
    function padTooltip(pad, effect, mode, mask, muted) {
        if (Math.round(effect) === 0)
            return qsTr("Pad %1 has no effect configured.").arg(pad);
        let text = qsTr("Pad %1 - %2 (%3, %4). ")
                .arg(pad)
                .arg(root.effectName(effect))
                .arg(root.modeName(mode))
                .arg(root.targetWords(mask));
        text += root.modeTips[Math.max(0, Math.min(2, Math.round(mode)))];
        if (muted)
            text += " " + qsTr("Press the pad to release the mute.");
        return text;
    }
    function refreshSeparationStatus() {
        root.separationStatus = root.currentTrack
                ? Mixxx.StemService.statusForTrack(root.currentTrack)
                : -1;
        root.separationMessage = root.currentTrack
                ? Mixxx.StemService.messageForTrack(root.currentTrack)
                : "";
    }

    color: Theme.surface1
    implicitWidth: Theme.padfxDeckMinWidth
    radius: Theme.radiusMd

    onCurrentTrackChanged: root.refreshSeparationStatus()
    onGroupChanged: root.editingPad = 0
    onStemCountChanged: {
        if (root.stemCount === 0)
            root.editingPad = 0;
    }
    Component.onCompleted: root.refreshSeparationStatus()

    Connections {
        target: Mixxx.StemService

        function onQueueChanged() {
            root.refreshSeparationStatus();
        }
        function onStemsReady(trackId) {
            root.refreshSeparationStatus();
        }
        function onTrackStatusChanged(trackId, status, progress, message) {
            root.refreshSeparationStatus();
        }
    }

    // Exit edit view with Escape. Only the deck that is actually editing
    // enables the shortcut, so one Escape cannot collapse a sibling deck.
    Shortcut {
        enabled: root.editingPad > 0
        sequence: "Escape"

        onActivated: root.editingPad = 0
    }

    Mixxx.ControlProxy {
        id: bankControl

        group: root.padFxGroup
        key: "bank"
    }
    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.space6
            spacing: Theme.space1

            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Deck %1").arg(root.deckNumber)
                verticalAlignment: Text.AlignVCenter
            }
            Repeater {
                model: 4

                delegate: Skin.Button {
                    id: bankKey

                    required property int index

                    readonly property int bankValue: bankKey.index + 1
                    readonly property string bankLetter: String.fromCharCode(65 + bankKey.index)
                    readonly property bool current: Math.round(bankControl.value) === bankKey.bankValue

                    Accessible.checkable: true
                    Accessible.checked: bankKey.current
                    Accessible.name: qsTr("Pad FX bank %1 for deck %2").arg(bankKey.bankLetter).arg(root.deckNumber)
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredHeight: Theme.padfxHitHeight
                    Layout.preferredWidth: Theme.padfxHitHeight
                    enabled: root.stemCount > 0
                    highlight: bankKey.current
                    implicitHeight: Theme.padfxHitHeight
                    implicitWidth: Theme.padfxHitHeight
                    text: bankKey.bankLetter

                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Bank %1 - four independent pad configurations").arg(bankKey.bankLetter)
                    ToolTip.visible: bankKey.hovered

                    onClicked: bankControl.value = bankKey.bankValue
                }
            }
        }

        Flickable {
            id: body

            Layout.fillWidth: true
            Layout.fillHeight: true
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            contentHeight: Math.max(padColumn.implicitHeight, body.height)
            contentWidth: body.width
            visible: root.stemCount > 0

            ColumnLayout {
                id: padColumn

                spacing: Theme.space2
                width: body.width
                height: body.contentHeight

                Repeater {
                    model: 4

                    delegate: Rectangle {
                        id: padCard

                        required property int index

                        readonly property int pad: padCard.index + 1
                        readonly property string padKey: "pad" + padCard.pad
                        readonly property int effectIndex: Math.round(padEffect.value)
                        readonly property int modeIndex: Math.max(0, Math.min(2, Math.round(padMode.value)))
                        readonly property int targetMask: Math.round(padTarget.value)
                        readonly property bool isEditing: root.editingPad === padCard.pad
                        readonly property bool isSummary: root.editingPad > 0 && !padCard.isEditing
                        readonly property bool isActive: padActive.value > 0
                        readonly property bool isMuted: padMuted.value > 0
                        readonly property string effectText: root.effectName(padCard.effectIndex)
                        readonly property string descriptionText: root.padDescription(
                                                                         padCard.pad,
                                                                         padCard.effectIndex,
                                                                         padCard.modeIndex,
                                                                         padCard.targetMask)

                        Layout.fillHeight: root.editingPad === 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.editingPad === 0
                                                 ? root.padCardHeight
                                                 : padCard.isEditing
                                                   ? cardContent.implicitHeight + 2 * Theme.space2
                                                   : Theme.padfxControlHeight
                        color: Theme.surface2
                        radius: Theme.radiusSm

                        Mixxx.ControlProxy {
                            id: padControl

                            group: root.padFxGroup
                            key: padCard.padKey
                        }
                        Mixxx.ControlProxy {
                            id: padActive

                            group: root.padFxGroup
                            key: padCard.padKey + "_active"
                        }
                        Mixxx.ControlProxy {
                            id: padMuted

                            group: root.padFxGroup
                            key: padCard.padKey + "_muted"
                        }
                        Mixxx.ControlProxy {
                            id: padEffect

                            group: root.padFxGroup
                            key: "effect" + padCard.pad
                        }
                        Mixxx.ControlProxy {
                            id: padTarget

                            group: root.padFxGroup
                            key: "target" + padCard.pad
                        }
                        Mixxx.ControlProxy {
                            id: padMode

                            group: root.padFxGroup
                            key: "mode" + padCard.pad
                        }

                        // ── Collapsed summary while a sibling pad is edited ──
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space2
                            anchors.rightMargin: Theme.space1
                            spacing: Theme.space1
                            visible: padCard.isSummary

                            Text {
                                Layout.fillWidth: true
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Theme.fontWeightMedium
                                text: qsTr("PAD %1 · %2 · %3 · %4")
                                        .arg(padCard.pad)
                                        .arg(padCard.effectText)
                                        .arg(root.modeGlyphs[padCard.modeIndex])
                                        .arg(root.targetSummary(padCard.targetMask))
                                verticalAlignment: Text.AlignVCenter
                            }
                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                Accessible.ignored: true
                                border.color: Theme.warning
                                border.width: 1
                                color: Qt.alpha(Theme.warning, 0.14)
                                implicitHeight: Theme.padfxModeBadgeHeight
                                implicitWidth: summaryMutingText.implicitWidth + 2 * Theme.space1
                                radius: Theme.radiusXs
                                visible: padCard.isMuted

                                Text {
                                    id: summaryMutingText

                                    anchors.centerIn: parent
                                    color: Theme.warning
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    font.weight: Theme.fontWeightDemiBold
                                    text: qsTr("⊘ MUTING")
                                }
                            }
                            Skin.Button {
                                Accessible.name: qsTr("Edit pad %1 effect, mode and parameters").arg(padCard.pad)
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredHeight: Theme.padfxHitHeight
                                Layout.preferredWidth: Theme.padfxHitHeight
                                flat: true
                                implicitHeight: Theme.padfxHitHeight
                                implicitWidth: Theme.padfxHitHeight
                                text: "✎"

                                ToolTip.delay: 500
                                ToolTip.text: qsTr("Edit pad %1").arg(padCard.pad)
                                ToolTip.visible: hovered

                                onClicked: root.editingPad = padCard.pad
                            }
                        }

                        // ── Perform card and expanded editor ────────────────
                        ColumnLayout {
                            id: cardContent

                            anchors.fill: parent
                            anchors.margins: Theme.space2
                            spacing: Theme.space1
                            visible: !padCard.isSummary

                            // Pad face + edit rail (perform view)
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.padfxPadHeight
                                spacing: 0
                                visible: !padCard.isEditing

                                Skin.Button {
                                    id: padButton

                                    Accessible.description: {
                                        let parts = [padCard.descriptionText];
                                        if (padCard.isActive)
                                            parts.push(qsTr("Active"));
                                        if (padCard.isMuted)
                                            parts.push(qsTr("Muting"));
                                        if (padCard.effectIndex === 0)
                                            parts.push(qsTr("No effect configured"));
                                        return parts.join(". ");
                                    }
                                    Accessible.name: padCard.descriptionText
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                    highlight: padCard.isActive
                                    implicitHeight: Theme.padfxPadHeight
                                    opacity: padCard.effectIndex === 0 ? 0.55 : 1.0

                                    ToolTip.delay: 500
                                    ToolTip.text: root.padTooltip(padCard.pad,
                                                                  padCard.effectIndex,
                                                                  padCard.modeIndex,
                                                                  padCard.targetMask,
                                                                  padCard.isMuted)
                                    ToolTip.visible: padButton.hovered

                                    onPressedChanged: padControl.value = padButton.pressed ? 1 : 0

                                    background: Rectangle {
                                        border.color: padButton.highlight
                                                      ? Qt.lighter(Theme.accent, 1.12)
                                                      : Theme.border
                                        border.width: 1
                                        color: padButton.highlight ? Theme.accent : Theme.surface3
                                        radius: Theme.radiusSm

                                        Behavior on color {
                                            ColorAnimation {
                                                duration: Theme.motionFast
                                            }
                                        }
                                    }
                                    contentItem: Item {
                                        Text {
                                            id: padCaption

                                            anchors.left: parent.left
                                            anchors.leftMargin: Theme.space2
                                            anchors.top: parent.top
                                            anchors.topMargin: Theme.space2
                                            color: padButton.highlight ? Theme.textOnAccent : Theme.textSecondary
                                            font.capitalization: Font.AllUppercase
                                            font.family: Theme.fontFamily
                                            font.letterSpacing: Theme.letterSpacingCaps
                                            font.pixelSize: Theme.fontSizeXs
                                            font.weight: Theme.fontWeightDemiBold
                                            text: qsTr("Pad %1").arg(padCard.pad)
                                        }
                                        Text {
                                            id: padEffectName

                                            anchors.left: parent.left
                                            anchors.leftMargin: Theme.space2
                                            anchors.right: parent.right
                                            anchors.rightMargin: Theme.space2
                                            anchors.top: padCaption.bottom
                                            anchors.topMargin: Theme.space1
                                            color: padButton.highlight
                                                   ? Theme.textOnAccent
                                                   : padCard.effectIndex === 0
                                                     ? Theme.textMuted
                                                     : Theme.textPrimary
                                            elide: Text.ElideRight
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeMd
                                            font.weight: Theme.fontWeightDemiBold
                                            text: padCard.effectText
                                        }
                                        // Mode badge: H / L / 1x. Informational,
                                        // the full sentence lives in the tooltip and
                                        // in the pad's Accessible.description.
                                        Rectangle {
                                            id: modeBadge

                                            anchors.right: parent.right
                                            anchors.rightMargin: Theme.space1
                                            anchors.top: parent.top
                                            anchors.topMargin: Theme.space1
                                            Accessible.ignored: true
                                            border.color: padButton.highlight
                                                          ? Qt.rgba(1, 1, 1, 0.5)
                                                          : Theme.borderStrong
                                            border.width: 1
                                            color: padButton.highlight
                                                   ? Qt.rgba(0, 0, 0, 0.22)
                                                   : Theme.accentSoft
                                            implicitHeight: Theme.padfxModeBadgeHeight
                                            implicitWidth: Math.max(Theme.padfxModeBadgeHeight,
                                                                    modeBadgeText.implicitWidth + 2 * Theme.space1)
                                            radius: Theme.radiusXs

                                            Text {
                                                id: modeBadgeText

                                                anchors.centerIn: parent
                                                color: padButton.highlight ? Theme.textOnAccent : Theme.textSecondary
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                font.weight: Theme.fontWeightDemiBold
                                                text: root.modeGlyphs[padCard.modeIndex]
                                            }
                                            ToolTip.delay: 500
                                            ToolTip.text: root.modeTips[padCard.modeIndex]
                                            ToolTip.visible: modeBadgeHover.hovered

                                            HoverHandler {
                                                id: modeBadgeHover
                                            }
                                        }
                                        // Amber marker driven by padN_muted, not by
                                        // padN_active: a finished one-shot is dark but
                                        // still owns the stems' mute.
                                        Rectangle {
                                            id: mutingBadge

                                            anchors.bottom: parent.bottom
                                            anchors.bottomMargin: Theme.space1
                                            anchors.right: parent.right
                                            anchors.rightMargin: Theme.space1
                                            Accessible.ignored: true
                                            border.color: Theme.warning
                                            border.width: 1
                                            color: Qt.alpha(Theme.warning, 0.14)
                                            implicitHeight: Theme.padfxModeBadgeHeight
                                            implicitWidth: Math.max(Theme.padfxModeBadgeHeight,
                                                                    mutingBadgeText.implicitWidth + 2 * Theme.space1)
                                            radius: Theme.radiusXs
                                            visible: padCard.isMuted

                                            Text {
                                                id: mutingBadgeText

                                                anchors.centerIn: parent
                                                color: Theme.warning
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                font.weight: Theme.fontWeightDemiBold
                                                text: qsTr("⊘ MUTING")
                                            }
                                            ToolTip.delay: 500
                                            ToolTip.text: qsTr("This pad is holding its target stems muted. Press the pad to release.")
                                            ToolTip.visible: mutingBadgeHover.hovered

                                            HoverHandler {
                                                id: mutingBadgeHover
                                            }
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: Theme.padfxHitHeight
                                    color: "transparent"

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        color: Theme.border
                                        width: 1
                                    }
                                    Skin.Button {
                                        id: editKey

                                        Accessible.name: qsTr("Edit pad %1 effect, mode and parameters").arg(padCard.pad)
                                        anchors.centerIn: parent
                                        flat: true
                                        implicitHeight: Theme.padfxHitHeight
                                        implicitWidth: Theme.padfxHitHeight
                                        text: "✎"

                                        ToolTip.delay: 500
                                        ToolTip.text: qsTr("Edit pad %1").arg(padCard.pad)
                                        ToolTip.visible: editKey.hovered

                                        onClicked: root.editingPad = padCard.pad
                                    }
                                }
                            }

                            // Expanded editor header
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.padfxHitHeight
                                spacing: Theme.space1
                                visible: padCard.isEditing

                                Text {
                                    Layout.fillWidth: true
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    font.capitalization: Font.AllUppercase
                                    font.family: Theme.fontFamily
                                    font.letterSpacing: Theme.letterSpacingCaps
                                    font.pixelSize: Theme.fontSizeXs
                                    font.weight: Theme.fontWeightDemiBold
                                    text: qsTr("Pad %1").arg(padCard.pad)
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Rectangle {
                                    Accessible.ignored: true
                                    border.color: Theme.borderStrong
                                    border.width: 1
                                    color: Theme.accentSoft
                                    implicitHeight: Theme.padfxModeBadgeHeight
                                    implicitWidth: Math.max(Theme.padfxModeBadgeHeight,
                                                            editBadgeText.implicitWidth + 2 * Theme.space1)
                                    radius: Theme.radiusXs

                                    Text {
                                        id: editBadgeText

                                        anchors.centerIn: parent
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        font.weight: Theme.fontWeightDemiBold
                                        text: root.modeGlyphs[padCard.modeIndex]
                                    }
                                    ToolTip.delay: 500
                                    ToolTip.text: root.modeTips[padCard.modeIndex]
                                    ToolTip.visible: editBadgeHover.hovered

                                    HoverHandler {
                                        id: editBadgeHover
                                    }
                                }
                                Skin.Button {
                                    Accessible.name: qsTr("Done editing pad %1").arg(padCard.pad)
                                    Layout.preferredHeight: Theme.padfxHitHeight
                                    implicitHeight: Theme.padfxHitHeight
                                    implicitWidth: Theme.space7
                                    text: qsTr("Done")

                                    ToolTip.delay: 500
                                    ToolTip.text: qsTr("Return pad %1 to perform view").arg(padCard.pad)
                                    ToolTip.visible: hovered

                                    onClicked: root.editingPad = 0
                                }
                            }

                            // Effect selector
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.padfxControlHeight
                                spacing: Theme.space1
                                visible: padCard.isEditing

                                Text {
                                    Layout.preferredHeight: Theme.padfxControlHeight
                                    Layout.preferredWidth: Theme.space8
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    text: qsTr("Effect")
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Skin.ComboBox {
                                    Accessible.name: qsTr("Pad %1 effect").arg(padCard.pad)
                                    Layout.fillWidth: true
                                    currentIndex: padCard.effectIndex
                                    model: root.effectNames
                                    popupMaxItem: 3
                                    textLeftPadding: Theme.space2

                                    ToolTip.delay: 500
                                    ToolTip.text: qsTr("Effect for pad %1").arg(padCard.pad)
                                    ToolTip.visible: hovered

                                    onActivated: index => {
                                        padEffect.value = index;
                                    }
                                }
                            }

                            // Mode selector
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.padfxControlHeight
                                spacing: Theme.space1
                                visible: padCard.isEditing

                                Text {
                                    Layout.preferredHeight: Theme.padfxControlHeight
                                    Layout.preferredWidth: Theme.space8
                                    color: Theme.textSecondary
                                    elide: Text.ElideRight
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    text: qsTr("Mode")
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Skin.ComboBox {
                                    Accessible.name: qsTr("Pad %1 mode").arg(padCard.pad)
                                    Layout.fillWidth: true
                                    currentIndex: padCard.modeIndex
                                    model: root.modeNames
                                    popupMaxItem: 3
                                    textLeftPadding: Theme.space2

                                    ToolTip.delay: 500
                                    ToolTip.text: qsTr("Press behavior for pad %1").arg(padCard.pad)
                                    ToolTip.visible: hovered

                                    onActivated: index => {
                                        padMode.value = index;
                                    }
                                }
                            }

                            // Labelled stem chips. The dot reinforces the label;
                            // it never replaces it.
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.padfxHitHeight
                                spacing: Theme.space1

                                Repeater {
                                    model: 4

                                    delegate: AbstractButton {
                                        id: stemChip

                                        required property int index

                                        readonly property bool on: (padCard.targetMask & (1 << stemChip.index)) !== 0
                                        readonly property color stemColor: root.stemColor(stemChip.index)
                                        readonly property string stemLabelText: root.stemLabel(stemChip.index)

                                        Accessible.checkable: true
                                        Accessible.checked: stemChip.on
                                        Accessible.name: qsTr("Pad %1 target %2").arg(padCard.pad).arg(stemChip.stemLabelText)
                                        Accessible.role: Accessible.CheckBox
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: Theme.padfxHitHeight
                                        hoverEnabled: true
                                        implicitHeight: Theme.padfxHitHeight
                                        padding: 0

                                        ToolTip.delay: 500
                                        ToolTip.text: stemChip.on
                                                       ? qsTr("Remove %1 from pad %2").arg(stemChip.stemLabelText.toLowerCase()).arg(padCard.pad)
                                                       : qsTr("Include %1 in pad %2").arg(stemChip.stemLabelText.toLowerCase()).arg(padCard.pad)
                                        ToolTip.visible: stemChip.hovered

                                        onClicked: root.toggleTarget(padTarget, stemChip.index)

                                        background: Rectangle {
                                            border.color: stemChip.on ? stemChip.stemColor : Theme.border
                                            border.width: 1
                                            color: stemChip.on ? Qt.alpha(stemChip.stemColor, 0.18) : Theme.surface2
                                            radius: Theme.radiusSm

                                            Behavior on color {
                                                ColorAnimation {
                                                    duration: Theme.motionFast
                                                }
                                            }
                                        }
                                        contentItem: RowLayout {
                                            spacing: Theme.space1

                                            Rectangle {
                                                Layout.alignment: Qt.AlignVCenter
                                                Layout.leftMargin: Theme.space1
                                                Layout.preferredHeight: Theme.space2
                                                Layout.preferredWidth: Theme.space2
                                                color: stemChip.on ? stemChip.stemColor : Theme.stemMuted
                                                radius: Theme.radiusPill
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                Layout.rightMargin: Theme.space1
                                                color: stemChip.on ? Theme.textPrimary : Theme.textMuted
                                                elide: Text.ElideRight
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                font.weight: stemChip.on
                                                             ? Theme.fontWeightMedium
                                                             : Theme.fontWeightRegular
                                                text: stemChip.stemLabelText
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                    }
                                }
                            }

                            // All / Instrumental / Vocals shortcuts. Lit only
                            // when the mask equals the preset exactly.
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.padfxHitHeight
                                spacing: Theme.space1

                                Repeater {
                                    model: [
                                        {
                                            "label": qsTr("All"),
                                            "mask": 15,
                                            "hint": qsTr("Set pad %1 to all stems")
                                        },
                                        {
                                            "label": qsTr("Instrumental"),
                                            "mask": 7,
                                            "hint": qsTr("Set pad %1 to drums, bass and other")
                                        },
                                        {
                                            "label": qsTr("Vocals"),
                                            "mask": 8,
                                            "hint": qsTr("Set pad %1 to vocals")
                                        }
                                    ]

                                    delegate: Skin.Button {
                                        id: presetKey

                                        required property var modelData

                                        Accessible.name: presetKey.modelData.hint.arg(padCard.pad)
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: Theme.padfxHitHeight
                                        enabled: true
                                        highlight: padCard.targetMask === presetKey.modelData.mask
                                        implicitHeight: Theme.padfxHitHeight
                                        text: presetKey.modelData.label

                                        ToolTip.delay: 500
                                        ToolTip.text: presetKey.modelData.hint.arg(padCard.pad)
                                        ToolTip.visible: presetKey.hovered

                                        onClicked: padTarget.value = presetKey.modelData.mask
                                    }
                                }
                            }

                            // Effect parameters, only the rows the effect labels.
                            Repeater {
                                model: 5

                                delegate: Skin.PadFxParameter {
                                    required property int index

                                    readonly property string parameterLabel: root.paramLabel(padCard.effectIndex,
                                                                                            index)

                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Theme.padfxControlHeight
                                    group: root.padFxGroup
                                    key: "pad" + padCard.pad + "_param" + (index + 1)
                                    label: parameterLabel
                                    ownerLabel: qsTr("Pad %1").arg(padCard.pad)
                                    visible: padCard.isEditing && parameterLabel.length > 0
                                }
                            }
                        }
                    }
                }
                Item {
                    Layout.fillHeight: root.editingPad > 0
                }
            }
        }

        // ── No-stems empty state ────────────────────────────────────────────
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: Theme.space1
            visible: root.stemCount === 0

            Item {
                Layout.fillHeight: true
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: Text.AlignHCenter
                text: root.emptyTitle
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                horizontalAlignment: Text.AlignHCenter
                text: root.emptyBody
                visible: root.emptyBody.length > 0
                wrapMode: Text.WordWrap
            }
            Skin.Button {
                id: separateKey

                Accessible.name: qsTr("Separate stems for deck %1").arg(root.deckNumber)
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.space1
                enabled: root.canSeparate
                implicitHeight: Theme.padfxControlHeight
                text: qsTr("Separate stems")
                visible: root.currentTrack !== null
                         && Mixxx.StemService.available
                         && Mixxx.StemService.hasModel

                ToolTip.delay: 500
                ToolTip.text: qsTr("Start stem separation for the loaded track")
                ToolTip.visible: separateKey.hovered

                onClicked: Mixxx.StemService.generateStems(root.currentTrack)
            }
            Item {
                Layout.fillHeight: true
            }
        }
    }
}
