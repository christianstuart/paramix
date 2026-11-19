# F07 mocks - deck loop card and pane layout

- **Issue:** #65
- **Status:** design gate for owner approval. No QML is changed by this document.
- **Baseline:** assumes F13 has merged: the deck has three panes (Move, Cue, Grid),
  the stem pane is gone, and a stored `[Skin],deckN_pane` of 3 already opens Cue.
- **Scale:** every number below is at `Theme.uiScale = 1.0`. The band and row
  heights follow `uiScale` through `Theme.deckPaneHeight` and the spacing grid.
  The loop card width is a fixed deck metric (like today's 185) and is clamped
  to `paneSubRow - 196` so the 196 px pane minimum survives at any scale.
- **Measured inputs:** `res/qml/Deck.qml`, `Deck/Loop.qml`, `Deck/DeckPaneTabs.qml`,
  `Deck/BeatJump.qml`, `Deck/GridPane.qml`, `Theme/Theme.qml`,
  `docs/qml-responsive-layout.md`, and the bounds assertions in
  `src/test/qml/qmlstartupsmoketest.cpp`.

The goal is the same in both candidates:

- the loop card fills the whole 112 px transport band instead of 62 px of it;
- the vertical 34 px tab column goes away;
- the band height stays at `Theme.deckPaneHeight` (112), so the waveform strip,
  library, and deck height (`Theme.deckMinHeight`, 240) do not move;
- every loop-card action stays reachable, including the two right-click-only
  ones.

## 1. Current deck band at 1280x720

Deck width = `(1280 - mixer 152) / 2 = 564`. Deck content = `564 - 2 * space1 = 556`.
Main column = `556 - tempo 60 - 2 * space1 = 488`. The transport row is 488 wide
and 112 tall; inside it the pane / loop / tabs sub-row is
`488 - play/cue 60 - space1 = 424` wide.

```text
deck 564 x 240 = 2*space1 (8) + header 52 + overview 46 + toolbar 22 + band 112
transport row 488 x 112 = play/cue 60 + gap 4 + pane 197 + gap 4 + loop 185 + gap 4 + tabs 34

+----------+  +-----------------------------+  +---------------+  +------+
| PLAY CUE |  |  pane 197 x 112             |  | loop 185 x 62 |  | CUE  |
|    60    |  |  CUE / MOVE / GRID          |  |               |  | MOVE |
|          |  |  (minimum 196 x 112)        |  |  25 px above  |  | GRID |
|          |  |                             |  |  25 px below  |  |      |
+----------+  +-----------------------------+  +---------------+  +------+
                                                ^ 50 px of the 112 px band
                                                  unused by the loop card
```

Wasted areas, in tokens:

| Area | Today | Cost |
|---|---|---|
| Loop card height | 62 (`1 + 12 + 3*15 + 3*1 + 2`) inside a 112 band | 50 px, split 25 above and 25 below because the item is `Qt.AlignVCenter` |
| Pane tabs | vertical 34 x 112 column | 34 px of transport width + `space1` gap |
| Pane minimum | 196 x 112 | forces the whole sub-row to 112 even when a pane needs less |
| FX-assign column | 135 wide; 180 tall when the deck is at least 708 wide | bounds test requires at least 30 px tall; not touched by F07 |

The pane minimum is why the band cannot shrink: the smallest asserted deck is
564 wide, and the current sub-row gives the pane exactly 197 px.

## 2. Candidate A - horizontal tabs above the pane

The loop card grows to the full band height and to 224 px wide (the 185 it has
today plus the 34 px tab column and one 4 px gap is 223, rounded to 224). The
pane column gets the rest: `row - 224 - space1`. The three tabs sit in a 16 px
row above the pane.

```text
transport row 488 x 112
+----------+  +----------------------------+  +-----------------------------+
| PLAY CUE |  | loop 224 x 112             |  | pane column 196 x 112       |
|    60    |  | LOOP [4.00][A]        (o)  |  | [ CUE ][ MOVE ][ GRID ] 16  |
|          |  | < [1/8][1/4][1/2][1][2] >  |  +-----------------------------+
|          |  |    ===== scroll =====      |  | pane 92                     |
|          |  | [IN|^][OUT|v][LP][REL][STP]|  | CUE / MOVE / GRID contents  |
|          |  | [ROLL][1/2][1][2][4]       |  |                             |
+----------+  +----------------------------+  +-----------------------------+
```

At 1280x720: `424 - 224 - 4 = 196` pane width, exactly the current minimum.

### Loop-card rows (A)

Card inner height = 112 - 2 borders = 110. Rows are:

| Row | Height | Token | Contents |
|---|---|---|---|
| Header | 16 | `deckLoopHeaderHeight = space4` | `LOOP` label, editable beat-size field (`beatloop_size`, 46 x 16), anchor key `A` (`loop_anchor`), loop-enabled lamp (`loop_enabled`) |
| Beat sizes | 39 | `deckLoopBeatRowHeight = 39` | `loop_halve` and `loop_double` at the ends; a scrolling viewport with 1/8, 1/4, 1/2, 1, 2, 4, 8, 16, 32 as `beatloop_<size>_toggle`; 5 visible at a time; 3 px scrollbar under the keys |
| Transport | 24 | `deckLoopActionRowHeight = space5` | split `IN` (`loop_in` + `loop_in_goto`), split `OUT` (`loop_out` + `loop_out_goto`), `LOOP` (`beatloop_activate`), `REL`/`EXIT` (`reloop_toggle` / `loop_enabled`), `STOP` (`reloop_andstop`) |
| Roll | 24 | `deckLoopRollRowHeight = space5` | `ROLL` label + 1/2, 1, 2, 4 as `beatlooproll_<size>_activate` |
| Inset | 2 top + 2 bottom | `deckLoopInset = space1 / 2` | |
| Row gaps | 3 x 1 | `deckLoopRowGap = 1` | |

Sum: `2 + 4 + 16 + 39 + 24 + 24 + 3 = 112`. No unused height.

The beat viewport is scrolling, not paged, because 9 sizes do not split evenly
into pages. The scrollbar is the visible affordance; wheel and drag remain
accelerators. Halve and double are pinned at the left and right ends.

### Pane contents (A)

| Pane | Height | Contents |
|---|---|---|
| Cue | 92 | 2 x 4 hotcues with `space2` margins and `space1` gaps: pads are `(92 - 16 - 4) / 2 = 36` px tall. Unchanged behavior. |
| Move | 92 | `BeatJump`: `space1` margins + 12 px header + `space1` gap leaves 68 px for Back/Fwd and the size strip. The strip hides below a 420 px pane, so it is hidden at 1440x852 (276) and below and shown from 1792x1008 (452). |
| Grid | 92 | `GridPane` in its existing `dense` mode (it already switches below 104). Its natural minimum is 86 px (`26 + 18 + 16 + 16 + 3*2 spacing + 2*2 margins`), so 92 leaves 6 px of slack. The FINE and CLR latches are 14 px keys in the BPM row; the BPM readout yields width first, as it already does in dense mode. |

Tab row: 16 px (`deckPaneTabHeight = space4`), three equal keys CUE / MOVE /
GRID. This replaces the 34 x 112 vertical column. The bounds assertion for the
pane changes from `>= 110` to `>= 90`; tabs get a `>= 16` assertion.

## 3. Candidate B - CUE permanent, MOVE in the loop card

The pane stack disappears: Cue is always shown at the full 112 px. Move becomes
a jump row inside the loop card. The grid editor is opened by a visible `GRID`
key in the Cue pane header and reuses the existing `GridPane` as a 112 px drawer
over the same band. That keeps every grid control bound without a fourth tab
column or a hidden gesture.

```text
transport row 488 x 112
+----------+  +----------------------------+  +-----------------------------+
| PLAY CUE |  | loop 224 x 112             |  | pane 112                    |
|    60    |  | LOOP [4.00][A]        (o)  |  | CUE            [EDIT][GRID] | 16
|          |  | < [1/8][1/4][1/2][1][2] >  |  +-----------------------------+
|          |  |    ===== scroll =====      |  | 2 x 4 hotcues, 40 px pads   | 84
|          |  | [IN|^][OUT|v][LP][REL][STP]|  |                             |
|          |  | [JUMP][<][-][4.00][+][>]   |  |                             |
|          |  | [ROLL][1/2][1][2][4]       |  |                             |
+----------+  +----------------------------+  +-----------------------------+
```

At 1280x720 the pane is again `424 - 224 - 4 = 196` wide, now 112 tall.

### Loop-card rows (B)

| Row | Height | Token | Contents |
|---|---|---|---|
| Header | 16 | `deckLoopHeaderHeight = space4` | same as A |
| Beat sizes | 32 | `deckLoopBeatRowHeight = space6` | same scrolling row and controls as A, 4 px tighter |
| Transport | 18 | `deckLoopActionRowHeight = 18` | In / Out / Loop / Rel / Stop, split In/Out as in A |
| Jump | 18 | `deckLoopJumpRowHeight = 18` | `JUMP` label, `beatjump_backward`, `beatjump_size_halve`, editable `beatjump_size` field (52), `beatjump_size_double`, `beatjump_forward` |
| Roll | 18 | `deckLoopRollRowHeight = 18` | `ROLL` + four roll keys |
| Inset | 2 + 2 | `deckLoopInset = space1 / 2` | |
| Row gaps | 4 x 1 | `deckLoopRowGap = 1` | |

Sum: `2 + 4 + 16 + 32 + 18 + 18 + 18 + 4 = 112`.

### Pane contents (B)

| Area | Height | Contents |
|---|---|---|
| Cue header | 16 | `CUE` label, `EDIT` latch (replaces the hotcue right-click popup), `GRID` key that opens the grid drawer |
| Hotcues | 84 | 2 x 4 grid, `112 - 2*space1 inset - header 16 - space1 gap = 84`; pads are `(84 - space1) / 2 = 40` px tall |
| Grid drawer | 112 overlay | the existing `GridPane`, opened by `GRID`; closes on Escape or a second press. All GridPane controls stay bound, including the FINE and CLR latches in the BPM row. |

`MOVE` no longer exists as a pane, and the 34 px column is gone. The loop card
carries the beatjump controls that used to live in that pane.

## 4. Responsive check

Width formula, at uiScale 1.0:

```text
deck          = (windowWidth - 152) / 2
pane sub-row  = deck - 8 (deck margins) - 60 (tempo) - 8 (gaps)
                     - 60 (play/cue) - 4 (gap)                 = deck - 140
pane width    = pane sub-row - loop 224 - space1 (4)           = pane sub-row - 228
```

| Window | Decks | Deck width | Pane sub-row | Pane width | Deck height | Band check |
|---|---|---|---|---|---|---|
| 2560x1369 | 2 | 1204 | 1064 | 836 | 240 | 224 + 4 + 836 = 1064 |
| 1792x1008 | 2 | 820 | 680 | 452 | 240 | 224 + 4 + 452 = 680 |
| 1440x852 | 2 | 644 | 504 | 276 | 240 | 224 + 4 + 276 = 504 |
| 1280x800 | 2 | 564 | 424 | 196 | 240 | 224 + 4 + 196 = 424 |
| 1280x720 | 2 | 564 | 424 | 196 | 240 | 224 + 4 + 196 = 424 |
| 2560x1369 | 4 | 1204 | 1064 | 836 | 240 each | band 480 = 2 x 240 |
| 1792x1008 | 4 | 820 | 680 | 452 | 240 each | band 480 = 2 x 240 |
| 1440x852 | 4 | 644 | 504 | 276 | 240 each | band 480 = 2 x 240 |
| 1280x800 | 4 | 564 | 424 | 196 | 240 each | band 480 = 2 x 240 |
| 1280x720 | 4 | 564 | 424 | 196 | 240 each | band 480 = 2 x 240 |

Height arithmetic per deck is unchanged:
`2*space1 (8) + header 52 + overview 46 + toolbar 22 + band 112 = 240`.
Both candidates keep the band at 112, so the waveform strip, library, and
`Theme.deckMinHeight` do not move. Four decks split the same 480 px band into
two 240 px decks; each still has the same 112 px transport band.

The opt-in jog wheel does not break the width minimum. Between 643 and 708 px
the tempo strip is replaced by the spinny, and above 708 both fit:

| Deck width | Tempo | Spinny | Pane sub-row | Pane |
|---|---|---|---|---|
| 644 (1440 window, jog on) | hidden | 140 | 644 - 8 - 140 - 5 - 64 = 427 | 199 |
| 820 (1792 window, jog on) | 60 | 140 | 820 - 8 - 60 - 8 - 140 - 5 - 64 = 535 | 307 |

Both stay above the 196 px pane minimum. The FX-assign column sits in the
spinny column and only exists from a 708 px deck; its height is
`240 - 2*space1 - 52 = 180`, well above the bounds test's 30 px floor.

## 5. Hidden gestures replaced

| Gesture today | Bound control | Replacement |
|---|---|---|
| Right-click `In` | `loop_in_goto` | Split key: `IN` sets, a visible right-hand `^` segment jumps to the in point. Both are separate accessible targets with tooltips. |
| Right-click `Out` | `loop_out_goto` | Split key: `OUT` sets, a visible right-hand `v` segment jumps to the out point. |
| Right-click `Rel` | `reloop_andstop` | Visible `STOP` key beside `REL`, with the tooltip "Reloop and stop". |
| Right-click a grid nudge key | `beats_translate_half`, `shift_cues_*_small` | `FINE` latch in the GridPane BPM row. While lit, the six nudge keys send the fine control; the latch has an accessible state and a tooltip. |
| Right-click a marker key | `*_clear` | `CLR` latch in the GridPane BPM row. While lit, the four intro/outro keys clear instead of set. |
| Wheel on the loop size strip | `beatloop_size` stepping | Visible 3 px scrollbar under the beat-size viewport. Wheel and drag remain accelerators, never the only path. |
| Right-click a hotcue | cue color/clear popup | Kept in Candidate A: the pads keep `Accessible.description` ("right click opens the color and clear menu"), which is a standard secondary action rather than a removed control. Candidate B replaces it with the `EDIT` latch in the Cue header. If the owner wants A to drop it too, an EDIT latch costs 16 px of pad height (36 -> 31 px). |

## 6. Pane persistence

`[Skin],deckN_pane` keeps its values: 0 = Move, 1 = Cue, 2 = Grid. No schema or
control-name change, so skin controls and controller mappings keep working.

- **Candidate A:** 0, 1, and 2 keep their current meaning. A stored 3 (the
  removed stem pane) opens Cue on load, which F13 already does; F07 does not
  rewrite it again.
- **Candidate B:** 0 opens Cue (Move no longer has a pane), 1 opens Cue, 2 opens
  Cue with the grid drawer open, 3 opens Cue. The stored value is left alone so
  the choice stays reversible if panes return; only 3 is rewritten to 1 by the
  existing F13 migration.
- **Both:** the active pane is not persisted while the grid drawer is open in B;
  the drawer is a transient view over Cue.

## 7. Recommendation

**Recommend Candidate A.** It fixes both wasted areas without dropping the
three-pane model: the loop card gets the full 112 px and 224 px wide, the
vertical tab column and its 34 px are gone, and all three panes stay one click
away with the same persistence values. The pane only loses 20 px of height
(92), still above GridPane's 86 px natural minimum, and the hotcue pads stay
36 px tall. Every loop action, including the former right-click In/Out and
reloop-and-stop, becomes a visible key, and the grid's fine and clear actions
get latches in a row that already exists. The change is also the smaller of the
two: the pane model and its persistence contract do not change, and BeatJump
and GridPane keep their current controls. Candidate B is the better choice only
if the owner values the extra 4 px of hotcue pad height and a permanently
visible Cue pane more than an inline grid; it also moves the grid behind a
drawer and changes the meaning of stored pane 0, which is a larger behavior
change for the same loop-card result.
