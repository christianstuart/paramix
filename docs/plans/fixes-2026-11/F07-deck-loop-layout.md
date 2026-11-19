# F07 - Refactor the loop card and deck panes

- **Branch:** `bradley/fix-deck-loop-layout`
- **Worktree:** `C:\Users\Bradl\paramix-fx-deck-ui` (reused after F13 merges)
- **Lane / wave:** Deck UI lane, wave 2
- **Depends on:** F13 merged (it removes the stem pane; this plan redistributes the space), and
  the owner's uncommitted `Loop.qml` rewrite being landed or parked in wave 0
- **Blocks:** nothing
- **Needs its own build:** only for the test change; QML work runs against the main build

## Owned files

- `res/qml/Deck.qml`, `res/qml/Deck/Loop.qml`, `Deck/DeckPaneTabs.qml`, `Deck/BeatJump.qml`,
  `Deck/GridPane.qml`, `Deck/HotcueAndStem.qml` (hotcue part), `Deck/Toolbar.qml`
- `res/qml/Theme/Theme.qml` - deck metrics block only
- `src/test/qml/qmlstartupsmoketest.cpp` - the deck assertions in the bounds test only
- `docs/qml-responsive-layout.md`

## What is known

- Transport row today: play/cue | toolbar (22 px) over [pane stack (fill, min 196x112) + loop card
  (185x62) + vertical tab stack (34x112)].
- The loop card is 62 px tall in a 112 px band, so about 50 px under it is unused.
- Panes: MOVE (beat jump), CUE (hotcues), GRID (beatgrid/BPM), plus STEM until F13 removes it.
  Selection persists per deck in `[Skin],deckN_pane`.
- The loop card has In, Out (right-click jumps to the point, a hidden gesture), Exit/Reloop,
  halve/double and one to four beat-size keys. The uncommitted working copy has no loop-roll keys.
- **The owner has an uncommitted rewrite of `Loop.qml` (about 260 lines changed).** Treat whatever
  wave 0 lands from that as the starting point; do not rebuild it from `main`.
- The bounds test asserts the pane is at least 110 px tall and the FX-assign column at least 30 px.

## Design direction (confirm with a mock before building)

- Loop card uses the full band height: a row of beat-size keys (1/8 to 32, scrolling or paged), a
  row with In / Out / Loop on-off / Reloop, halve and double at the ends, and an optional roll row
  (`beatlooproll_X_activate`) when height allows.
- Replace right-click-only actions with visible affordances or tooltips that state the gesture.
- With three panes left, consider horizontal tabs above the pane, or showing CUE permanently with
  MOVE folded into the loop card as a jump row, so the tab column and its 34 px go away. Pick one
  with the owner; do not ship both.
- Keep the constant band height so the waveform and library do not move when panes change.
- Responsive rules stay token-driven; two- and four-deck layouts and the 1280x720 minimum must fit.

## Steps

1. Annotated grab of the current deck with wasted areas marked, and one or two mock layouts.
   Owner picks. This is a subjective item; do not skip the gate.
2. Rebuild the loop card on the chosen layout.
3. Rework the pane selector and pane contents; keep `[Skin],deckN_pane` values stable (0 move,
   1 cue, 2 grid) or migrate them.
4. Update the bounds-test assertions to the new minimums and `docs/qml-responsive-layout.md`.

## Verification

- Bounds test at 2560x1369, 1792x1008, 1440x852, 1280x800 and 1280x720, two and four decks.
- `tools/controller_ui_parity.py` (if landed in wave 0) still passes: no bound control dropped.
- Grabs of every pane at the minimum and a large size in the PR.

## Done when

The loop card and panes fill the band without dead space, every former action is reachable without
a hidden gesture, the owner has approved the grabs, and the bounds test pins the new layout.
