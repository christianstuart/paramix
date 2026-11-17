# Paramix QML design system

The Paramix QML skin (`res/qml`) is being modernized in place toward a
Rekordbox/Traktor-inspired pro console look. This note describes the token
layer and the rules new components follow. It is a living document; update it
when a token group changes.

## 1. Where the tokens live

`res/qml/Theme/Theme.qml` is the single source of truth, registered as a QML
singleton by `res/qml/Theme/qmldir`. Components import it with:

```qml
import "../Theme"
```

New components must use the semantic names below. The legacy Mixxx names
(`darkGray`, `white`, `buttonNormalColor`, `imgButton`, ...) are kept as
aliases so un-migrated components keep working. Do not add new code that uses
the legacy names; migrate call sites opportunistically.

## 2. Token groups

| Group | Tokens | Notes |
|---|---|---|
| Surfaces | `surface0`..`surface3`, `surfaceHover`, `surfaceRaised`, `border`, `borderStrong`, `topHighlight`, `accentSoft`, `shadow`, `scrim` | `surface0` is the app stage, `surface3` is the most raised panel. `topHighlight` is the 1 px lit edge on raised plates. |
| Text | `textPrimary`, `textSecondary`, `textMuted`, `onAccent` | `onAccent` is for text/icons drawn on `accent`. |
| Accents | `accent`, `accentHover`, `accentPressed`, `positive`, `warning`, `danger`, `focusRing` | `focusRing` is for keyboard focus only. |
| Stems | `stemDrums`, `stemBass`, `stemVocals`, `stemOther`, `stemMuted`, `stemSolo` | Never encode stem identity by color alone; always pair with a label. |
| Decks | `deckA`..`deckD` | Thin top strips, badges, meters — never full panel tints. |
| Pads | `hotcueColors[0..7]` | Rekordbox 8-pad hues for set hotcues; unset pads stay recessed. |
| Meters | `vuGreen`, `vuYellow`, `vuRed`, `vuRail` | Segmented ladder on a recessed rail. |
| Phrases | `phraseIntro`, `phraseUp`, `phraseDown`, `phraseVerse`, `phraseBridge`, `phraseChorus`, `phraseOutro` | Reserved for the Rekordbox-style phrase display once beatgrid phrase storage lands. |
| Type | `fontFamily`, `fontFamilyDisplay`, `fontFamilyMono`, `fontSizeXs`..`fontSizeXxl`, `fontWeight*`, `letterSpacingCaps`, `lineHeight` | Use `fontSizeXs` + uppercase + `letterSpacingCaps` for section labels. |
| Spacing | `space1`..`space8` | 4 px base grid: 4, 8, 12, 16, 24, 32, 48, 64. |
| Radius | `radiusXs`..`radiusXl`, `radiusPill` | Traktor-sharp: panels `radiusSm` (2 px), keys `radiusSm`; only pills stay round. |
| Motion | `motionFast`, `motionNormal`, `motionSlow`, `motionSpringDuration` | Hover/press is `motionFast`; layout springs use `motionSpringDuration`. |
| Layout | `toolbarHeight`, `deckHeaderHeight`, `deckMinHeight`, `deckPaneHeight`, `stemLaneMinWidth`, `stemLaneHeight`, `minWindowWidth`, `minWindowHeight` | Minimum window is 1280x720, enforced by `main.qml` and declared in `res/qml/skin.ini`. `deckMinHeight` is the deck's fixed-parts total; the waveform overview absorbs the rest of the band. |
| Top bar effects | `topBarHeight`, `topBarEffectsHeight`, `fxControlHeight`, `fxKnobSize`, `fxSlotMinWidth`, `fxAssignKeyWidth` | The effects strip is one `fxControlHeight` row (30 px at scale 1) under the 44 px toolbar; `topBarHeight` is the sum and is what the deck band reserves. Each of the two chains carries A-D deck assignment keys; the super knob hides below `fxSuperMinWidth`. |

## 3. Named color schemes

`Theme.scheme` selects one of three palettes:

- **Paramix Dark** (default): neutral near-black surfaces, blue accent.
- **Paramix Midnight**: lower-contrast variant for dim rooms.
- **Paramix High Contrast**: Okabe-Ito color-blind-safe stem colors.

The palette is exposed as `Theme.palette` and every semantic token reads from
it. A skin can switch schemes at runtime by assigning `Theme.scheme`; keep
contrast in mind and check all three schemes when adding a color.

`textMuted` is tuned to at least 4.5:1 on the panel surfaces it labels, and
`textOnAccent` is chosen by WCAG contrast ratio (`Theme.contrastRatio`) rather
than a luminance threshold, so white-on-blue is no longer used where it fails
AA. Both are token-level fixes; do not override them per component.

## 4. Base controls

These are token-driven replacements for the legacy bitmap controls. Keep their
public properties stable because call sites still set them.

| File | Purpose | Preserved API |
|---|---|---|
| `Button.qml` | Flat checkable/toggle button | `activeColor`, `normalColor`, `pressedColor`, `highlight`, `checked`, `icon` |
| `ComboBox.qml` | Flat dropdown | `footerItems`, `popupMaxItem`, `popupWidth`, `activateFooter()` |
| `TextField.qml` | Flat text input | Standard `TextField` API |
| `SectionBackground.qml` | Panel surface | Default `Rectangle` |
| `SectionText.qml` | Uppercase section label | Default `Text` |
| `EmbeddedBackground.qml` | Inset surface | Default `Rectangle` |

## 5. Rules for new UI

1. **Tokens, not hex.** Do not hard-code colors except where a value must match
   a non-QML source (waveform renderer markers, VU-meter segment colors).
2. **Qt 6.4 only.** No `Accessible.value`/`minimumValue`/`maximumValue`/
   `stepSize`/`announce`, and no QtQuick import newer than 6.4. Run
   `python3 tools/qml_qt_api_guard.py` before committing.
3. **Localize and label.** Wrap user-visible strings in `qsTr()` and give
   interactive controls an `Accessible.role` and `Accessible.name`. See
   `docs/qml-i18n-a11y-notes.md`.
4. **Keep control bindings intact.** The design work must not change any
   `ControlProxy` group/key, model role, or signal wiring.
5. **Prefer console plates.** `SectionBackground` + 1 px `border` frame in flat
   matte black. No glossy top highlight except the faint `topHighlight`
   hairline kept for recessed wells (fader tracks, waveform rails).
6. **Transport colour language.** Play is signal-green backlit, cue is amber
   backlit, sync/active states are signal-blue. Never swap them.
7. **Motion is optional but consistent.** Hover and press feedback uses
   `Theme.motionFast`; do not invent new durations.
8. **Gesture budget.** Double-click on a knob resets it (the Mixxx convention).
   Band-level or destructive toggles go on right-click, so one gesture never
   both resets and kills a band.

## 6. Runtime customization

The skin is customizable at runtime without reloading. `res/qml/main.qml` binds
the `Theme` singleton to the persisted `[Paramix]` values through `Mixxx.Config`:

| Property | Default | Meaning |
|---|---|---|
| `paramixColorScheme` | `Paramix Dark` | `Paramix Dark`, `Paramix Midnight` or `Paramix High Contrast` |
| `paramixAccentColor` | empty | CSS color string; empty uses the scheme accent |
| `paramixFontFamily` | `Inter` | Interface font; Inter is bundled in `res/fonts` |
| `paramixFontScale` | `1.0` | 0.85–1.35 type scale |
| `paramixMotionScale` | `1.0` | 0.5–1.5 animation scale (snappy → relaxed) |
| `paramixShowWaveforms` | `true` | Main waveform stack |
| `paramixShowCoverArt` | `true` | Deck header cover art |
| `paramixShowStems` | `true` | Deck stem strip tab |
| `paramixShowHotcues` | `true` | Deck hotcue grid |
| `paramixShowLibrary` | `true` | Library pane |
| `paramixCompactDecks` | `false` | Tighter deck header and margins |

Waveform playhead position, waveform style and stem lane stacking use the
existing `[Waveform]` settings (`PlayMarkerPosition`, `WaveformType`,
`StemSplitTracks`).

The Paramix options live in the native Preferences dialog: appearance and
layout on **Interface**, the fourth EQ band on **Mixer**, and the waveform
settings on **Waveform**. The old QML settings popup is retired.

The Interface page no longer offers **Switch to classic Mixxx interface**; the
native Help menu has the reverse action, **Use Paramix QML Interface**, which
restarts with `--new-ui`.

## 7. Scope and status

The QML skin is currently **developer-only**: `src/skin/qml/qmlskin.cpp` is only
discovered when the app runs in developer mode, and macOS x64 CI builds with
`QML=OFF`. Making Paramix the default skin is a separate packaging decision once
the skin is feature-complete.

Follow-up: the retired QML settings popup (`res/qml/Settings.qml` and its
`Settings/` pages) was deleted on 2026-11-16 together with the single-use
helpers it kept alive (`FormButton`, `ActionPopup`, `ActionButton`,
`InputField`). The native Mixxx Preferences dialog is the only settings
surface. Do not add a second one.

Stem-specific UI (4-lane stem strip, acapella/instrumental complement buttons,
separation status) is described in `docs/paramix-stem-beatgrid-ui-plan.md`
§5.8/§6.6. Solo is currently QML-level mute orchestration; a native engine solo
control is a later change.
