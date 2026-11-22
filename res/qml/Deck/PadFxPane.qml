import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "../Theme"

/// Deck pane tab: the active Pad FX bank as a 2 x 2 pad grid, with the A-D bank
/// keys on the right.
///
/// A press holds padN for as long as the pad is held; a right click opens the
/// shared PadFxPadPopup for everything the Pad FX window's edit card shows, so
/// the band never grows past four pads.
Rectangle {
    id: root

    required property string group

    readonly property string padFxGroup: root.group.substr(0, root.group.length - 1) + "_PadFX]"
    readonly property int deckNumber: Number(root.group.replace(/[^0-9]/g, ""))
    readonly property int stemCount: Math.max(0, Math.round(stemCountControl.value))
    readonly property var effectNames: [qsTr("Off"), qsTr("Vinyl Brake"), qsTr("Echo Out")]
    readonly property var modeNames: [qsTr("Momentary"), qsTr("Toggle"), qsTr("One Shot")]
    readonly property var modeGlyphs: ["H", "L", "1×"]
    readonly property var stemNames: [qsTr("Drums"), qsTr("Bass"), qsTr("Other"), qsTr("Vocals")]

    /// Pad face metrics. Every width is measured so the wrapped effect name can
    /// never be cut off: the corner marks reserve their own width at each side
    /// and the word itself always gets its full advance width.
    readonly property int padPadding: Theme.space1
    readonly property int padMaxLines: 2
    readonly property int muteOutlineWidth: 2

    FontMetrics {
        id: padFont

        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
    }

    function widestAdvance(texts) {
        let width = 0;
        for (let i = 0; i < texts.length; ++i)
            width = Math.max(width, padFont.advanceWidth(texts[i]));
        return Math.ceil(width);
    }

    /// Longest single word any effect name wraps to ("Brake" / "Vinyl").
    readonly property int padWordWidth: {
        let width = 0;
        for (let i = 0; i < root.effectNames.length; ++i) {
            const words = root.effectNames[i].split(" ");
            for (let w = 0; w < words.length; ++w)
                width = Math.max(width, Math.ceil(padFont.advanceWidth(words[w])));
        }
        return width;
    }
    readonly property int padNumberWidth: root.widestAdvance(["1", "2", "3", "4"])
    readonly property int padModeWidth: root.widestAdvance(root.modeGlyphs)
    readonly property int padTextLeftInset: root.padPadding + root.padNumberWidth
    readonly property int padTextRightInset: root.padPadding + root.padModeWidth
    /// Narrowest cell that fits the longest word beside both corner marks.
    readonly property int padMinWidth: root.padWordWidth + root.padTextLeftInset
            + root.padTextRightInset
    /// Same with the pad number dropped, the way a narrow cell degrades first.
    readonly property int padMinWidthNoNumber: root.padWordWidth + 2 * root.padPadding
            + root.padModeWidth
    /// Two wrapped lines plus their padding, never below the hit target.
    readonly property int padMinHeight: Math.max(Theme.padfxHitHeight,
            root.padMaxLines * Math.ceil(padFont.height) + 2 * root.padPadding)

    function effectName(effect) {
        return root.effectNames[Math.max(0, Math.min(2, Math.round(effect)))];
    }
    function modeName(mode) {
        return root.modeNames[Math.max(0, Math.min(2, Math.round(mode)))];
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
                parts.push(root.stemNames[bit]);
        }
        return parts.join(qsTr(", "));
    }
    function openPadEditor(pad, anchor) {
        padPopup.openFor(anchor, pad);
    }

    border.color: Theme.border
    border.width: 1
    clip: true
    color: Theme.surface1
    radius: Theme.radiusMd

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

    RowLayout {
        anchors.bottomMargin: Theme.space2
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        anchors.topMargin: Theme.space2
        spacing: Theme.space1

        // ── Pads (2 x 2, pad 1 top-left) ────────────────────────────────────
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            GridLayout {
                id: padGrid

                anchors.fill: parent
                columnSpacing: Theme.space1
                columns: 2
                rowSpacing: Theme.space1
                visible: root.stemCount > 0

                Repeater {
                    model: 4

                    delegate: Skin.Button {
                        id: padButton

                        required property int index

                        readonly property int pad: index + 1
                        readonly property int effectIndex: Math.round(padEffect.value)
                        readonly property int modeIndex: Math.max(0, Math.min(2, Math.round(padMode.value)))
                        readonly property int targetMask: Math.round(padTarget.value)
                        readonly property bool isActive: padActive.value > 0
                        readonly property bool isMuted: padMuted.value > 0
                        readonly property string effectText: root.effectName(padButton.effectIndex)
                        readonly property string descriptionText: qsTr("Pad %1: %2, %3, %4")
                            .arg(padButton.pad)
                            .arg(padButton.effectText)
                            .arg(root.modeName(padButton.modeIndex))
                            .arg(root.targetWords(padButton.targetMask))
                        /// Corner marks are dropped, the pad number first, when
                        /// the cell is too narrow to clear them; the centred
                        /// text then keeps only the insets that are left.
                        readonly property bool showNumber: padButton.width >= root.padMinWidth
                        readonly property bool showMode: padButton.width >= root.padMinWidthNoNumber
                        readonly property int textLeftInset: root.padPadding
                                + (padButton.showNumber ? root.padNumberWidth : 0)
                        readonly property int textRightInset: root.padPadding
                                + (padButton.showMode ? root.padModeWidth : 0)

                        Accessible.description: {
                            let parts = [padButton.descriptionText];
                            if (padButton.isActive)
                                parts.push(qsTr("Active"));
                            if (padButton.isMuted)
                                parts.push(qsTr("Muting"));
                            parts.push(qsTr("Right click edits the pad"));
                            return parts.join(". ");
                        }
                        Accessible.name: padButton.descriptionText
                        // The GridLayout gives all four delegates the same cell,
                        // because they share this measured implicit size.
                        implicitHeight: root.padMinHeight
                        implicitWidth: root.padMinWidth
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        // The cell may be narrower than the preferred size at
                        // a larger ui scale; let it shrink and drop the corner
                        // marks instead of overflowing into the bank keys.
                        Layout.minimumHeight: 0
                        Layout.minimumWidth: 0
                        highlight: padButton.isActive
                        opacity: padButton.effectIndex === 0 ? 0.55 : 1.0

                        ToolTip.delay: 500
                        ToolTip.text: {
                            const text = padButton.effectIndex === 0
                                    ? qsTr("Pad %1 has no effect configured. Right click to edit the pad.").arg(padButton.pad)
                                    : qsTr("Pad %1 - %2 (%3, %4). Right click to edit the pad.")
                                      .arg(padButton.pad)
                                      .arg(padButton.effectText)
                                      .arg(root.modeName(padButton.modeIndex))
                                      .arg(root.targetWords(padButton.targetMask));
                            return padButton.isMuted
                                    ? text + " " + qsTr("Muting: press the pad to release.")
                                    : text;
                        }
                        ToolTip.visible: padButton.hovered

                        onPressedChanged: padControl.value = padButton.pressed ? 1 : 0

                        background: Rectangle {
                            // Muting is an outline, not a text badge: the cell
                            // has no room for one.
                            border.color: padButton.isMuted
                                          ? Theme.warning
                                          : padButton.highlight
                                            ? Qt.lighter(Theme.accent, 1.12)
                                            : Theme.border
                            border.width: padButton.isMuted ? root.muteOutlineWidth : 1
                            color: padButton.highlight ? Theme.accent : Theme.surface3
                            radius: Theme.radiusSm

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.motionFast
                                }
                            }
                        }
                        contentItem: Item {
                            // Wrapped effect name, centred. Its insets already
                            // clear the corner marks, so no line can collide.
                            Text {
                                id: padEffectName

                                anchors.bottomMargin: root.padPadding
                                anchors.fill: parent
                                anchors.leftMargin: padButton.textLeftInset
                                anchors.rightMargin: padButton.textRightInset
                                anchors.topMargin: root.padPadding
                                color: padButton.highlight
                                       ? Theme.textOnAccent
                                       : padButton.effectIndex === 0
                                         ? Theme.textMuted
                                         : Theme.textPrimary
                                elide: Text.ElideNone
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Theme.fontWeightDemiBold
                                horizontalAlignment: Text.AlignHCenter
                                maximumLineCount: root.padMaxLines
                                text: padButton.effectText
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.Wrap
                            }
                            // Pad number, one digit in the top-left corner.
                            Text {
                                id: padNumberText

                                anchors.left: parent.left
                                anchors.leftMargin: root.padPadding
                                anchors.top: parent.top
                                anchors.topMargin: root.padPadding
                                color: padButton.highlight ? Theme.textOnAccent : Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Theme.fontWeightDemiBold
                                text: padButton.pad
                                visible: padButton.showNumber
                            }
                            // Mode glyph (H / L / 1x), plain text in the
                            // top-right corner; the sentence is in the tooltip.
                            Text {
                                id: padModeText

                                anchors.right: parent.right
                                anchors.rightMargin: root.padPadding
                                anchors.top: parent.top
                                anchors.topMargin: root.padPadding
                                color: padButton.highlight ? Theme.textOnAccent : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Theme.fontWeightDemiBold
                                text: root.modeGlyphs[padButton.modeIndex]
                                visible: padButton.showMode

                                HoverHandler {
                                    id: padModeHover
                                }
                                ToolTip.delay: 500
                                ToolTip.text: qsTr("Mode: %1").arg(root.modeName(padButton.modeIndex))
                                ToolTip.visible: padModeHover.hovered
                            }
                        }
                        // Right click edits; it never presses the pad, because
                        // this area only accepts the right button.
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton

                            onPressed: root.openPadEditor(padButton.pad, padButton)
                        }

                        Mixxx.ControlProxy {
                            id: padControl

                            group: root.padFxGroup
                            key: "pad" + padButton.pad
                        }
                        Mixxx.ControlProxy {
                            id: padActive

                            group: root.padFxGroup
                            key: "pad" + padButton.pad + "_active"
                        }
                        Mixxx.ControlProxy {
                            id: padMuted

                            group: root.padFxGroup
                            key: "pad" + padButton.pad + "_muted"
                        }
                        Mixxx.ControlProxy {
                            id: padEffect

                            group: root.padFxGroup
                            key: "effect" + padButton.pad
                        }
                        Mixxx.ControlProxy {
                            id: padMode

                            group: root.padFxGroup
                            key: "mode" + padButton.pad
                        }
                        Mixxx.ControlProxy {
                            id: padTarget

                            group: root.padFxGroup
                            key: "target" + padButton.pad
                        }
                    }
                }
            }

            // ── No-stems empty state, centred where the pads would be ───────
            Text {
                anchors.centerIn: parent
                color: Theme.textMuted
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Pad FX need a track with stems")
                visible: root.stemCount === 0
                width: parent.width - 2 * Theme.space1
                wrapMode: Text.WordWrap
            }
        }

        // ── Bank keys ───────────────────────────────────────────────────────
        GridLayout {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: 2 * Theme.padfxHitHeight + Theme.space1
            Layout.preferredWidth: 2 * Theme.padfxHitHeight + Theme.space1
            columnSpacing: Theme.space1
            columns: 2
            enabled: root.stemCount > 0
            rowSpacing: Theme.space1

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
                    Layout.preferredHeight: Theme.padfxHitHeight
                    Layout.preferredWidth: Theme.padfxHitHeight
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
    }

    Skin.PadFxPadPopup {
        id: padPopup

        deckNumber: root.deckNumber
        padFxGroup: root.padFxGroup
    }
}
