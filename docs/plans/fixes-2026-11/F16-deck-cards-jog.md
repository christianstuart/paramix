# F16 - Taller cards, no clipped labels, jog wheel beside the deck

- **Branch:** `bradley/review-padfx-compact` (follows F14 + F15)
- **Needs its own build:** no (QML only)

## Owner decisions (2026-11-22)

| Question | Answer |
|---|---|
| Card height | Loop card and function card 55 -> **64 px** |
| Labels | No key may elide to "..." (Grid keys did) |
| Jog layout | Rekordbox-style platter + tempo strip on the right, from the overview's top to the cards' bottom; the overview narrows |
| Width at a 644 px deck | **Slim the card row**: loop card 224 -> 184, Play/Cue 60 -> 52, pane floor 196 -> 188 |

## Findings

- **Dots in the Grid pane:** `Button.qml` reserves 6 px of label margin per side and elides. The 14 px F/C keys keep ~2 px for text, and the row-2 glyph keys squeeze to ~16 px.
- **Blank space above and below the cards:** the generic `column` delegate in `Deck.qml` has no `Layout.fillHeight`. Play/Cue (60 + 5 + 60 = 125) are taller than toolbar + cards (22 + 5 + 55 = 82), so the cards float in the transport row.
- **No jog wheel:** `[Paramix] ShowJogWheels` is off by default. It still exists under Preferences > Interface. When on, a 644 px deck had room for the jog or the tempo strip, not both, and the platter was a fixed 140 px.

## Steps

1. Theme: header 20 + key row 37 gives a 64 px loop card. Loop card 184, pane floor 188. Add `deckJogReservedWidth` (516 = everything in the row except the jog) and set `deckSpinnyMinWidth` to 516 + 110.
2. `Button.qml`: add a `labelPadding` (default 6) and a read-only `labelWidth`. Grid keys use 2 px padding and their label width as their layout minimum.
3. `Deck.qml`: columns fill the row height. Play/Cue are 52 wide and together exactly toolbar + card height. The jog column is its own side column, as wide as it is tall (capped by the deck width), and shown with the tempo strip, not instead of it. The FX-assign keys stay under the jog when there is room.
4. `Spinny.qml`: the platter, hub and tick scale with the column instead of a fixed 140 px.
5. Verify with screenshots of all four panes at the owner's 1440 window, jog on and off.
