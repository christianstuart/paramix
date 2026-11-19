# F05 - Frequency colors on the non-stem waveform and the deck overview

- **Branch:** `bradley/fix-rgb-waveform`
- **Worktree:** `C:\Users\Bradl\paramix-fx-waveform` (reused after F01 merges)
- **Lane / wave:** Waveform lane, wave 2
- **Depends on:** F01 merged (lane order), F13 merged (it owns `WaveformDisplay.qml` first)
- **Blocks:** F12
- **Needs its own build:** only if the overview C++ changes; otherwise QML-only

## Owned files

- `res/qml/WaveformDisplay.qml` - the signal renderer blocks only (`WaveformRendererRGB`,
  `Filtered`, `HSV`, `Simple`)
- `res/qml/WaveformOverview.qml`, `res/qml/Deck/WaveformOverview.qml`
- `src/qml/qmlwaveformoverview.{h,cpp}` (only if the blend needs changing)
- `src/qml/qmlconfigproxy.cpp` (defaults only)
- `res/qml/Theme/Theme.qml` - one additive block of three tokens

## What is known

- The deck already defaults to the RGB renderer (`Mixxx.Config.waveformType`, default RGB), and the
  renderer already colors each column from its low/mid/high energy. It just is not fed red, green
  and blue: today `lowColor` is `#2154D7` (blue), `midColor` `#97632D` (brown) and `highColor`
  `#D5C2A2` (tan).
- The overview also defaults to RGB (`waveformOverviewType` 2) with `colorLow` `#E24B3F`,
  `colorMid` `#C45B9E`, `colorHigh` `#8B6BE0`.
- Signal renderers skip stem tracks (`hasStem()`), so this does not affect stem lanes.
- The preview deck bar (`res/qml/Mixer/PreviewDeckBar.qml`) has no waveform at all. "Deck preview
  waveform" is read here as the deck's overview strip. Adding a waveform to the preview deck bar
  is listed as an optional last step.

## Steps

1. Add `Theme.waveformLowColor`, `waveformMidColor`, `waveformHighColor` (red, green, blue, tuned
   for the dark background: fully saturated primaries read harsh, so start near `#FF3B30`,
   `#34D058`, `#3B82F6` and adjust by eye).
2. Bind the deck RGB renderer and the overview to those tokens. Leave the Filtered, HSV and Simple
   renderers alone apart from not regressing them.
3. Check the blend on real material: kick-heavy sections read red, vocals and synths green to
   yellow, hats and cymbals blue to white; full-range mixes should trend toward white, not gray.
   If the overview's `getRgbPenColor` normalization makes everything pastel, fix the normalization
   there rather than over-saturating the tokens.
4. Honor a user who already picked another waveform type or custom colors in Preferences: only the
   defaults change.
5. Optional, only with integrator sign-off: a compact overview in `PreviewDeckBar.qml` using the
   same component.

## Verification

- Window grabs of a non-stem track at three zoom levels and of the overview, attached to the PR,
  next to the same track before the change.
- Stem track unchanged (grab).
- `QmlStartupSmokeTest`, the bounds test and the QML Qt-API guard pass.

## Done when

A non-stem deck and its overview show bass as red, mids as green and highs as blue by default.
