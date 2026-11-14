pragma Singleton

import QtQuick 2.12

/// Paramix design tokens.
///
/// This singleton is the single source of truth for color, type, spacing,
/// radius, motion and layout metrics in the QML skin. New components should
/// use the semantic names (`surface1`, `textSecondary`, `accent`, ...); the
/// legacy Mixxx names (`darkGray`, `white`, `buttonNormalColor`, ...) are kept
/// as aliases so existing components keep working while they are migrated.
///
/// The palette is selected by `scheme`. The values are plain colors so a skin
/// can override individual tokens at runtime by assigning them.
QtObject {
    id: theme

    // ── Named color schemes ────────────────────────────────────────────────
    // "Paramix Dark" is the default. "Paramix Midnight" is a lower-contrast
    // variant for dim rooms. "Paramix High Contrast" uses the Okabe-Ito
    // color-blind-safe palette for the stem colors.
    property string scheme: "Paramix Dark"

    readonly property var palette: {
        switch (scheme) {
        case "Paramix Midnight":
            return {
                "surface0": "#07080B",
                "surface1": "#0D1016",
                "surface2": "#141924",
                "surface3": "#1E2635",
                "border": "#232C3B",
                "borderStrong": "#33405A",
                "textPrimary": "#EAF0FA",
                "textSecondary": "#A8B4C9",
                "textMuted": "#6E7A91",
                "accent": "#5B9DFF",
                "accentHover": "#7FB4FF",
                "accentPressed": "#3B7BE0",
                "onAccent": "#06101F",
                "positive": "#3DDC97",
                "warning": "#FFC857",
                "danger": "#FF5C6C",
                "focusRing": "#8FC0FF",
                "stemDrums": "#E85D4A",
                "stemBass": "#7A5AF8",
                "stemVocals": "#2BB673",
                "stemOther": "#F2C94C"
            };
        case "Paramix High Contrast":
            // Okabe-Ito color-blind-safe palette.
            return {
                "surface0": "#000000",
                "surface1": "#101010",
                "surface2": "#1C1C1C",
                "surface3": "#2A2A2A",
                "border": "#3A3A3A",
                "borderStrong": "#5A5A5A",
                "textPrimary": "#FFFFFF",
                "textSecondary": "#D0D0D0",
                "textMuted": "#9A9A9A",
                "accent": "#FFB000",
                "accentHover": "#FFC64D",
                "accentPressed": "#E09B00",
                "onAccent": "#000000",
                "positive": "#00C2A8",
                "warning": "#FFD166",
                "danger": "#FF4D4D",
                "focusRing": "#FFD166",
                "stemDrums": "#E69F00",
                "stemBass": "#56B4E9",
                "stemVocals": "#009E73",
                "stemOther": "#CC79A7"
            };
        default:
            return {
                "surface0": "#0E1014",
                "surface1": "#151922",
                "surface2": "#1D2330",
                "surface3": "#273040",
                "border": "#2A3344",
                "borderStrong": "#3A465C",
                "textPrimary": "#F3F6FC",
                "textSecondary": "#B7C0D4",
                "textMuted": "#7C879E",
                "accent": "#4C8DFF",
                "accentHover": "#6BA2FF",
                "accentPressed": "#2F6FE0",
                "onAccent": "#0B1220",
                "positive": "#3DDC97",
                "warning": "#FFC857",
                "danger": "#FF5C6C",
                "focusRing": "#7FB2FF",
                "stemDrums": "#E85D4A",
                "stemBass": "#7A5AF8",
                "stemVocals": "#2BB673",
                "stemOther": "#F2C94C"
            };
        }
    }

    // ── Surfaces ───────────────────────────────────────────────────────────
    readonly property color surface0: palette.surface0
    readonly property color surface1: palette.surface1
    readonly property color surface2: palette.surface2
    readonly property color surface3: palette.surface3
    readonly property color surfaceRaised: palette.surface3
    readonly property color border: palette.border
    readonly property color borderStrong: palette.borderStrong
    readonly property color scrim: Qt.rgba(0, 0, 0, 0.55)

    // ── Text ───────────────────────────────────────────────────────────────
    readonly property color textPrimary: palette.textPrimary
    readonly property color textSecondary: palette.textSecondary
    readonly property color textMuted: palette.textMuted
    readonly property color onAccent: palette.onAccent

    // ── Accents and states ─────────────────────────────────────────────────
    readonly property color accent: palette.accent
    readonly property color accentHover: palette.accentHover
    readonly property color accentPressed: palette.accentPressed
    readonly property color positive: palette.positive
    readonly property color warning: palette.warning
    readonly property color danger: palette.danger
    readonly property color focusRing: palette.focusRing

    // ── Stem palette ───────────────────────────────────────────────────────
    // Never encode stem identity by color alone; pair it with a label.
    readonly property color stemDrums: palette.stemDrums
    readonly property color stemBass: palette.stemBass
    readonly property color stemVocals: palette.stemVocals
    readonly property color stemOther: palette.stemOther
    readonly property color stemMuted: Qt.rgba(1, 1, 1, 0.35)
    readonly property color stemSolo: palette.accent

    // ── Phrase palette (Rekordbox-style phrase labels) ─────────────────────
    readonly property color phraseIntro: "#5B8DEF"
    readonly property color phraseUp: "#3DDC97"
    readonly property color phraseDown: "#FF7A59"
    readonly property color phraseVerse: "#B07CFF"
    readonly property color phraseBridge: "#F2C94C"
    readonly property color phraseChorus: "#FF4D8D"
    readonly property color phraseOutro: "#7C879E"

    // ── Typography ─────────────────────────────────────────────────────────
    readonly property string fontFamily: "Open Sans"
    readonly property string fontFamilyDisplay: "Open Sans"
    readonly property string fontFamilyMono: "monospace"
    readonly property int fontSizeXs: 10
    readonly property int fontSizeSm: 11
    readonly property int fontSizeMd: 13
    readonly property int fontSizeLg: 16
    readonly property int fontSizeXl: 22
    readonly property int fontSizeXxl: 28
    readonly property int fontWeightRegular: Font.Normal
    readonly property int fontWeightMedium: Font.Medium
    readonly property int fontWeightDemiBold: Font.DemiBold
    readonly property int fontWeightBold: Font.Bold
    readonly property real letterSpacingCaps: 0.8
    readonly property real lineHeight: 1.25

    // ── Spacing (4 px base grid) ───────────────────────────────────────────
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 24
    readonly property int space6: 32
    readonly property int space7: 48
    readonly property int space8: 64

    // ── Radius ─────────────────────────────────────────────────────────────
    readonly property int radiusXs: 2
    readonly property int radiusSm: 4
    readonly property int radiusMd: 6
    readonly property int radiusLg: 10
    readonly property int radiusXl: 16
    readonly property int radiusPill: 999

    // ── Motion ─────────────────────────────────────────────────────────────
    readonly property int motionFast: 90
    readonly property int motionNormal: 160
    readonly property int motionSlow: 280
    readonly property int motionSpringDuration: 500

    // ── Layout ─────────────────────────────────────────────────────────────
    readonly property int toolbarHeight: 44
    readonly property int deckHeaderHeight: 56
    readonly property int stemLaneMinWidth: 54
    readonly property int stemLaneHeight: 120
    readonly property int minWindowWidth: 1280
    readonly property int minWindowHeight: 720

    // ── Legacy Mixxx aliases ───────────────────────────────────────────────
    // Keep these until every component uses the semantic names above.
    readonly property color accentColor: accent
    readonly property color backgroundColor: surface0
    readonly property color blue: accent
    readonly property color bpmSliderBarColor: accent
    readonly property color buttonActiveColor: textPrimary
    readonly property color buttonDisableColor: textMuted
    readonly property int buttonFontPixelSize: fontSizeSm
    readonly property color buttonNormalColor: textSecondary
    readonly property color crossfaderBarColor: accent
    readonly property color crossfaderOrientationColor: textMuted
    readonly property color darkGray: surface0
    readonly property color darkGray2: surface1
    readonly property color darkGray3: surface2
    readonly property color darkGray4: surface0
    readonly property color deckActiveColor: accent
    readonly property color deckBackgroundColor: surface0
    readonly property color deckBeatjumpBackgroundColor: surface2
    readonly property color deckBeatjumpLabelColor: textSecondary
    readonly property color deckEmptyCoverArt: surface3
    readonly property color deckInfoBarBackgroundColor: surface1
    readonly property color deckLineColor: border
    readonly property color deckLoopBackgroundColor: surface2
    readonly property color deckLoopLabelColor: textSecondary
    readonly property color deckTextColor: textPrimary
    readonly property color effectColor: warning
    readonly property color effectUnitColor: danger
    readonly property color embeddedBackgroundColor: surface1
    readonly property color eqFxColor: warning
    readonly property color eqHighColor: textPrimary
    readonly property color eqLowColor: textPrimary
    readonly property color eqMidColor: textPrimary
    readonly property color gainKnobColor: accent
    readonly property color green: positive
    readonly property color knobBackgroundColor: surface2
    readonly property color libraryPanelSplitterBackground: surface0
    readonly property color libraryPanelSplitterHandle: borderStrong
    readonly property color libraryPanelSplitterHandleActive: accent
    readonly property color lightGray: textMuted
    readonly property color lightGray2: textSecondary
    readonly property color lightGray3: textMuted
    readonly property color midGray: borderStrong
    readonly property color midGray2: border
    readonly property color midGray3: surface3
    readonly property color panelSplitterBackground: surface0
    readonly property color panelSplitterHandle: border
    readonly property color panelSplitterHandleActive: accent
    readonly property color pflActiveButtonColor: accent
    readonly property color red: danger
    readonly property color samplerColor: accent
    readonly property color sunkenBackgroundColor: surface0
    readonly property color textColor: textPrimary
    readonly property int textFontPixelSize: fontSizeMd
    readonly property color toolbarActiveColor: accent
    readonly property color toolbarBackgroundColor: surface1
    readonly property color volumeSliderBarColor: accent
    readonly property color warningColor: danger
    readonly property color waveformBeatColor: textMuted
    readonly property color waveformCursorColor: textPrimary
    readonly property color waveformMarkerDefault: "#ff7a01"
    readonly property color waveformMarkerIntroOutroColor: "#2c5c9a"
    readonly property color waveformMarkerLabel: Qt.rgba(255, 255, 255, 0.8)
    readonly property color waveformMarkerLoopColor: positive
    readonly property color waveformMarkerLoopColorDisabled: textPrimary
    readonly property color waveformPostrollColor: textMuted
    readonly property color waveformPrerollColor: textMuted
    readonly property color white: textPrimary
    readonly property color yellow: warning

    // ── Legacy image assets ────────────────────────────────────────────────
    // New components draw flat surfaces instead of these, but the images stay
    // available until the last legacy control is migrated.
    readonly property string imgBpmSliderBackground: "images/slider_bpm.svg"
    readonly property string imgButton: "images/button.svg"
    readonly property string imgButtonPressed: "images/button_pressed.svg"
    readonly property string imgCrossfaderBackground: "images/slider_crossfader.svg"
    readonly property string imgCrossfaderHandle: "images/slider_handle_crossfader.svg"
    readonly property string imgKnob: "images/knob.svg"
    readonly property string imgKnobMini: "images/miniknob.svg"
    readonly property string imgKnobMiniShadow: "images/miniknob_shadow.svg"
    readonly property string imgKnobShadow: "images/knob_shadow.svg"
    readonly property string imgMicDuckingSlider: "images/slider_micducking.svg"
    readonly property string imgMicDuckingSliderHandle: "images/slider_handle_micducking.svg"
    readonly property string imgPopupBackground: imgButton
    readonly property string imgSectionBackground: "images/section.svg"
    readonly property string imgSliderHandle: "images/slider_handle.svg"
    readonly property string imgVolumeSliderBackground: "images/slider_volume.svg"
}
