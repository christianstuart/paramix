# F17 - Readable cards: shorter overview, toolbar under the jog

- **Branch:** `bradley/review-padfx-compact` (follows F16)
- **Needs its own build:** no (QML only)

## Owner request (2026-11-22)

Keep the deck's footprint. Shrink the waveform overview and the jog platter so the Grid and Pad FX
cards get more room. The loop card may shrink. Move the slim bar under the overview (reverse, QNT,
SLIP, RPT, eject) to under the jog platter.

## Plan

| Part | F16 | F17 |
|---|---|---|
| Overview | fills (~63 px at 1440x852) | fixed `deckOverviewHeight` 44 |
| Card band (loop + pane) | fixed 64 | fills the rest, 64 minimum (~109 at 1440x852 with the jog) |
| Toolbar | 22 px row under the overview | under the jog, two stacked rows (48 px) while the jog shows; unchanged without the jog |
| Jog platter | ~127 | ~105 (row height minus the stacked toolbar) |
| Loop card width | 184 | 168 (RELOOP still 48, other keys 36 ≥ 33) |
| Pane at a 644 px deck | 188 | ~227 |
| Grid / pad labels | 9 px | 12 px once rows are ≥ 26 px (Grid) or faces ≥ 34 px (pads, hotcues) |

- Footprint: `deckMinHeight` stays 128 + 64; the deck height is still `max(mixer, deckMinHeight)`.
- `Toolbar.stacked` switches its row to a 3-column grid and keeps QNT/SLIP/RPT visible (the
  old `width > 250` gate applies only to the single row).
- `deckJogReservedWidth` drops to 500 (Play 52, loop 168 + 4, pane floor 188, tempo 60,
  margins 8, four 5 px gaps); the jog shows from 500 + 96.

## Verify

Screenshots of Cue / Move / Grid / Pad FX at the owner's 1440 window with the jog on, and one
with the jog off.
