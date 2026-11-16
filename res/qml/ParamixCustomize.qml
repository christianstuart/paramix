import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQml
import Mixxx 1.0 as Mixxx
import "." as Skin
import "Theme"

/// Paramix-specific interface customization.
///
/// The classic Mixxx preferences remain the place for audio, library,
/// controllers and analysis. This panel only controls the Paramix skin:
/// colors, typography, motion, layout visibility and waveform presentation.
Popup {
    id: root

    readonly property var fontModel: [
        "Helvetica Neue",
        "Helvetica"
    ]
    readonly property var motionModel: [0.75, 1.0, 1.35]
    readonly property var schemeModel: [
        "Paramix Dark",
        "Paramix Midnight",
        "Paramix High Contrast"
    ]
    readonly property var waveformTypeModel: [
        qsTr("Simple"),
        qsTr("Filtered"),
        qsTr("HSV"),
        qsTr("RGB"),
        qsTr("Stacked")
    ]
    readonly property var waveformTypeValues: [
        Mixxx.WaveformDisplay.Type.Simple,
        Mixxx.WaveformDisplay.Type.Filtered,
        Mixxx.WaveformDisplay.Type.HSV,
        Mixxx.WaveformDisplay.Type.RGB,
        Mixxx.WaveformDisplay.Type.Stacked
    ]

    function resetParamixOptions() {
        Mixxx.Config.paramixColorScheme = "Paramix Dark";
        Mixxx.Config.paramixAccentColor = "";
        Mixxx.Config.paramixFontFamily = "Helvetica Neue";
        Mixxx.Config.paramixFontScale = 1.0;
        Mixxx.Config.paramixMotionScale = 1.0;
        Mixxx.Config.paramixShowWaveforms = true;
        Mixxx.Config.paramixShowCoverArt = true;
        Mixxx.Config.paramixShowStems = true;
        Mixxx.Config.paramixShowHotcues = true;
        Mixxx.Config.paramixShowLibrary = true;
        Mixxx.Config.paramixShowPreview = true;
        Mixxx.Config.paramixShowJogWheels = false;
        Mixxx.Config.paramixCompactDecks = false;
        Mixxx.Config.paramixFourBandEq = false;
        Mixxx.Config.paramixBpmInJog = true;
        Mixxx.Config.waveformStemSplitTracks = false;
        Mixxx.Config.waveformPlayMarkerPosition = 0.5;
    }

    function toHex(c) {
        function channel(value) {
            const text = Math.round(Math.max(0, Math.min(1, value)) * 255).toString(16);
            return text.length < 2 ? "0" + text : text;
        }
        return "#" + channel(c.r) + channel(c.g) + channel(c.b);
    }

    function refresh() {
        schemeInput.currentIndex = Math.max(0, root.schemeModel.indexOf(Mixxx.Config.paramixColorScheme));
        fontInput.currentIndex = Math.max(0, root.fontModel.indexOf(Mixxx.Config.paramixFontFamily));
        motionInput.currentIndex = Mixxx.Config.paramixMotionScale < 0.9
                ? 0
                : Mixxx.Config.paramixMotionScale > 1.15 ? 2 : 1;
        waveformInput.currentIndex = Math.max(0,
                root.waveformTypeValues.indexOf(Mixxx.Config.waveformType));
        stemLanesInput.currentIndex = Mixxx.Config.waveformStemSplitTracks ? 1 : 0;
    }

    closePolicy: Popup.CloseOnEscape
    contentHeight: height
    contentWidth: width
    onOpened: root.refresh()
    height: Math.min(760, parent ? parent.height - 64 : 760)
    modal: true
    padding: 0
    width: Math.min(880, parent ? parent.width - 96 : 880)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    background: Rectangle {
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface1
        radius: Theme.radiusSm
    }
    Overlay.modal: Rectangle {
        color: Theme.scrim
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space5
            Layout.rightMargin: Theme.space5
            Layout.topMargin: Theme.space4
            spacing: Theme.space3

            Rectangle {
                color: Theme.accent
                implicitHeight: 22
                implicitWidth: 3
                radius: Theme.radiusXs
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLg
                    font.weight: Theme.fontWeightDemiBold
                    text: qsTr("Customize Paramix")
                }
                Text {
                    Layout.fillWidth: true
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    text: qsTr("Changes apply immediately. Audio, library and controller settings remain in Preferences.")
                }
            }
            Skin.Button {
                Accessible.name: qsTr("Close customization")
                implicitWidth: 30
                text: qsTr("Close")

                onClicked: root.close()
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space5
            Layout.rightMargin: Theme.space5
            Layout.topMargin: Theme.space3
            color: Theme.border
            implicitHeight: 1
        }

        ScrollView {
            id: scroll

            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                spacing: Theme.space3
                width: scroll.availableWidth

                // ── Appearance ───────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.space5
                    Layout.rightMargin: Theme.space5
                    Layout.topMargin: Theme.space3
                    spacing: Theme.space2

                    Eyebrow {
                        text: qsTr("Appearance")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLabel {
                            text: qsTr("Color scheme")
                        }
                        Skin.ComboBox {
                            id: schemeInput

                            Layout.preferredWidth: 230
                            Accessible.name: qsTr("Color scheme")
                            model: root.schemeModel
                            currentIndex: Math.max(0, root.schemeModel.indexOf(Mixxx.Config.paramixColorScheme))

                            onActivated: index => {
                                Mixxx.Config.paramixColorScheme = root.schemeModel[index];
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLabel {
                            text: qsTr("Accent color")
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1

                            Repeater {
                                model: ["#4F8CF7", "#E85D4A", "#7A5AF8", "#FFB000", "#FF4D8D", "#00C2A8"]

                                Rectangle {
                                    required property string modelData

                                    Accessible.name: qsTr("Accent color %1").arg(modelData)
                                    Accessible.role: Accessible.Button
                                    border.color: Mixxx.Config.paramixAccentColor.toLowerCase() === modelData.toLowerCase()
                                            ? Theme.textPrimary
                                            : Theme.border
                                    border.width: 1
                                    color: modelData
                                    implicitHeight: 22
                                    implicitWidth: 22
                                    radius: Theme.radiusSm

                                    TapHandler {
                                        onTapped: Mixxx.Config.paramixAccentColor = modelData
                                    }
                                }
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Skin.Button {
                                Accessible.name: qsTr("Choose a custom accent color")
                                implicitWidth: 104
                                text: qsTr("Custom…")

                                onClicked: colorPicker.open()
                            }
                            Skin.Button {
                                Accessible.name: qsTr("Reset the accent color")
                                implicitWidth: 90
                                text: qsTr("Default")

                                onClicked: Mixxx.Config.paramixAccentColor = ""
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLabel {
                            text: qsTr("Interface font")
                        }
                        Skin.ComboBox {
                            id: fontInput

                            Layout.preferredWidth: 230
                            Accessible.name: qsTr("Interface font")
                            model: root.fontModel
                            currentIndex: Math.max(0, root.fontModel.indexOf(Mixxx.Config.paramixFontFamily))

                            onActivated: index => {
                                Mixxx.Config.paramixFontFamily = root.fontModel[index];
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLabel {
                            text: qsTr("Font size")
                        }
                        ParamixSlider {
                            id: fontScaleSlider

                            Layout.preferredWidth: 230
                            Accessible.name: qsTr("Font size")
                            from: 0.85
                            stepSize: 0.05
                            to: 1.35

                            Binding {
                                target: fontScaleSlider
                                property: "value"
                                value: Mixxx.Config.paramixFontScale
                                when: !fontScaleSlider.pressed
                            }
                            onMoved: Mixxx.Config.paramixFontScale = value
                        }
                        Text {
                            Layout.preferredWidth: 52
                            color: Theme.textSecondary
                            font.family: Theme.fontFamilyMono
                            font.pixelSize: Theme.fontSizeSm
                            text: Math.round(fontScaleSlider.value * 100) + "%"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLabel {
                            text: qsTr("Animation speed")
                        }
                        Skin.ComboBox {
                            id: motionInput

                            Layout.preferredWidth: 230
                            Accessible.name: qsTr("Animation speed")
                            model: [qsTr("Snappy"), qsTr("Balanced"), qsTr("Relaxed")]
                            currentIndex: Mixxx.Config.paramixMotionScale < 0.9
                                    ? 0
                                    : Mixxx.Config.paramixMotionScale > 1.15 ? 2 : 1

                            onActivated: index => {
                                Mixxx.Config.paramixMotionScale = root.motionModel[index];
                            }
                        }
                    }
                }
                Hairline {
                }

                // ── Layout ────────────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.space5
                    Layout.rightMargin: Theme.space5
                    spacing: Theme.space1

                    Eyebrow {
                        text: qsTr("Layout")
                    }
                    ToggleRow {
                        label: qsTr("Waveforms")
                        hint: qsTr("Show the main waveform stack above the decks.")
                        value: Mixxx.Config.paramixShowWaveforms

                        onChanged: Mixxx.Config.paramixShowWaveforms = !value
                    }
                    ToggleRow {
                        label: qsTr("Cover art")
                        hint: qsTr("Show cover art in the deck header.")
                        value: Mixxx.Config.paramixShowCoverArt

                        onChanged: Mixxx.Config.paramixShowCoverArt = !value
                    }
                    ToggleRow {
                        label: qsTr("Stem strip")
                        hint: qsTr("Show the four-lane stem mixer tab on each deck.")
                        value: Mixxx.Config.paramixShowStems

                        onChanged: Mixxx.Config.paramixShowStems = !value
                    }
                    ToggleRow {
                        label: qsTr("Hotcues")
                        hint: qsTr("Show the hotcue grid.")
                        value: Mixxx.Config.paramixShowHotcues

                        onChanged: Mixxx.Config.paramixShowHotcues = !value
                    }
                    ToggleRow {
                        label: qsTr("Library")
                        hint: qsTr("Show the library pane.")
                        value: Mixxx.Config.paramixShowLibrary

                        onChanged: Mixxx.Config.paramixShowLibrary = !value
                    }
                    ToggleRow {
                        label: qsTr("Preview deck")
                        hint: qsTr("Show the preview deck below the library.")
                        value: Mixxx.Config.paramixShowPreview

                        onChanged: Mixxx.Config.paramixShowPreview = !value
                    }
                    ToggleRow {
                        label: qsTr("Jog wheels")
                        hint: qsTr("Show a jog wheel on each deck. On narrow decks it replaces the tempo fader.")
                        value: Mixxx.Config.paramixShowJogWheels

                        onChanged: Mixxx.Config.paramixShowJogWheels = !value
                    }
                    ToggleRow {
                        label: qsTr("Compact decks")
                        hint: qsTr("Tighter deck header and margins for smaller screens.")
                        value: Mixxx.Config.paramixCompactDecks

                        onChanged: Mixxx.Config.paramixCompactDecks = !value
                    }
                    ToggleRow {
                        label: qsTr("BPM in jog centre")
                        hint: qsTr("Show the BPM and pitch in the centre of the jog wheel. Turn off to show them above the tempo fader.")
                        value: Mixxx.Config.paramixBpmInJog

                        onChanged: Mixxx.Config.paramixBpmInJog = !value
                    }
                    ToggleRow {
                        label: qsTr("Mid-high EQ band")
                        hint: qsTr("Add a fourth EQ knob between HIGH and MID. Requires the BQ EQ/ISO equalizer in Preferences -> Mixer.")
                        value: Mixxx.Config.paramixFourBandEq

                        onChanged: Mixxx.Config.paramixFourBandEq = !value
                    }
                }
                Hairline {
                }

                // ── Waveform ──────────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.space5
                    Layout.rightMargin: Theme.space5
                    spacing: Theme.space2

                    Eyebrow {
                        text: qsTr("Waveform")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLabel {
                            text: qsTr("Playhead position")
                        }
                        ParamixSlider {
                            id: playheadSlider

                            Layout.preferredWidth: 230
                            Accessible.name: qsTr("Playhead position")
                            from: 0
                            stepSize: 0.01
                            to: 1

                            Binding {
                                target: playheadSlider
                                property: "value"
                                value: Mixxx.Config.waveformPlayMarkerPosition
                                when: !playheadSlider.pressed
                            }
                            onMoved: Mixxx.Config.waveformPlayMarkerPosition = value
                        }
                        Text {
                            Layout.preferredWidth: 52
                            color: Theme.textSecondary
                            font.family: Theme.fontFamilyMono
                            font.pixelSize: Theme.fontSizeSm
                            text: Math.round(playheadSlider.value * 100) + "%"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLabel {
                            text: qsTr("Waveform style")
                        }
                        Skin.ComboBox {
                            id: waveformInput

                            Layout.preferredWidth: 230
                            Accessible.name: qsTr("Waveform style")
                            model: root.waveformTypeModel
                            currentIndex: Math.max(0, root.waveformTypeValues.indexOf(Mixxx.Config.waveformType))

                            onActivated: index => {
                                Mixxx.Config.waveformType = root.waveformTypeValues[index];
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        RowLabel {
                            text: qsTr("Stem lanes")
                        }
                        Skin.ComboBox {
                            id: stemLanesInput

                            Layout.preferredWidth: 230
                            Accessible.name: qsTr("Stem lane layout")
                            model: [qsTr("Overlap"), qsTr("Stacked")]
                            currentIndex: Mixxx.Config.waveformStemSplitTracks ? 1 : 0

                            onActivated: index => {
                                Mixxx.Config.waveformStemSplitTracks = index === 1;
                            }
                        }
                    }
                }
                Hairline {
                }

                // ── Interface ─────────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.space5
                    Layout.rightMargin: Theme.space5
                    Layout.bottomMargin: Theme.space4
                    spacing: Theme.space2

                    Eyebrow {
                        text: qsTr("Interface")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space3

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            RowLabel {
                                text: qsTr("Classic Mixxx interface")
                            }
                            Hint {
                                text: qsTr("Restart into the original widget interface. Your library and settings are kept; the Paramix customization is preserved for the next QML launch.")
                            }
                        }
                        Skin.Button {
                            Accessible.name: qsTr("Restart in the classic Mixxx interface")
                            implicitWidth: 190
                            text: qsTr("Switch and restart")

                            onClicked: Mixxx.Application.restartApplication(false)
                        }
                    }
                }
            }
        }

        // Footer
        Rectangle {
            Layout.fillWidth: true
            color: Theme.border
            implicitHeight: 1
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space5
            Layout.rightMargin: Theme.space5
            Layout.bottomMargin: Theme.space4
            Layout.topMargin: Theme.space3
            spacing: Theme.space2

            Skin.Button {
                Accessible.name: qsTr("Reset Paramix appearance options")
                implicitWidth: 150
                text: qsTr("Reset appearance")

                onClicked: root.resetParamixOptions()
            }
            Item {
                Layout.fillWidth: true
            }
            Skin.Button {
                Accessible.name: qsTr("Close customization")
                implicitWidth: 90
                text: qsTr("Done")

                onClicked: root.close()
            }
        }
    }

    ColorDialog {
        id: colorPicker

        title: qsTr("Accent color")

        onAccepted: Mixxx.Config.paramixAccentColor = root.toHex(selectedColor)
    }

    component Eyebrow: Text {
        Layout.fillWidth: true
        color: Theme.textMuted
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
    }
    component Hairline: Rectangle {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.space5
        Layout.rightMargin: Theme.space5
        color: Theme.border
        implicitHeight: 1
    }
    component Hint: Text {
        Layout.fillWidth: true
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
        wrapMode: Text.WordWrap
    }
    component ParamixSlider: Slider {
        id: control

        background: Rectangle {
            color: Theme.surface3
            height: 2
            radius: 1
            width: control.availableWidth
            x: control.leftPadding
            y: control.topPadding + control.availableHeight / 2 - height / 2

            Rectangle {
                color: Theme.accent
                height: parent.height
                radius: parent.radius
                width: control.visualPosition * parent.width
            }
        }
        handle: Rectangle {
            border.color: Theme.borderStrong
            border.width: 1
            color: Theme.textPrimary
            height: 12
            radius: height / 2
            width: height
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: control.topPadding + control.availableHeight / 2 - height / 2
        }
    }
    component RowLabel: Text {
        Layout.fillWidth: true
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeMd
        verticalAlignment: Text.AlignVCenter
    }
    component ToggleRow: RowLayout {
        id: row

        property string hint
        property string label
        property bool value

        signal changed

        Layout.fillWidth: true
        spacing: Theme.space3

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLabel {
                text: row.label
            }
            Hint {
                text: row.hint
                visible: row.hint.length > 0
            }
        }
        ToggleSwitch {
            accessibleName: row.label
            checked: row.value

            onToggled: row.changed()
        }
    }
}
