# QML responsive layout

Status: stages 0-2 in progress. This file records the measurements the work is
based on, because two earlier assumptions about the failing machine were wrong.

## Measured window and screen

From the live application on the 200 %-scaled laptop:

```text
window=1440x852  dpr=2  screen=1440x900  avail=1440x852
```

So the skin receives **1440x852 logical px**. `Screen.width` and
`desktopAvailableWidth` already return logical pixels, and the window is
correctly clamped to them by `main.qml`.

The design target in `main.qml` is **1792x1008 logical**. The gap is therefore
352 px wide and 156 px tall — a normal-size window on a scaled display, not a
tiny one. Earlier estimates of "720x426 logical" came from applying the 2.0
device pixel ratio to coordinates that were already logical, and are wrong.

Practical consequence: the compact-layout breakpoints are relative to the
design size, not to a mobile-sized window.

| token (`Theme.qml`) | fires when | at 1440x852 |
|---|---|---|
| `shortWindow` | height < 900 | true |

There is no global `narrow`/`compact` width token. Width gates are per band
(`deckSpinnyMinWidth`, `masterTempoMinWidth`, ...) because each band has its own
minimum; a single window-width breakpoint kept hiding controls that fitted.

## Per-band minimum sizes

Measured from the QML. "needs" is the smallest size the band can render at
without clipping; every value is logical px.

| Band | Needs | Set by |
|---|---|---|
| Top bar | 44 high (main row) + 30 high (effects strip), ~390 wide without effects | `Theme.toolbarHeight` + `Theme.topBarEffectsHeight`; per-key 26x56 minimums |
| Mixer | 152 wide, ~190 high | `Mixer.qml` `implicitWidth`; `EqColumn` sets the height |
| Output bar | 32 high | `Mixer/OutputBar.qml` `implicitHeight` |
| Deck | ~1265 wide with spinny, ~457 without, ~386 with tempo hidden; 232 high | `Theme.deckMinHeight`; `Deck.qml` DelegateChooser `minWidth` 620 / 270 / 200 |
| Library | 358 wide, 120 high (228 with preview deck) | `Library.qml` sidebar 150 + browser 200 |
| Waveform strip | no intrinsic minimum | two `WaveformDisplay`s at `height / 2` |

At 1440x852 the width budget is 1440 for two decks plus a 196 mixer, so decks
get ~622 each — above the 457 the deck needs with the spinny hidden. Vertically,
44 + 299 (mixer + output) + 208 (library) + 301 (waveforms) exactly fills 852.

## What already adapts

These predate the responsive work and should be folded into the `Theme`
breakpoints rather than duplicated:

| Where | Condition | Effect |
|---|---|---|
| `MainWindow.qml:28` | `height > 515` | four-deck layout |
| `MainWindow.qml:183/192/200` | `width >= 1100 / 1400 / 1200` | master tempo / output meters / record box |
| `MainWindow.qml:773` | `height - mixer.height >= 400` | library shown |
| `MainWindow.qml:565,577,589` | `width < 1400` | focused-deck placement |
| `Deck.qml:163/175` | model `minWidth` 800 / 270 | hides spinny, then tempo strip |
| `Deck/InfoBar.qml:152,165,206` | `width > 500 / 450 / 400` | hides year / time / rating |
| `Mixer/OutputBar.qml:19` | `width < 1200` | tile width 56 vs 64 |

## Landed so far

- **Stage 2 — breakpoints.** `Theme.windowWidth` / `windowHeight`, bound from
  `main.qml`, plus `shortWindow`. Every existing width gate now comes from a
  named `Theme` token instead of a literal in the component:
  `masterTempoMinWidth` (1100), `recordingBoxMinWidth` (1200),
  `masterMeterMinWidth` (1400), `focusedDeckMinWindowWidth` (1400),
  `deckSpinnyMinWidth` (800), `deckTempoMinWidth` (270). Behaviour is
  unchanged; only the source of the numbers moved.
- **Stage 1 — structural floor.** `MainWindow.bandMinimumHeight` clamps the
  deck band's `SplitView.minimumHeight` to `windowHeight - toolbarHeight`, so
  the split can no longer reserve more than the window has.
- **Stage 1 — waveform strip.** `SplitView.preferredHeight` drops 160 -> 96
  when `shortWindow`, instead of a fixed 160.
- **Stage 1 — bounds test.** `QmlResponsiveBoundsTest` in
  `src/test/qml/qmlstartupsmoketest.cpp` starts the QML application, resizes
  the `MainWindow` item to each target size, waits for the layout to go
  quiescent, and fails listing every visible item whose mapped bounds escape
  the window (items under a clipping ancestor are skipped).

- **Stage 4 — `uiScale`.** `Theme.uiScale` (0.75-1.25, clamped in
  `QmlConfigProxy::set_paramixUiScale`) multiplies the spacing grid
  (`space1`-`space8`) and the fixed layout metrics (`toolbarHeight`,
  `deckHeaderHeight`, `stemLaneMinWidth`, `stemLaneHeight`). Text stays on
  `fontScale`, deliberately: they are two independent levers, so a compact
  display can tighten the grid without shrinking its labels. Persisted as
  `[Paramix] UiScale`; bound in `main.qml`. Covered by
  `QmlThemeUiScaleTest.ScalingShrinksTheSpacingGrid`.

- **Stage 5 — the harness matches the skin (2026-11-16).** The bounds test now
  drives `Theme.windowWidth`/`windowHeight` from the size it sets on the
  `MainWindow` item (before, it measured 1440x852 with the tokens still on the
  real 1792x1008 window), asserts the design size as well, fails when the band
  geometry never settles instead of silently measuring at the 3 s cap, and
  flags (a) visible items collapsed to zero height inside a live parent,
  (b) the deck FX-assign column below 30 px, (c) the deck pane below
  `Theme.deckPaneContentHeight` (92 px), and (d) the horizontal pane tab row
  below `Theme.deckPaneTabHeight` (16 px). Both deck heights are pinned in the
  test, so lowering the Theme token fails rather than relaxing the check. The
  settle check compares the top two layout levels only, because waveform
  playheads and VU segments animate by design.

## Overlap pass (2026-11-16)

The bounds test passed while the skin visibly overlapped, because it only saw
items leaving the *window*. Three root causes, none of them about window size:

- **Output bar over the decks.** The bar was anchored to `mixer.bottom`, but the
  decks are `max(mixer, Theme.deckMinHeight)` tall. With the mixer at 186 and
  the deck at 232, the bar covered the bottom 46 px of both decks (CUE, pane
  tabs, FX-assign keys) at every window size. It is now anchored below the
  taller of the two, and the mixer spans the band (`Mixer.qml` stretches its
  faders; `implicitHeight` stays the natural 186).
- **The font differed per machine.** `Theme.fontFamily` asked for "Helvetica
  Neue", which only macOS has. Windows substituted Tahoma, whose metrics the
  fixed boxes were never sized for. `Theme.fontFallbacks` now resolves to an
  installed Helvetica-metric family (Arial on Windows, Liberation Sans on
  Linux) and ends in the bundled Open Sans.
- **Gates below the real row width.** The deck row is main column 490 + 5 +
  tempo 60 (+ 5 + spinny 140) + 8. `deckSpinnyMinWidth` was 620 and
  `deckTempoMinWidth` 270, so the row outgrew the deck and `clip: true` cut the
  right-hand end off. The tempo strip now needs a 563 px deck and the full row
  (`deckFullRowMinWidth`) 708.
- **Jog wheels are opt-in.** Preferences > Interface > Show a jog wheel
  (`[Paramix] ShowJogWheels`, off by default) decides whether a deck has one at
  all. When on, it shows from
  a 643 px deck (`deckSpinnyMinWidth`, a 1440 px window) and takes the tempo
  strip's place until the deck reaches 708, where both fit. The rule is
  `Deck.rowItemShown()`. `PARAMIX_QML_JOG_WHEELS=1` makes the bounds test
  measure that layout.

Also: the library sidebar's source tree now shrinks to 120 px and the preview
deck hides below a 228 px sidebar (was a fixed 308 px that painted past the
window); the crossfader fits its slot and its unit no longer runs under the
output tiles; `FXAssign` drops its eyebrow label in the 30 px slot.

### Deck header (later the same day)

The header is 52 px (`Theme.deckHeaderHeight`, was 44) with 6 px insets top
and bottom, so `Theme.deckMinHeight` is 240. Its two rows carry explicit
height shares (22 : 18): with both merely filling, the title row took nearly
all of it and the artist / key / rating row was crushed onto the bottom border.
The track's key moved from the GRID pane into that second row.

The "Compact decks" option is gone; there is one deck density.

The stems pane's header row uses short "Aca" / "Inst" keys below a 280 px pane.
With the jog wheel shown the pane is ~196 px and the full-width keys ran under
the loop card. The pane also clips.

### Window fit

- A screen smaller than the design size opens the window maximized. Before, the
  client area was set to the whole available desktop and the frame hung off it.
- Below `Theme.minWindowWidth` x `minWindowHeight` (1280x720) the skin is
  scaled uniformly (`main.qml` `skinScale`, floor 0.5) instead of squeezed, so
  the layout never sees less than the floor. Popups are not scaled.

### Measuring

`QmlResponsiveBoundsTest` now asserts 2560x1369, 1792x1008, 1440x852, 1280x800
and 1280x720. It waits until no item has been created for half a second (the
library and the second deck load asynchronously), counts items as part of the
settle check, and re-applies its size until the Loader stops overriding it. For an audit:

```bash
cd build
QT_QPA_FONTDIR=C:/Windows/Fonts PARAMIX_QML_DUMP_DIR=C:/tmp/dump   PARAMIX_QML_SIZES=1366x720,1920x1009 ./mixxx-test --gtest_filter='QmlResponsive*'
python tools/qml_layout_audit.py C:/tmp/dump
```

Run it off-screen (the default). On the `windows` platform plugin the tree was
measured at 0x0. `QT_QPA_FONTDIR` gives the off-screen font database the system
fonts; without it only the bundled ones exist. The audit reports text over
text, text larger than its box, short labels elided and children outside their
parent. Knob art 3-5 px taller than its hit box is reported and intended.

Not covered: four-deck mode, the sampler row, maximized library,
and popups. The dump only sees the default two-deck layout.

## Deck fixes after the first run

Two problems reported on the 1440x852 window, both arithmetic rather than taste:

- **Jog wheels hidden.** The spinny column was gated at 800 px deck width, but the
  spinny alone only needs the main column, itself and the tempo strip (~600).
  The FX-assign column beside it was what pushed the requirement to 800. Now
  `deckSpinnyMinWidth` is 620 and the FX column carries its own
  `deckFxColumnMinWidth` of 800, so the spinny returns and the FX column still
  hides until there is room for it.
- **Stem labels elided to "Dru...".** The stem pane is only ~365 px: the loop
  card (185) and the pane tabs (34) share the same transport row. Four lanes
  side by side were ~86 px each, so every label became a fragment. The wrap
  decision now uses `stemLaneComfortWidth` (110) instead of the 58 px hard
  minimum, which makes that pane 2x2 with ~180 px lanes. Four across now only
  happens when each lane really gets the label width.

Measured in the harness at 1440x852: deck width **644**, and 564 at 1280x800.

## Deck height, second pass (2026-11-16)

The 2026-11-15 compaction took the deck band to 192 px while the deck's fixed
children still needed ~186 and the pane was told to render a 112 px stem layout
in 78 px. That produced the overlapping panes: stems lanes 9-22 px tall, the
FX-assign keys at 0 px, the loop card 62 px of content in a 58 px box, and the
info bar's two rows sharing 24 px with 20 px left blank.

| Element | Before | After |
|---|---|---|
| `Theme.deckMinHeight` | 192 | 232 |
| `Theme.deckPaneHeight` (pane + tabs) | 78 | 112 |
| Loop card | 58 fixed | 62 (= content) |
| Info bar bottom inset | 20 | 4 |
| Output bar | 46 | 54 (fits the 2x16 px crossfader keys) |

The waveform overview absorbs the difference; at 1440x852 the band is
232 + 54 = 286 of the 299 px the previous compaction budgeted. Stem lanes wrap
to 2x2 at every current deck width, so a lane now hides its QuickFX row when it
is shorter than 64 px instead of overflowing the card. `BeatJump` drops its
size strip below a 420 px pane and keeps the header's size field.

## Vertical compaction

The deck's height is pinned to the mixer's (`MainWindow.qml:506`), so shrinking
the deck alone only tightens it. Both bands had to come down together.

| Element | Before | After |
|---|---|---|
| Info bar (`Theme.deckHeaderHeight`) | 56 | 44 |
| Waveform overview | 50 | 40 |
| Deck pane (hotcues / beatjump / grid / stems) | 92 | 72 |
| Loop card | 92 | 58 |
| Deck toolbar | 24 | 22 |
| Tempo strip width | 75 | 60 |
| Tempo fader / well | 20 / 14 | 16 / 10 |
| Tempo and pitch buttons | 24 x 22 | 20 x 18 |
| EQ knob | 36 (30 inner) | 32 (27 inner) |
| QuickFX knob | 36 (30 inner) | 30 (25 inner) |
| QuickFX preset selector | 18 | 16 |

Loop took the deepest cut: internal rows 24 -> 18, eyebrow 14 -> 10, padding
`space2` -> `space1`. The pane tabs dropped to a 16 px minimum so four tabs
still fit the shorter pane.

Deck content: 234 -> 190. Mixer: ~212 -> ~189. The deck therefore gets ~189
instead of ~253, about 75 % of its previous height.

**The pane cannot go much below 72:** BeatJump needs its eyebrow plus two rows,
and the stems pane needs an 18 px header plus lane rows. Reaching a strict 2/3
would clip both.

## Top bar effects (2026-11-16)

The standard effects moved from a deck-band row into a second top-bar row, so the deck band gained
the old 168 px back and the effects stay visible with the library maximized. The strip is
`Theme.topBarEffectsHeight` (30 px at uiScale 1) and the whole top bar is `Theme.topBarHeight`
(74 px); `MainWindow.bandMinimumHeight` subtracts it.

| Part | Compact rule |
|---|---|
| Deck assignment | Four independent toggle keys per chain; any combination of decks A-D can be active at once |
| Selectors | `Layout.fillWidth`, elide, full name in a tooltip; never a fixed 300 px |

The per-deck quick chains stay in the mixer; they are not duplicated in the strip. The strip is
measured by `QmlResponsiveBoundsTest` at the same five sizes as the rest of the skin. At 1280 the
bounds test still passes; the deck band is 30 px shorter than before.

## Deck band and pane tabs (F07, 2026-11-19)

The deck transport band stays at `Theme.deckPaneHeight` (112), but the 34 px vertical tab column is
gone. The band is a compact loop card (224 px wide, 53 px tall) centred beside a pane column with a
horizontal CUE / MOVE / GRID row above the pane:

| Part | Height | Token |
|---|---|---|
| Loop card | 53 | `Theme.deckLoopCardHeight` |
| Pane tab row | 16 | `Theme.deckPaneTabHeight` |
| Tab gap | 4 | `Theme.space1` |
| Pane | 92 | `Theme.deckPaneContentHeight` |
| Loop header | 16 | `Theme.deckLoopHeaderHeight` |
| Loop action row | 32 | `Theme.deckLoopKeyRowHeight` |

The loop card has a header (beat size, anchor, loop lamp) and one In / In-goto / Out / Out-goto /
Loop / Rel / Stop row. The row heights are fixed and sum to the card height exactly at any
`uiScale`, so the card never needs to stretch. The fixed beat-size and loop-roll rows are gone;
both control sets remain in the engine for controllers.

The pane keeps its 196 px floor: at the 564 px deck a 1280 px window gives,
`224 (loop) + 4 (gap) + 196 (pane) = 424` transport px inside a 429 px main column. The pane is
273 px at 1440x852 and 309 px at 1792x1008; the 135 px FX-assign column appears from a 708 px
deck, which is why the pane does not grow by the full window delta. At 2560x1369 it is 693 px.
Deck height, waveform strip and library split are unchanged.

The bounds test now asserts the pane is at least `Theme.deckPaneContentHeight` (92) and the tab
row at least `Theme.deckPaneTabHeight` (16), and keeps the FX-assign column at 30 px. Both
numbers are pinned in the test, so a Theme edit that lowers the band fails instead of quietly
weakening the assertion. The old `Deck/Loop.qml` width gate is gone.

## Deck band at loop-card height (F15, 2026-11-22)

The band beside the loop card is now exactly as tall as the card: `Theme.deckPaneHeight` is
bound to `deckLoopCardHeight` (55 px at uiScale 1), so the two never drift under `uiScale`. This
supersedes the "pane cannot go much below 72" note above: the panes went to single rows
instead.

| Part | Before | After | Token |
|---|---|---|---|
| Tab row | 16 | 12 | `deckPaneTabHeight` |
| Tab gap | 4 | 2 | `deckPaneTabGap` (= `deckLoopKeyGap`) |
| Pane | 92 | 41 | `deckPaneContentHeight` |
| Band | 112 | 55 | `deckPaneHeight` = `deckLoopCardHeight` |
| `deckMinHeight` | 240 | 183 | 128 + band |

| Pane | Layout |
|---|---|
| Tabs | Same four keys; the Grid lock badge is a 2 px underline. |
| Cue, Pad FX | 4 x 2 one-line faces (`N label`, elided, tooltip) below `deckPadRowMinWidth` (540); one row of eight two-line faces above it. |
| Move | One row: Back, editable size, size strip (still hidden below a 420 px pane), Fwd. The "Beatjump" caption is gone. Back/Fwd minimum 60 so the row fits the 196 px pane. |
| Grid | Two rows: BPM, ½, ×2, Tap, F, C, Align, Match / four intro-outro markers, ◀ ▶, B− B+, Lock, Undo. The cue-shift keys (`shift_cues_*`) are controller-only. |

The deck is `max(mixer, deckMinHeight)` tall, so the band now follows the mixer and the main
waveforms and library get the difference. `QmlResponsiveBoundsTest` pins 55 / 41 / 12.

## Still open

- **Narrow-width mixer rules still deferred.** The sleek-mixer work landed, and
  its EQ/QuickFX stack came down in height as part of the compaction above, but
  the narrow-width rules (drop the EQ rows, columns 84 -> 56) are still not
  written.
- **No UI control for `uiScale` yet.** The setting and the binding exist, but
  the control belongs in the Interface preference page. Until then it can only
  be set in `paramix.cfg`.
- **Below ~1200 px wide the split layout still oscillates** (see below). The
  window minimum is now `Theme.minWindowWidth`/`minWindowHeight` (1280x720), so
  a user cannot drag the window into the unstable range, but the binding cycle
  itself is unfixed.

## Known open issues found by the bounds test

- **The layout does not converge below roughly 1200 px wide.** At 1024x768 the
  geometry keeps changing: repeated runs of the bounds test either pass or
  fail at that size, and a quiescence wait never settles. That is a real
  oscillation (a binding cycle, most likely between the library's `active`
  threshold, the split's minimum heights and the deck height derived from
  `mixer.height`), not a measurement artefact. The test therefore asserts only
  at 1792x1008, 1440x852 and 1280x800, which are stable, and this needs its own
  fix before smaller sizes can be asserted.
- **The `ApplicationWindow` cannot be resized from C++.** Its `width`/`height`
  are bound to `Screen`-derived values in `main.qml`, so `QQuickWindow::resize`
  and `setProperty("width", ...)` change the native window while the skin item
  keeps its old size. The bounds test works around this by sizing the
  `MainWindow` item directly.

## Remaining

1. **`narrow`/`compact` tokens do not exist.** Width gates are per band; the
   earlier plan to fold them into one global pair was abandoned because each
   band has its own minimum. Keep adding named per-band tokens.
2. **No UI control for `uiScale`.** It can only be set in `paramix.cfg`.
3. **Below ~1200 px wide the layout still oscillates.** The window minimum is
   now 1280x720, so a user cannot reach it, but the binding cycle is unfixed.
4. **Library column widths ignore `fontScale`** (`Library/SourceTree.qml`
   preferred widths are fixed px) and elided titles have no tooltip.
5. **Four-deck mode clips.** `bandContentHeight / 2` gives each deck ~116 px
   while the deck's fixed parts need ~186. Four decks need either a taller
   window or the minimized deck card; the toggle is currently a footgun.

## Note on measuring

A temporary `console.log` from `MainWindow.qml` root handlers did not appear in
`paramix.log` even though removing `MainWindow.qml` provably breaks the app, so
the file is the live one. Logging from `main.qml` works. Worth understanding
before relying on QML-side instrumentation again; the numbers above were taken
from `main.qml` and from reading the components.

## Taller cards and the jog column (F16, 2026-11-22)

| Part | Before | After |
|---|---|---|
| Loop card / pane band | 55 | 64 (header 20 + key row 37) |
| Loop card width | 224 | 184 |
| Pane floor | 196 | 188 |
| Play / Cue | 60 x 60 each | 52 wide, together toolbar + band high |

- Columns fill the row height, so the overview stretches and the cards sit on the deck's bottom edge.
- The opt-in jog platter is its own side column between the main column and the tempo strip, as
  wide as the row is tall, capped at `width - deckJogReservedWidth` (516). It shows from a
  626 px deck (`deckSpinnyMinWidth`) and no longer replaces the tempo strip.
- `Button.labelPadding` / `labelWidth`: Grid keys use 2 px padding and their label width as the
  layout minimum, so no key elides. Below a 240 px pane the Grid adds a middle row for
  Align / Match / Lock / Undo.
- 4 x 2 pad and hotcue faces wrap names to two tight lines before eliding.
- The Play key shows only its glyph; the word stays on the minimized deck.

### F17: cards fill, toolbar under the jog

The overview is a fixed `deckOverviewHeight` (44) and the card band fills the rest (64 minimum,
~109 at 1440x852), so the deck footprint is unchanged. While the jog shows, the deck toolbar
(reverse, QNT, SLIP, RPT, eject) sits under the platter in two rows (`Toolbar.stacked`,
`deckSideToolbarHeight` 48) and the platter shrinks to ~105. The loop card is 168 wide
(`deckJogReservedWidth` 500). Grid keys and pad/hotcue faces switch to 12 px text once their rows are tall
enough.

## Traktor deck rows (F18, 2026-11-22)

Supersedes the F16/F17 card layout. Under the header: a fixed `deckTopHeight` (56) row with the
overview, the optional jog platter (as wide as the row is tall) and the compact tempo strip
(`TempoColumn.compact`, `deckTempoCompactWidth` 100); a fixed `deckTransportRowHeight` (28) row with
Play, Cue, the deck toolbar (`Toolbar.compactRow`), the loop row (shared `Deck/SizeStrip.qml`,
A, In, Out, Loop, Reloop) and SYNC / range; then the pane row filling the rest, full width, with
the stacked CUE / MOVE / GRID / FX selector on its right (`deckPaneTabColumnWidth` 44).
`deckMinHeight` is the sum of those rows. At 1440x852 the Cue and Pad FX panes are one row of
eight and the Grid uses two rows.

### F18b: big jog, full-height tempo

The jog platter and the tempo strip now run the full height under the header on the right
(`deckJogReservedWidth` 380 keeps the left controls' minimum). The tempo strip is the fader beside a
column of BPM readout, pitch/keylock, SYNC and range. The rate nudge keys (rate_perm_*/rate_temp_*)
are controller-only. On the left: overview (`deckTopHeight` 36), the transport row, the loop row (both
`deckTransportRowHeight` 26) and the pane, which keeps a 40 px floor (`deckPaneRowMinHeight`).
