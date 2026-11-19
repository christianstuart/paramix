# F13 - Stem labels, mute/solo and volume on the waveform; remove the stem deck pane

- **Branch:** `bradley/fix-stem-lane-controls`
- **Worktree:** `C:\Users\Bradl\paramix-fx-deck-ui`
- **Lane / wave:** Deck UI lane, wave 1
- **Depends on:** wave 0
- **Blocks:** F05 and F12 (`WaveformDisplay.qml`), F07 (deck panes)
- **Needs its own build:** only for the test change; QML work runs against the main build

## Owned files

- `res/qml/StemLaneStrip.qml`
- `res/qml/WaveformDisplay.qml` - the strip instantiation block only
- `res/qml/Deck.qml`, `res/qml/Deck/DeckPaneTabs.qml`, `res/qml/Deck/HotcueAndStem.qml` - removal
  of the stem pane only; layout changes belong to F07
- `src/test/qml/qmlstartupsmoketest.cpp` - only if an assertion references the stem pane

No engine change: solo stays QML-side orchestration over `[ChannelN_StemX],mute`. An engine solo
control is a possible follow-up and would touch `src/engine/`, which this plan avoids.

## What is known

- `StemLaneStrip.qml` overlays the left edge of a stem waveform, one row per lane. Today each row
  has a color key that doubles as the mute toggle, the stem name in capitals taken from the
  track/model label, and a gain **knob** on `[ChannelN_StemX],volume`. There is no solo.
- PR #55 fixed lane order to drums, bass, other, vocals.
- The stem deck pane is pane 3 in `Deck.qml` (`Deck.Pane.Stems`), the fourth tab in
  `DeckPaneTabs.qml`, and `paneMode: 2` of `HotcueAndStem.qml`. It holds per-lane mute, solo
  (QML logic that saves and restores the other lanes' mutes), a volume fader, a QuickFX knob with
  enable and preset, and Acapella / Instrumental buttons.
- With the library visible a lane is about 20 px tall, so each row's controls must fit in 20 px.
- The pane selection persists in `[Skin],deckN_pane`; a stored 3 must not leave a deck with no
  pane.

## What removing the pane drops

- Per-stem QuickFX knob, enable and preset in the UI. The controls remain for controllers, and
  Pad FX covers per-stem effects. Not re-homed in this plan.
- Acapella / Instrumental shortcuts. Re-home these as two small keys at the top of the strip if
  they fit; otherwise drop and note it in the PR.

## Steps

1. Strip rows: fixed labels DRUMS, BASS, OTHER, VOCALS by lane position (fall back to the track
   label only if a stem file declares something else), an **M** key, an **S** key and a horizontal
   volume slider with double-click reset, all inside the 20 px row. Keep the fading panel and the
   press/wheel swallowing so the strip never scrubs the track.
2. Move the solo logic out of `HotcueAndStem.qml` into the strip (one owner of solo state per
   deck): solo mutes the others and remembers their previous mutes; a second solo adds to the
   solo set; clearing restores. A mute changed from a controller while solo is active must not
   corrupt the saved state.
3. Narrow layouts: below a width threshold show label + M + S and reveal the slider on hover.
4. Remove `Deck.Pane.Stems`, its tab and `paneMode: 2`. Map a stored `deckN_pane` of 3 to Cue on
   load. Leave the remaining three panes' layout exactly as is; F07 reworks it.
5. Delete QML that only the stem pane used. Check `tools/controller_ui_parity.py` and the mappings
   for anything that selects pane 3.
6. Tooltips and `Accessible.name` for every key and slider.

## Verification

- Grabs: stem deck with library shown (20 px lanes) and hidden (tall lanes), two and four decks,
  one lane muted, one soloed.
- Mute, solo and volume stay in sync with a controller mapped to the stem controls (Traktor F1 or
  Hercules Mk2 mappings from W1).
- A profile saved with pane 3 opens on Cue.
- `QmlStartupSmokeTest`, bounds test and the QML Qt-API guard pass.

## Done when

Each stem lane carries its name, M, S and a volume slider on the waveform, and the deck has no
stem pane.
