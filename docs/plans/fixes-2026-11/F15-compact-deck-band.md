# F15 - Deck transport band at loop-card height

- **Branch:** `bradley/fix-compact-deck-band`
- **Base:** local `main` at `3616c75`
- **Needs its own build:** no for the QML, yes to run `QmlResponsiveBoundsTest` (pinned values change)
- **Depends on:** F14 for `PadFxPadFace.qml` (or take ownership of it and rebase F14)

## Goal

Shrink the band beside the loop card from 112 px to the loop card's own height, **55 px**
(`Theme.deckLoopCardHeight` = 2 + 2x2 inset + 16 header + 1 gap + 32 key row). Every current
indicator stays on screen except the two cue-shift keys, which the owner chose to make
controller-only.

## Owner decisions (2026-11-22)

| Question | Answer |
|---|---|
| Area | Deck transport band only (pane tabs + Cue / Move / Grid / Pad FX pane). Header, overview, tempo and loop card unchanged. |
| Tabs | Keep a horizontal row, thinner: 12 px. Pane gets 41 px. |
| Grid pane | Everything stays except **C◀ / C▶ (shift cues ±10 ms)**: removed from the UI; `shift_cues_*` stay mappable. |
| Move pane | Drop the "BEATJUMP" caption; one row. |
| Pad / hotcue grid | **Adaptive**: 4x2 one-line faces on narrow panes, 8x1 two-line faces when the pane is wide (~540 px+). Elided text keeps the full name in the tooltip. |
| Pad face extras | None (no mode badge, no target dot). |

## Owned files

- `res/qml/Theme/Theme.qml` - `deckPane*` tokens only
- `res/qml/Deck/DeckPaneTabs.qml`, `HotcueAndStem.qml`, `BeatJump.qml`, `GridPane.qml`, `PadFxPane.qml`
- `res/qml/HotcueButton.qml` / the hotcue face inside `HotcueAndStem.qml`, `res/qml/PadFxPadFace.qml` (layout)
- `res/qml/Deck.qml` - band height binding only
- `src/test/qml/qmlstartupsmoketest.cpp` - the pinned band values (`:472-480`)
- `docs/qml-responsive-layout.md` - new section; mark the "pane cannot go much below 72" note as superseded

## Height budget (uiScale 1)

| Part | Now | After |
|---|---|---|
| Tab row (`deckPaneTabHeight`) | 16 | 12 |
| Tab gap | 4 | 2 |
| Pane (`deckPaneContentHeight`) | 92 | 41 |
| **Band (`deckPaneHeight`)** | **112** | **55**, bound to `deckLoopCardHeight` so the two stay equal under `uiScale` |

`Theme.deckMinHeight` drops by the same 57 px (240 → ~183). The deck's height is
`max(mixer, deckMinHeight)` (`MainWindow.qml`), so at 1440x852 the band becomes mixer-bound
(~186) and the main waveforms and library gain about 54 px. **Assumption:** the freed height goes to
the waveforms and library. It does not give the overview or the header more room.

## Pane layouts

Pane widths measured by F07: 196 (1280x800), 273 (1440x852), 309 (1792x1008), 693 (2560x1369).

- **Tabs:** same four keys and order. 12 px, `fontSizeXs`; the Grid lock badge becomes a 2 px
  underline. The Pad FX tab keeps its `FX 1` / `FX 2` label and shift-click.
- **Cue (8 hotcues) and Pad FX (8 pads):** one shared rule, `columns: paneWidth >= Theme.deckPadRowMinWidth ? 8 : 4`, with `deckPadRowMinWidth` ≈ 540.
  - 4x2 (rows ~19 px): one line, `"N label"`. Hotcue colour, lit state and the Pad FX active
    fill are unchanged; the label is elided.
  - 8x1 (row 41 px): number above a label of up to 2 lines, as today.
  - Margins go from 12/8 to 2/2 and the gap from 4 to 2.
  - The empty-state text ("no track" / hotcues hidden) gets one centred line.
- **Move:** one row: `◀ BACK | size | ‹ 1 2 4 8 16 › | FWD ▶`. The size strip keeps its
  existing `width >= 420` gate and its 1-5 key count. BACK/FWD keep their text-hiding rule.
- **Grid:** two rows of ~19 px with glyph or short labels and full names in tooltips:
  - Row 1: BPM readout (mono 12 px; was 15) · ½ · ×2 · Tap · F · C · Align · Match
  - Row 2: |◢ ◢| |◣ ◣| (intro/outro markers) · ◀ ▶ (beats) · B− B+ · Lock · Undo
  - Delete the C◀/C▶ row (`GridPane.qml:307-322`). FINE still switches ◀▶ to half-beat.
  - **Risk:** at the 196 px floor (1280x800), row 2 is 10 keys of about 17 px. If the audit
    reports clipping, ask the owner before moving anything else off screen. Don't decide it in code.

## Steps

1. Theme tokens: set `deckPaneTabHeight` 12, the tab gap to `deckLoopKeyGap`, bind `deckPaneHeight` to
   `deckLoopCardHeight`, add `deckPadRowMinWidth`.
2. Tabs, then Cue and Pad FX (shared adaptive rule), then Move, then Grid.
3. Update the bounds test pins (41 / 12) and their message to the new arithmetic
   (55 = 12 + 2 + 41).
4. Run the audit at all five sizes:
   `PARAMIX_QML_DUMP_DIR=... PARAMIX_QML_SIZES=1280x800,1440x852,1792x1008,2560x1369 ./mixxx-test --gtest_filter='QmlResponsive*'`,
   then `python tools/qml_layout_audit.py <dump>`. Fix any text over text or text larger than its box.
5. Screenshots of all four panes at 1440x852 and 2560x1369 for the owner.
6. Docs: add a section to `qml-responsive-layout.md` and update the `pad-fx.md` grid description.

## Verification

- `QmlStartupSmokeTest`, `QmlResponsiveBoundsTest`, the audit clean (knob-art overhang excepted).
- Every control in the F15 inventory is still on screen except C◀/C▶. Check against the
  table in this plan's research (the same list `GridPane`/`BeatJump`/`HotcueAndStem`/`PadFxPane` render today).
- Right-click actions still work: hotcue colour popup, Pad FX editor, loop goto/reloop.
- `shift_cues_earlier/later(_small)` still respond from a controller mapping.

## Not covered

- Four-deck mode (already clips; see `qml-responsive-layout.md` "Remaining" 5).
- The Pad FX window (`PadFxDeckPanel.qml`) keeps its own layout.
