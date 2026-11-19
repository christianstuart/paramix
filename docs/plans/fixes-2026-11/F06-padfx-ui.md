# F06 - Make the Pad FX UI intuitive

- **Branch:** `bradley/fix-padfx-ui`
- **Worktree:** `C:\Users\Bradl\paramix-fx-effects-ui` (reused after F11 merges)
- **Lane / wave:** FX UI lane, wave 2
- **Depends on:** F11 merged (both edit the `MainWindow.qml` toolbar and View menu)
- **Blocks:** the `MainWindow.qml` entry point of F10
- **Needs its own build:** no (QML-only; run the main build against this worktree's `res/`)

## Owned files

- `res/qml/PadFxWindow.qml`, `res/qml/PadFxDeckPanel.qml`, `res/qml/PadFxParameter.qml`
- `res/qml/MainWindow.qml` - the Pad FX toolbar key and View menu item only
- `docs/pad-fx.md` (UI section)

Do not change `src/effects/padfx/*` or the `[ChannelN_PadFX]` control schema. If the UI needs a
control that does not exist, raise it with the integrator.

## What is known (Pad FX landed on `main` with PR #57)

- One free-floating 1240x720 `Window` with four deck columns, opened from a toolbar key or the
  View menu.
- Each deck shows four pad cards of 150 px: a pad button, effect / target / mode combo boxes, four
  22x20 px stem keys labeled D, B, O, V, and up to five sliders with 64 px labels.
- Problems visible in the code:
  - The backend has four banks per deck; the UI forces bank 1 on load and offers no selector.
  - Mode (Momentary, Toggle, One Shot) changes what a press does, and nothing on the pad says so.
  - Stem targets are single letters with no tooltip; hit areas are below a 24 px minimum.
  - `padN_muted` exists and is never shown, so a pad that silences a stem gives no feedback.
  - Without stems the whole column is disabled with a small line of text and no next step.
  - Configuration and performance are mixed: every pad always shows every combo box and slider.
  - All four decks are shown even in two-deck layouts.

## Design direction (confirm with a mock before building)

- **Perform vs. edit.** Default view is pads only: big pad, effect name, mode badge, target chips.
  An Edit toggle (or a per-pad disclosure) reveals effect/mode selectors and sliders.
- **Bank selector** A-D per deck, bound to `bank`; drop the forced reset to bank 1.
- **Stem targets as labeled chips** in the stem colors (Drums, Bass, Other, Vocals) with All /
  Instrumental / Vocals shortcuts; minimum 24 px tall.
- **State feedback:** lit pad from `padN_active`, a "muting" marker from `padN_muted`, and the mode
  shown on the pad face (hold / latch / one-shot glyph plus tooltip).
- **Decks shown follow the main window** (two or four); narrow windows page between decks.
- **Empty state:** "This deck has no stems" with a button that starts separation for the loaded
  track through `Mixxx.StemService`.
- Tooltips and `Accessible.name` on every control; no fixed pixel sizes outside `Theme` tokens.

## Steps

1. Produce one static mock per state (perform, edit, no stems) as QML with dummy data or as an
   annotated grab; get the owner's go-ahead. This is a subjective item, so do not skip the gate.
2. Restructure `PadFxDeckPanel.qml` into perform and edit layers; add the bank selector.
3. Replace stem keys with chips and shortcuts; wire `padN_muted`.
4. Deck count and paging; empty state.
5. Update `docs/pad-fx.md`.

## Verification

- `padfxtest` still passes (backend untouched).
- Grabs of each state at 1280x720 and 1920x1080 in the PR.
- Every bank reachable; a mapped controller pad and the on-screen pad stay in sync.
- `QmlStartupSmokeTest` and the QML Qt-API guard pass.

## Done when

A first-time user can trigger, retarget and re-bank a pad without opening a combo box to find out
what it does, and the owner has approved the grabs.
