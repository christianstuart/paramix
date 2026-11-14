# Paramix QML design system

The Paramix QML skin (`res/qml`) is being modernized in place toward a
Rekordbox/Traktor-inspired look. This note describes the token layer and the
rules new components follow. It is a living document; update it when a token
group changes.

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
| Surfaces | `surface0`..`surface3`, `surfaceRaised`, `border`, `borderStrong`, `scrim` | `surface0` is the app background, `surface3` is the most raised panel. |
| Text | `textPrimary`, `textSecondary`, `textMuted`, `onAccent` | `onAccent` is for text/icons drawn on `accent`. |
| Accents | `accent`, `accentHover`, `accentPressed`, `positive`, `warning`, `danger`, `focusRing` | `focusRing` is for keyboard focus only. |
| Stems | `stemDrums`, `stemBass`, `stemVocals`, `stemOther`, `stemMuted`, `stemSolo` | Never encode stem identity by color alone; always pair with a label. |
| Phrases | `phraseIntro`, `phraseUp`, `phraseDown`, `phraseVerse`, `phraseBridge`, `phraseChorus`, `phraseOutro` | Reserved for the Rekordbox-style phrase display once beatgrid phrase storage lands. |
| Type | `fontFamily`, `fontFamilyDisplay`, `fontFamilyMono`, `fontSizeXs`..`fontSizeXxl`, `fontWeight*`, `letterSpacingCaps`, `lineHeight` | Use `fontSizeXs` + uppercase + `letterSpacingCaps` for section labels. |
| Spacing | `space1`..`space8` | 4 px base grid: 4, 8, 12, 16, 24, 32, 48, 64. |
| Radius | `radiusXs`..`radiusXl`, `radiusPill` | Panels use `radiusMd`; buttons use `radiusSm`. |
| Motion | `motionFast`, `motionNormal`, `motionSlow`, `motionSpringDuration` | Hover/press is `motionFast`; layout springs use `motionSpringDuration`. |
| Layout | `toolbarHeight`, `deckHeaderHeight`, `stemLaneMinWidth`, `stemLaneHeight`, `minWindowWidth`, `minWindowHeight` | Minimum window is 1280×720 (`res/qml/skin.ini`). |

## 3. Named color schemes

`Theme.scheme` selects one of three palettes:

- **Paramix Dark** (default): cool neutral surfaces, blue accent.
- **Paramix Midnight**: lower-contrast variant for dim rooms.
- **Paramix High Contrast**: Okabe-Ito color-blind-safe stem colors.

The palette is exposed as `Theme.palette` and every semantic token reads from
it. A skin can switch schemes at runtime by assigning `Theme.scheme`; keep
contrast in mind and check all three schemes when adding a color.

## 4. Base controls

These are token-driven replacements for the legacy bitmap controls. Keep their
public properties stable because call sites still set them.

| File | Purpose | Preserved API |
|---|---|---|
| `Button.qml` | Flat checkable/toggle button | `activeColor`, `normalColor`, `pressedColor`, `highlight`, `checked`, `icon` |
| `ActionButton.qml` | Dialog/menu action button | `category` (`None`/`Danger`/`Action`), `label` |
| `FormButton.qml` | Settings selector button | `backgroundColor`, `highlight`, `checked` |
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
5. **Prefer flat surfaces.** Use `Rectangle` + `border` + `radiusSm`/`radiusMd`
   instead of the legacy nine-slice images. The images remain until the last
   control is migrated.
6. **Motion is optional but consistent.** Hover and press feedback uses
   `Theme.motionFast`; do not invent new durations.

## 6. Scope and status

The QML skin is currently **developer-only**: `src/skin/qml/qmlskin.cpp` is only
discovered when the app runs in developer mode, and macOS x64 CI builds with
`QML=OFF`. Making Paramix the default skin is a separate packaging decision once
the skin is feature-complete.

Stem-specific UI (4-lane stem strip, acapella/instrumental complement buttons,
separation status) is described in `docs/paramix-stem-beatgrid-ui-plan.md`
§5.8/§6.6. Solo is currently QML-level mute orchestration; a native engine solo
control is a later change.
