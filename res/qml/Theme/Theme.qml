pragma Singleton

import QtQuick 2.12

/// Paramix design tokens.
///
/// This singleton is the single source of truth for color, type, spacing,
/// radius, motion and layout metrics in the QML skin. The semantic names
/// (`surface1`, `textSecondary`, `accent`, ...) are preferred; the legacy
/// Mixxx names (`darkGray`, `white`, `buttonNormalColor`, ...) are aliases kept
/// so un-migrated components keep working.
///
/// The values are injected from `Mixxx.Config` by `main.qml`, which keeps this
/// singleton loadable by the QML tests without the C++ module. A skin or
/// Preferences can switch the scheme, accent, font and motion scale at runtime.
QtObject {
    id: theme

    // ── Runtime overrides (bound in main.qml) ──────────────────────────────
    property string scheme: "Paramix Dark"
    property string accentOverride: ""
    property string fontFamilyOverride: ""
    property real fontScale: 1.0
    /// Scales the spacing grid and the fixed layout metrics (bar heights, lane
    /// widths) without touching type. Text stays on `fontScale`, so a compact
    /// display is handled by lowering both independently.
    property real uiScale: 1.0
    property real motionScale: 1.0

    // ── Window size and breakpoints (bound in main.qml) ────────────────────
    /// Window size in logical pixels, so the breakpoints below are a single
    /// source of truth instead of each component inventing its own threshold.
    property int windowWidth: 1792
    property int windowHeight: 1008

    /// The toolbar plus two mixer columns stop fitting side by side.
    /// The waveform strip, deck band and library cannot all keep full height.
    readonly property bool shortWindow: windowHeight < 900

    // ── Contrast helpers ───────────────────────────────────────────────────
    // WCAG 2.1 relative luminance and contrast ratio. Used to pick the label
    // colour that stays legible on a user-chosen accent, instead of guessing
    // from a luminance threshold (which put white on signal blue at 3.9:1).
    function srgbToLinear(channel) {
        return channel <= 0.04045 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4);
    }
    function relativeLuminance(color) {
        return 0.2126 * srgbToLinear(color.r)
                + 0.7152 * srgbToLinear(color.g)
                + 0.0722 * srgbToLinear(color.b);
    }
    function contrastRatio(colorA, colorB) {
        const a = relativeLuminance(colorA);
        const b = relativeLuminance(colorB);
        return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
    }

    // ── Named color schemes ────────────────────────────────────────────────
    // Traktor stage: neutral near-black canvas, flat matte panels with
    // hairline frames. No blue-tinted surfaces. Deck identity comes from
    // the deckA–D tokens, never from tinting whole cards.
    readonly property var palette: {
        switch (scheme) {
        case "Paramix Midnight":
            return {
                "surface0": "#030303",
                "surface1": "#0A0A0A",
                "surface2": "#131313",
                "surface3": "#1B1B1B",
                "surfaceHover": "#222222",
                "border": "#1C1C1C",
                "borderStrong": "#2E2E2E",
                "textPrimary": "#E8E8E8",
                "textSecondary": "#9A9A9A",
                "textMuted": "#8A8A8A",
                "accent": "#2F80ED",
                "accentHover": "#5A9BF2",
                "accentPressed": "#1E63C6",
                "textOnAccent": "#08101C",
                "positive": "#33D17A",
                "warning": "#FFB224",
                "danger": "#FF4D5E",
                "focusRing": "#B5B5B5",
                "stemDrums": "#C13BD6",
                "stemBass": "#F08A24",
                "stemVocals": "#2FBF71",
                "stemOther": "#22C7D6"
            };
        case "Paramix High Contrast":
            // Okabe-Ito color-blind-safe palette.
            return {
                "surface0": "#000000",
                "surface1": "#0E0E0E",
                "surface2": "#1B1B1B",
                "surface3": "#2A2A2A",
                "surfaceHover": "#333333",
                "border": "#3A3A3A",
                "borderStrong": "#5C5C5C",
                "textPrimary": "#FFFFFF",
                "textSecondary": "#D4D4D4",
                "textMuted": "#A6A6A6",
                "accent": "#FFB000",
                "accentHover": "#FFC64D",
                "accentPressed": "#D99400",
                "textOnAccent": "#000000",
                "positive": "#00C2A8",
                "warning": "#FFD166",
                "danger": "#FF5252",
                "focusRing": "#FFD166",
                "stemDrums": "#C13BD6",
                "stemBass": "#F08A24",
                "stemVocals": "#2FBF71",
                "stemOther": "#22C7D6"
            };
        default:
            // Paramix Dark: Traktor near-black stage. Neutral matte
            // surfaces, restrained signal-blue accent, Pioneer-style
            // channel coding, warm performance-pad hues.
            return {
                "surface0": "#060606",
                "surface1": "#0D0D0D",
                "surface2": "#161616",
                "surface3": "#1E1E1E",
                "surfaceHover": "#242424",
                "border": "#1F1F1F",
                "borderStrong": "#303030",
                "textPrimary": "#EAEAEA",
                "textSecondary": "#A3A3A3",
                "textMuted": "#8C8C8C",
                "accent": "#2F80ED",
                "accentHover": "#5A9BF2",
                "accentPressed": "#1E63C6",
                "textOnAccent": "#08101C",
                "positive": "#33D17A",
                "warning": "#FFB224",
                "danger": "#FF4D5E",
                "focusRing": "#B5B5B5",
                "stemDrums": "#C13BD6",
                "stemBass": "#F08A24",
                "stemVocals": "#2FBF71",
                "stemOther": "#22C7D6"
            };
        }
    }

    // ── Surfaces ───────────────────────────────────────────────────────────
    readonly property color surface0: palette.surface0
    readonly property color surface1: palette.surface1
    readonly property color surface2: palette.surface2
    readonly property color surface3: palette.surface3
    readonly property color surfaceRaised: palette.surface3
    readonly property color surfaceHover: palette.surfaceHover
    readonly property color border: palette.border
    readonly property color borderStrong: palette.borderStrong
    readonly property color scrim: Qt.rgba(0, 0, 0, 0.6)
    readonly property color topHighlight: Qt.rgba(255, 255, 255, 0.03)
    readonly property color accentSoft: Qt.alpha(accent, 0.16)
    readonly property color shadow: Qt.rgba(0, 0, 0, 0.55)

    // ── Text ───────────────────────────────────────────────────────────────
    readonly property color textPrimary: palette.textPrimary
    readonly property color textSecondary: palette.textSecondary
    readonly property color textMuted: palette.textMuted

    // ── Accents and states ─────────────────────────────────────────────────
    readonly property bool hasCustomAccent: accentOverride.length > 0
    readonly property color customAccent: hasCustomAccent ? accentOverride : "transparent"
    readonly property color accent: hasCustomAccent ? customAccent : palette.accent
    readonly property color accentHover: hasCustomAccent
            ? Qt.lighter(accent, 1.18)
            : palette.accentHover
    readonly property color accentPressed: hasCustomAccent
            ? Qt.darker(accent, 1.22)
            : palette.accentPressed
    readonly property color textOnAccent: hasCustomAccent
            ? (contrastRatio(customAccent, "#08101C") >= contrastRatio(customAccent, "#FFFFFF")
               ? "#08101C"
               : "#FFFFFF")
            : palette.textOnAccent
    readonly property color focusRing: hasCustomAccent
            ? Qt.lighter(accent, 1.3)
            : palette.focusRing
    readonly property color positive: palette.positive
    readonly property color warning: palette.warning
    readonly property color danger: palette.danger

    // ── Stem palette ───────────────────────────────────────────────────────
    // Never encode stem identity by color alone; pair it with a label.
    readonly property color stemDrums: palette.stemDrums
    readonly property color stemBass: palette.stemBass
    readonly property color stemVocals: palette.stemVocals
    readonly property color stemOther: palette.stemOther
    readonly property color stemMuted: Qt.rgba(1, 1, 1, 0.35)
    readonly property color stemSolo: accent

    // ── Deck identity (Rekordbox/Traktor per-deck coding) ────────────────
    // Thin top strips, badges and active meters — never full panel tints.
    readonly property color deckA: "#00A3FF"
    readonly property color deckB: "#FF8B00"
    readonly property color deckC: "#B16CFF"
    readonly property color deckD: "#00D1A1"

    // ── Performance pads (Rekordbox 8-pad hues) ──────────────────────────
    readonly property var hotcueColors: [
        "#00C2FF", "#FF2E63", "#FFD60A", "#00E676",
        "#7C5CFF", "#FF6D00", "#00E5FF", "#FF4081"
    ]

    // ── Meters ───────────────────────────────────────────────────────────
    readonly property color vuGreen: palette.positive
    readonly property color vuYellow: palette.warning
    readonly property color vuRed: palette.danger
    readonly property color vuRail: "#101010"

    // ── Phrase palette (Rekordbox-style phrase labels) ─────────────────────
    readonly property color phraseIntro: "#5B8DEF"
    readonly property color phraseUp: "#3DDC97"
    readonly property color phraseDown: "#FF7A59"
    readonly property color phraseVerse: "#B07CFF"
    readonly property color phraseBridge: "#F2C94C"
    readonly property color phraseChorus: "#FF4D8D"
    readonly property color phraseOutro: "#7C879E"

    // ── Waveform frequency colors ──────────────────────────────────────────
    // Frequency coding for the non-stem RGB waveform and the overview: low
    // band reads red, mid green, high blue. Fully saturated primaries read
    // harsh on the near-black stage, so each keeps a little of its neighbours.
    // Both renderers normalise by the largest channel, so these are also the
    // exact colours a column that only carries one band reaches.
    readonly property color waveformLowColor: "#FF3B30"
    readonly property color waveformMidColor: "#34D058"
    readonly property color waveformHighColor: "#3B82F6"

    // ── Downbeat lines ─────────────────────────────────────────────────────
    // Bar lines reuse the ordinary beat-grid hue and are distinguished by
    // width (logical pixels, scaled by the device pixel ratio) and a stronger
    // opacity. Kept as tokens so the emphasis can be tuned without touching
    // the renderer.
    readonly property color waveformDownbeatColor: "#A1A1A1"
    readonly property real waveformDownbeatWidth: 3

    // ── Typography ─────────────────────────────────────────────────────────
    // Helvetica Neue throughout, where it exists. It is a macOS system font,
    // so everywhere else Qt used to substitute whatever it liked (Tahoma on
    // Windows) and the fixed-size boxes no longer fitted their labels. Resolve
    // to an installed family with Helvetica metrics instead; Open Sans is
    // bundled in res/fonts, so the list always ends in a match.
    readonly property var fontFallbacks: ["Helvetica Neue", "Helvetica", "Arial",
        "Liberation Sans", "Nimbus Sans", "Open Sans"]
    readonly property string fontFamily: {
        const installed = Qt.fontFamilies();
        const wanted = fontFamilyOverride.length > 0 ? [fontFamilyOverride] : [];
        return wanted.concat(fontFallbacks).find(family => installed.includes(family))
                || "sans-serif";
    }
    readonly property string fontFamilyDisplay: fontFamily
    readonly property string fontFamilyMono: fontFamily
    readonly property int fontSizeXs: Math.round(9 * fontScale)
    readonly property int fontSizeSm: Math.round(10 * fontScale)
    readonly property int fontSizeMd: Math.round(12 * fontScale)
    readonly property int fontSizeLg: Math.round(15 * fontScale)
    readonly property int fontSizeXl: Math.round(20 * fontScale)
    readonly property int fontSizeXxl: Math.round(26 * fontScale)
    readonly property int fontWeightRegular: Font.Normal
    readonly property int fontWeightMedium: Font.Medium
    readonly property int fontWeightDemiBold: Font.DemiBold
    readonly property int fontWeightBold: Font.Bold
    readonly property real letterSpacingCaps: 1.1
    readonly property real lineHeight: 1.15

    // ── Spacing (4 px base grid, scaled by uiScale) ────────────────────────
    readonly property int space1: Math.round(4 * uiScale)
    readonly property int space2: Math.round(8 * uiScale)
    readonly property int space3: Math.round(12 * uiScale)
    readonly property int space4: Math.round(16 * uiScale)
    readonly property int space5: Math.round(24 * uiScale)
    readonly property int space6: Math.round(32 * uiScale)
    readonly property int space7: Math.round(48 * uiScale)
    readonly property int space8: Math.round(64 * uiScale)

    // ── Radius ─────────────────────────────────────────────────────────────
    // Traktor-sharp: hardware panels carry a tight 3 px round, keys 2 px.
    // Only pills stay fully round.
    readonly property int radiusXs: 1
    readonly property int radiusSm: 2
    readonly property int radiusMd: 3
    readonly property int radiusLg: 4
    readonly property int radiusXl: 6
    readonly property int radiusPill: 999

    // ── Motion ─────────────────────────────────────────────────────────────
    // Fast feedback, no overshoot. `motionScale` is user-adjustable in
    // Preferences > Interface (Snappy to Relaxed).
    readonly property int motionFast: Math.round(60 * motionScale)
    readonly property int motionNormal: Math.round(100 * motionScale)
    readonly property int motionSlow: Math.round(160 * motionScale)
    readonly property int motionSpringDuration: Math.round(260 * motionScale)

    // ── Layout ─────────────────────────────────────────────────────────────
    readonly property int toolbarHeight: Math.round(44 * uiScale)

    // ── Top bar effects strip ──────────────────────────────────────────────
    // A single compact row under the main toolbar: the two standard effect
    // chains plus the per-deck quick chains. Control height follows the type
    // scale as well, so a large font cannot clip the selector labels.
    readonly property int fxControlHeight: Math.max(Math.round(22 * uiScale),
                                                    fontSizeSm + 2 * space1)
    readonly property int topBarEffectsHeight: fxControlHeight + 2 * space1
    readonly property int topBarHeight: toolbarHeight + topBarEffectsHeight
    readonly property int fxKnobSize: Math.round(22 * uiScale)
    readonly property int fxOnKeyWidth: Math.max(Math.round(28 * uiScale), fontSizeXs * 3)
    readonly property int fxAssignKeyWidth: Math.max(Math.round(20 * uiScale), fontSizeXs * 2)
    readonly property int fxPowerDotSize: Math.round(12 * uiScale)
    readonly property int fxUnitKeyMinWidth: Math.max(Math.round(38 * uiScale), fontSizeXs * 4)
    readonly property int fxSlotMinWidth: Math.round(64 * uiScale)
    // Beat FX: the strip stepper, the "BEAT" tag and the wider flyout column.
    readonly property int fxBeatStepKeyWidth: Math.max(Math.round(18 * uiScale),
                                                       fontSizeXs * 2 + space1)
    readonly property int fxBeatStepLabelWidth: Math.max(Math.round(30 * uiScale),
                                                         fontSizeSm * 3)
    readonly property int fxBeatKeySize: Math.max(Math.round(22 * uiScale),
                                                  fontSizeXs * 2 + space2)
    readonly property int fxBeatGridKeyWidth: Math.max(Math.round(36 * uiScale),
                                                      fontSizeXs * 3 + 2 * space1)
    readonly property int fxBeatLevelKnobSize: Math.round(44 * uiScale)
    readonly property int fxBeatColumnWidth: Math.round(190 * uiScale)
    readonly property int fxEffectRowHeight: Math.max(Math.round(24 * uiScale),
                                                      fontSizeSm + 2 * space1)
    readonly property int fxEffectPopupMinWidth: Math.round(150 * uiScale)
    // The strip gives the two fixed units its full width, so the super knob no
    // longer needs a strip-wide width gate. TopBarEffects.qml compares each
    // unit against its natural minimum width instead.

    // ── Pad FX window ──────────────────────────────────────────────────────
    // Perform-first pad cards. Four deck columns at `padfxDeckMinWidth` plus
    // the gaps are exactly the 1240 px default window width; narrower windows
    // page two columns at a time instead of squeezing the stem chips.
    /// Height of the pad trigger itself. The pad face is the LED.
    readonly property int padfxPadHeight: space8 + space2
    /// Minimum hit height for stem chips, shortcuts, bank keys, the edit rail
    /// and the page key. Floors at the 24 px pointer target.
    readonly property int padfxHitHeight: Math.max(24, fontSizeXs + 2 * space2)
    /// Shared row height for combo boxes, slider rows and compact pad rows.
    readonly property int padfxControlHeight: Math.max(Math.round(26 * uiScale),
                                                      fontSizeSm + 2 * space2)
    /// Informational mode / mute pill height.
    readonly property int padfxModeBadgeHeight: space4
    /// Column width at which four decks fit side by side.
    readonly property int padfxDeckMinWidth: Math.round(300 * uiScale)

    readonly property int deckHeaderHeight: Math.round(52 * uiScale)
    readonly property int stemLaneMinWidth: Math.round(58 * uiScale)
    /// Width a stem lane needs for its label to read as a word rather than an
    /// elided fragment: colour chip, label, status dot, volume fader and the
    /// mute/solo pair. Used to decide how many lanes go in one row, so the 58 px
    /// hard minimum above never drives that choice on its own.
    readonly property int stemLaneComfortWidth: Math.round(110 * uiScale)
    readonly property int stemLaneHeight: Math.round(110 * uiScale)
    readonly property int minWindowWidth: 1280
    readonly property int minWindowHeight: 720

    // Width gates for the toolbar's optional widgets, in the order they fit as
    // the window widens. The main toolbar keeps only its transport and basics
    // below the first one.
    readonly property int masterTempoMinWidth: 1100
    readonly property int recordingBoxMinWidth: 1200
    readonly property int masterMeterMinWidth: 1400
    /// Below this the toolbar status cluster would collide with the centred
    /// master tempo / recording box.
    readonly property int toolbarStatusMinWidth: 1000

    /// Height the deck needs for its fixed parts: info bar, waveform overview,
    /// pane and toolbar. The band uses it so the deck can never be clipped by a
    /// shorter mixer, which is what made the panes read wrong.
    /// 4 + 52 (header) + 134 (toolbar 22 + pane 112) + 4 + 46 (overview) = 240.
    readonly property int deckMinHeight: Math.round(240 * uiScale)
    /// Height of the MOVE / CUE / GRID deck band: the horizontal tab row plus
    /// the pane content. Kept constant so the waveform, library and deck height
    /// do not move when panes change.
    readonly property int deckPaneHeight: Math.round(112 * uiScale)
    /// Horizontal pane tab row above the pane (CUE / MOVE / GRID).
    readonly property int deckPaneTabHeight: Math.round(16 * uiScale)
    /// Visible pane below the tab row and its gap. Floored at zero: a pane
    /// height below the tab row would otherwise feed negative sizes into the
    /// pane contents while the deck is being laid out.
    readonly property int deckPaneContentHeight: Math.max(0,
            deckPaneHeight - deckPaneTabHeight - space1)
    /// Hard floor the pane may not shrink below beside the loop card.
    readonly property int deckPaneMinWidth: 196
    /// Width of the loop card. Fixed rather than scaled so the 196 px pane
    /// minimum still fits at a 564 px deck at every uiScale.
    readonly property int deckLoopCardWidth: 224
    /// Loop-card metrics: a header and one action row, sized to their content
    /// and vertically centred in the deck band.
    readonly property int deckLoopInset: Math.max(1, Math.round(space1 / 2))
    readonly property int deckLoopRowGap: 1
    readonly property int deckLoopHeaderHeight: space4
    readonly property int deckLoopKeyRowHeight: space6
    /// Border, insets, header, gap and the action row; the row sum is exact.
    readonly property int deckLoopCardHeight: 2 + 2 * deckLoopInset
            + deckLoopHeaderHeight + deckLoopRowGap + deckLoopKeyRowHeight
    /// Gap between keys and the side padding inside a key. Key labels are
    /// measured with their own metrics, so this is the only padding they carry.
    readonly property int deckLoopKeyGap: Math.max(1, Math.round(2 * uiScale))
    readonly property int deckLoopKeyPadding: Math.max(1, Math.round(2 * uiScale))
    /// Loop-active indicator dot in the header.
    readonly property int deckLoopStatusDotSize: Math.max(4, Math.round(6 * uiScale))

    // Deck-local width gates: the spinny/FX column hides first, then the tempo
    // strip, each at its own deck width rather than the window's. The row is
    // main column 490 + 5 + tempo 60 (+ 5 + spinny 140) + 8 of deck margin; a
    // gate below that sum lets the row outgrow the deck, which clips it.
    // The jog wheel is opt-in (Preferences > Interface > Show a jog wheel).
    // Between the first two
    // gates there is room for it or the tempo strip, not both, and it wins.
    readonly property int deckSpinnyMinWidth: 643
    readonly property int deckFullRowMinWidth: 708
    readonly property int deckTempoMinWidth: 563

    /// Window width below which the focused deck keeps the centred mixer.
    readonly property int focusedDeckMinWindowWidth: 1400

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
