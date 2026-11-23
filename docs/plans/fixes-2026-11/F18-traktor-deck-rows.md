# F18 - Traktor-style deck rows

- **Branch:** `bradley/review-padfx-compact` (replaces the F16/F17 card layout)
- **Reference:** `docs/reference/Screenshot 2026-11-22 at 11.22.{15,23,30} PM.png` (Traktor 4 deck)
- **Needs its own build:** no (QML only)

## Owner decisions (2026-11-22)

| Question | Answer |
|---|---|
| Direction | Use the horizontal space; model the controls under the overview on Traktor 4 |
| Tempo strip | Beside the overview only; SYNC and range move into row 1 |
| Loop size | Traktor strip only: halve/double chevrons and power-of-two keys, current size lit |
| Jog | Stays optional (Preferences > Interface > Show a jog wheel) |

## Layout

```text
header (unchanged)
overview ............................. | jog (opt.) | tempo: BPM/keylock + fader/nudge
row 1: Play  Cue | <REV QNT SLIP RPT EJECT | < 1/2 1 2 4 > A IN OUT LOOP RELOOP | SYNC RANGE
row 2: pane (hotcues / beatjump / grid / Pad FX), full width                  | CUE MOVE GRID FX
```

- The top row is a fixed `deckTopHeight`, row 1 a fixed `deckTransportRowHeight`, and row 2
  fills the rest. The deck footprint is unchanged.
- Only paramix controls are mapped. Traktor's CUP and FLX are not added.
- Shared `Deck/SizeStrip.qml` (from the BeatJump strip) drives both the beatjump and loop sizes.
- The loop card's editable size field, LOOP caption and status dot go away. RELOOP turns into a lit
  EXIT while looping, so the loop state stays visible.

## Verify

Screenshots of Cue / Move / Grid / Pad FX at the owner's 1440 window with the jog on and off.
