# F08 - DDJ-SX: load buttons toggle headphone cue; backspin needs a jog press

- **Branch:** `bradley/fix-ddj-sx`
- **Worktree:** `C:\Users\Bradl\paramix-fx-shell` (reused after F04) or its own; no shared files
- **Lane / wave:** Shell lane, wave 1 (can run fully in parallel with everything)
- **Depends on:** wave 0
- **Blocks:** nothing
- **Needs its own build:** no (mapping files are read from `res/`)

## Owned files

- `res/controllers/Pioneer DDJ-SX.midi.xml`
- `res/controllers/Pioneer-DDJ-SX-scripts.js`

## What is known

**Load buttons.** There is no MIDI collision: LOAD is `0x96/0x46`, headphone CUE is `0x90/0x54`.
The script does it on purpose:

```javascript
PioneerDDJSX.loadButton = function(...) {
    if (value) {
        engine.setValue(group, "LoadSelectedTrack", true);
        if (PioneerDDJSX.autoPFL) { /* sets pfl on this deck, clears it on the others */ }
    }
};
```

`PioneerDDJSX.autoPFL` defaults to `true`, so every load switches the headphone cue to that deck.
Loading itself works in the QML UI (`ControlLoadSelectedTrackHandler.qml`).

**Backspin.** `jogPlatterTick` only scratches when `engine.isScratching()` is true, and scratching
is enabled only by the touch sensor (`jogTouch` -> `scratchEnable`). On release it calls
`scratchDisable(deck, true)`. So:

- a spin given without the top being registered as touched falls through to pitch bend, and
- after release the platter's remaining ticks are ignored, so a thrown backspin stops with the hand.

There is no use of `engine.spinback()` in the mapping.

## Steps

1. Ask the owner which backspin gesture is meant, then test on the hardware:
   (a) throw the platter backward and let go, or (b) spin from the outer ring.
2. Load buttons: set `autoPFL` default to `false`. If the mapping exposes user settings, make it a
   setting; otherwise keep the variable at the top of the script with a comment.
3. Backspin (a), inertia after release: on touch release, do not disable scratch while platter
   ticks are still arriving. Keep scratching until ticks stop for a short window (a timer
   restarted by each tick), then `scratchDisable(deck, true)`. Look for a Pioneer mapping in
   `res/controllers` that already does this and copy its constants rather than inventing them.
4. Backspin (b), if wanted: in vinyl mode, a fast backward ring movement above a velocity
   threshold while playing triggers `engine.spinback()`. Only add this if the owner asks for it;
   it changes how ring nudging feels.
5. Check that slip mode, the play-button brake and the shift-jog search still behave.

## Verification

- All mappings still load (`tools/controller_ui_parity.py` if landed, and the controller fixture
  tests).
- On hardware: load does not change headphone cue; a thrown backspin carries through and playback
  resumes; nudging with the ring is unchanged.
- This cannot be verified without the controller. Say so in the PR and leave it for the owner to
  test before merge.

## Done when

The owner confirms both behaviors on the DDJ-SX.
