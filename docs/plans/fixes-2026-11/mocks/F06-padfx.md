# F06 mock - Pad FX perform / edit states

- **Issue:** #64
- **Lane:** FX UI, wave 2 (F11 > F06)
- **Status:** design gate. This document is the only deliverable; no QML is changed.
- **Decision requested:** approve the perform-first pad card with a **per-pad disclosure**
  (pencil key -> one pad expands, its siblings collapse to one-line summaries). The alternative,
  a deck-level Edit toggle that expands all four pads, is rejected below.
- **Branch:** `bradley/fix-padfx-mock` (docs only)

## 1. Ground truth from the shipped code

Everything below is drawn from code that already exists on `main` (PR #57 and its follow-ups).
No new backend control is proposed.

| UI value | Source | Range / notes |
|---|---|---|
| Pad FX group | `[ChannelN_PadFX]` (`PadFxUnit` derives it from the deck group) | one group per deck |
| Bank | `[ChannelN_PadFX] bank` | `ControlPotmeter` 1..4; the selected bank is saved in the `[PadFX]` config and restored by `loadConfig()`; changing it stops active pads and restores their mutes (`slotBankChanged`) |
| Pad trigger | `[ChannelN_PadFX] padN` | `ControlPushButton`, write; press = 1, release = 0 |
| Pad active state | `[ChannelN_PadFX] padN_active` | read-only `ControlObject`; the pad's lit state |
| Pad mute ownership | `[ChannelN_PadFX] padN_muted` | read-only `ControlObject`; 1 while the pad owns the stems' native mute |
| Effect | `[ChannelN_PadFX] effectN` | 0 Off, 1 Vinyl Brake, 2 Echo Out |
| Target mask | `[ChannelN_PadFX] targetN` | 1..15; 1 Drums, 2 Bass, 4 Other, 8 Vocals |
| Mode | `[ChannelN_PadFX] modeN` | 0 Momentary, 1 Toggle, 2 One Shot |
| Parameters | `[ChannelN_PadFX] padN_param1..5` | 0..1 |
| Stem count | `[ChannelN] stem_count` | 0 = empty state |
| Separate action | `Mixxx.StemService.generateStems(track)` | existing QML invokable |
| Separation status | `Mixxx.StemService.statusForTrack(track)`, `messageForTrack(track)`, `available`, `hasModel`, `progress` | existing QML singleton |
| Loaded track | `Mixxx.PlayerManager.getPlayer(group).currentTrack` | `QmlTrackProxy` or null |
| Deck count | `MainWindow.show4decks ? 4 : 2` | existing read-only property |
| Existing labels | `paramLabel()` in `PadFxDeckPanel.qml` | Vinyl Brake: Wind-down, Curve, Wind-up. Echo Out: Echo Time, Feedback, Tail, Reverb, Rev Time |

The backend semantics that the mock must show honestly:

- **Momentary** engages only while held and never owns a mute.
- **Toggle** engages on press; when the gesture ends it sets the native mute and stays lit until pressed
  again.
- **One Shot** engages on press; when the gesture ends its active state drops, but it keeps owning the
  native mute (`padN_muted = 1`) until pressed again.
- **Changing bank** restores any mute owned by the old bank's pads and clears their active states.

## 2. Proposed Theme metrics

These are the only new tokens the implementation needs. They go in the Theme effects/pad block,
next to `fxControlHeight`. Values are exact at `uiScale = 1`, `fontScale = 1`.

| Token | Formula | Value | Used for |
|---|---|---:|---|
| `padfxPadHeight` | `Theme.space8 + Theme.space2` | 72 | pad trigger |
| `padfxHitHeight` | `Math.max(24, Theme.fontSizeXs + 2 * Theme.space2)` | 25 | stem chips, shortcut keys, bank keys, edit rail, page key |
| `padfxControlHeight` | `26` (matches the existing `Skin.Button`/`Skin.ComboBox` implicit height) | 26 | combo rows, slider rows, empty-state key, compact pad rows |
| `padfxModeBadgeHeight` | `Theme.space4` | 16 | informational mode and mute badges |
| `padfxDeckMinWidth` | `Math.round(300 * uiScale)` | 300 | four-up width threshold |

Derived heights used in the drawings:

- Pad card = `2 * Theme.space2 + Theme.padfxPadHeight + 2 * Theme.space1 + 2 * Theme.padfxHitHeight`
  = 16 + 72 + 8 + 50 = **146 px**.
- Deck column = `Theme.space6` header + `Theme.space2` gap + 4 cards + 3 gaps
  = 32 + 8 + 584 + 24 = **648 px**, plus 2 x `Theme.space2` padding = **664 px**.
- Window = `Theme.space6` header + `Theme.space2` gap + 664 + 2 x `Theme.space2` margins
  = **720 px**. This is why the existing 1240 x 720 window is the right default: four
  `padfxDeckMinWidth` columns plus three `Theme.space2` gaps plus two margins is exactly 1240.
- Four-up threshold = `4 * Theme.padfxDeckMinWidth + 3 * Theme.space2 + 2 * Theme.space2`
  = 1200 + 24 + 16 = **1240 px**.

No hard-coded pixel sizes outside Theme. The only literal numbers are inside the token formulas:
the 24 px hit floor, the 26 px shared control height and the 300 px four-up column minimum. No
component carries a raw pixel value.

## 3. Perform view (default)

### 3.1 One deck column, four pads

Column width 300 at the four-up layout. Outer column padding is `Theme.space2` (8), so the card
width is 284. Card padding is `Theme.space2` (8), so the card content is 268. The edit rail is
`Theme.padfxHitHeight` (25) wide; the pad trigger is 243 x 72.

```text
  32  Theme.space6
  ┌──────────────────────────────────────────────────────────┐
  │ DECK 1                                     [A][B][C][D]  │  bank keys 25x25, 4 px apart
  └──────────────────────────────────────────────────────────┘
  8   Theme.space2
  72  Theme.padfxPadHeight
  ┌───────────────────────────────────────────┬──────────────┐
  │ PAD 1                              [L]    │              │
  │ Vinyl Brake                               │      ✎       │
  │                                           │              │
  └───────────────────────────────────────────┴──────────────┘
  25  Theme.padfxHitHeight
  [● Drums] [● Bass] [● Other] [● Vocals]        4 chips, ~64 wide
  25  Theme.padfxHitHeight
  [ All ] [ Instrumental ] [ Vocals ]            3 shortcut keys, ~86 wide
  8   Theme.space2
  ┌───────────────────────────────────────────┬──────────────┐
  │ PAD 2                              [1×]   │              │  pad2_active = 1
  │ Echo Out                                  │      ✎       │  accent fill + light text
  │                                           │              │
  └───────────────────────────────────────────┴──────────────┘
  [● Drums] [● Bass] [● Other] [● Vocals]
  [ All ] [ Instrumental ] [ Vocals ]
  8   Theme.space2
  ┌───────────────────────────────────────────┬──────────────┐
  │ PAD 3                              [H]    │              │  pad3_active = 0
  │ Vinyl Brake                               │      ✎       │  momentary, not held
  │                                           │              │
  └───────────────────────────────────────────┴──────────────┘
  [● Drums] [● Bass] [● Other] [● Vocals]
  [ All ] [ Instrumental ] [ Vocals ]
  8   Theme.space2
  ┌───────────────────────────────────────────┬──────────────┐
  │ PAD 4                              [1×]   │              │  pad4_active = 0
  │ Echo Out                        ◉ MUTING │      ✎       │  pad4_muted = 1
  │                                           │              │  amber marker only
  └───────────────────────────────────────────┴──────────────┘
  [● Drums] [● Bass] [● Other] [● Vocals]
  [ All ] [ Instrumental ] [ Vocals ]
```

The window repeats this column per visible deck. At 1240 x 720 four columns fit exactly; see §6.

### 3.2 Pad trigger

- A `Skin.Button` with a custom pad face, fillWidth, height `Theme.padfxPadHeight` (72).
- Content, top-left: `PAD N` in `Theme.fontSizeXs`, uppercase, `Theme.letterSpacingCaps`.
- Content, second line: effect name in `Theme.fontSizeMd`, `Theme.fontWeightDemiBold`. `Off` uses
  `Theme.textMuted` and the pad is at 55% opacity.
- The pad face is the LED. `highlight: padN_active > 0` lights the whole face in `Theme.accent`
  and switches text to `Theme.textOnAccent`. No separate LED dot is added.
- Press/release writes `padN` = 1/0 exactly as today. The UI never writes `padN_active` or
  `padN_muted` (both are read-only).
- The effect is `Off`: the pad is dimmed, the tooltip says "No effect configured", and the backend
  ignores the press (as it does today).

### 3.3 Mode badge

A 16 px-high pill at the top-right of the pad face (`Theme.padfxModeBadgeHeight`):

| Mode control | Glyph | Tooltip |
|---|---|---|
| `0` Momentary | `H` | "Momentary - engages only while the pad is held." |
| `1` Toggle | `L` | "Toggle - press to engage; press again to release." |
| `2` One Shot | `1×` | "One shot - fires once, then holds the target stems muted until pressed again." |

The badge is informational, not a control. It is `Accessible.ignored: true`; the pad's
`Accessible.description` carries the full mode text. It still has a `ToolTip` and a
`HoverHandler`, because "what does a press do" is the question this badge answers.

### 3.4 Muting marker

A 16 px-high amber pill (`Theme.warning`, `Qt.alpha(Theme.warning, 0.14)` fill, 1 px border) with
`⊘ MUTING`, bottom-right of the pad face, visible only when `padN_muted > 0`.

This is the state that survives a one-shot: `padN_active` is 0, but `padN_muted` is 1, so the
pad is not lit yet it owns the stems' mute and must remain visibly actionable. The pad tooltip in
that state ends with "Press the pad to release the mute." The marker is informational
(`Accessible.ignored: true`); the pad description says "Muting".

### 3.5 Target chips and shortcuts

Four chips, always labelled and never color-only:

| Chip | Mask bit | On visual |
|---|---:|---|
| `● Drums` | `1 << 0` | 1 px `Theme.stemDrums` border, 18% stem-color fill, 8 px stem dot, `Theme.textPrimary` label |
| `● Bass` | `1 << 1` | `Theme.stemBass` |
| `● Other` | `1 << 2` | `Theme.stemOther` |
| `● Vocals` | `1 << 3` | `Theme.stemVocals` |

Off chips use `Theme.surface2`, a `Theme.border` frame, a `Theme.stemMuted` dot and a
`Theme.textMuted` label. The stem dot stays visible in both states, so the color reinforces the
label instead of replacing it.

Chip hit area: `Theme.padfxHitHeight` (25) high; at the four-up width each chip is ~64 wide.
Clicking toggles that bit through `targetN`. If clearing a bit would make the mask 0, the existing
`toggleTarget()` behavior keeps the last bit set (a pad always targets at least one stem).

Second row, neutral preset keys (`padfxHitHeight` high):

| Shortcut | Sets `targetN` | Lit when |
|---|---:|---|
| `All` | 15 | `targetN == 15` |
| `Instrumental` | 7 | `targetN == 7` |
| `Vocals` | 8 | `targetN == 8` |

Preset keys are not stem chips: they have no stem dot, use `Theme.surface2`/`Theme.textSecondary`
when inactive, and an `Theme.accent` border plus `Theme.textPrimary` when the current mask equals
their value. Tooltips spell out the mask ("Set pad 1 to all stems", "Set pad 1 to drums, bass and
other", "Set pad 1 to vocals").

### 3.6 Edit rail

A `Theme.padfxHitHeight` (25) wide rail on the right edge of the pad card, separated by a 1 px
`Theme.border`. It contains a 25 x 25 pencil key (`Theme.padfxHitHeight`), vertically centered:

- Perform state: glyph `✎`, tooltip "Edit pad N effect, mode and parameters".
- Edit state: glyph `✓`, tooltip "Done editing pad N".

The rail sits outside the pad button, so hovering or clicking it never triggers the pad.

### 3.7 Bank selector

Four keys `A` `B` `C` `D` in the deck header, each `Theme.padfxHitHeight` (25) square, 4 px apart,
bound to `bank` 1..4:

- Current bank: `Theme.accent` fill, `Theme.textOnAccent` label.
- Other banks: `Theme.surface2`, `Theme.border`, `Theme.textSecondary`.
- No `Component.onCompleted` reset to bank 1; the bank saved in the PadFX config is the source of
  truth (`PadFxDeckPanel.qml` should drop the current forced reset).
- Changing bank is immediate. The backend stops any active pad and restores its mute first; the
  UI therefore does not need a confirm dialog.

## 4. Edit view (per-pad disclosure)

Enter with the pencil key on one pad. The selected pad expands in place; the other three pads
collapse to 26 px summary rows. Only one pad per deck is expanded at a time. The deck column
scrolls only if a large font scale makes the content exceed the column height.

```text
  32  deck header: DECK 1  [A][B][C][D]
  25  compact PAD 1 · Vinyl Brake · L · Drums+Bass        ✎
  309 expanded PAD 2 (Echo Out, five sliders)
  ┌──────────────────────────────────────────────────────────┐
  │ PAD 2                              [1×]          [ Done ]│  25
  │ Effect  [ Echo Out                                  v ]  │  26
  │ Mode    [ One Shot                                  v ]  │  26
  │ [● Drums] [● Bass] [● Other] [● Vocals]                  │  25
  │ [ All ] [ Instrumental ] [ Vocals ]                      │  25
  │ Echo Time   [=======o------------------]                 │  26
  │ Feedback    [=====o--------------------]                 │  26
  │ Tail        [=========o----------------]                 │  26
  │ Reverb      [====o---------------------]                 │  26
  │ Rev Time    [=======o------------------]                 │  26
  └──────────────────────────────────────────────────────────┘
  25  compact PAD 3 · Vinyl Brake · H · Drums               ✎
  25  compact PAD 4 · Echo Out · 1× · Vocals                ✎  ◉ MUTING
```

- Expanded header: pad number, mode badge, and a `Done` key. `Done` is `Theme.space7` (48) wide
  and `Theme.padfxHitHeight` (25) high.
- Effect row: a 64 px label (`Theme.space8`) plus the existing `Skin.ComboBox`; row height
  `Theme.padfxControlHeight` (26). Model: Off / Vinyl Brake / Echo Out.
- Mode row: same layout; model Momentary / Toggle / One Shot. Changing it immediately updates the
  badge on the collapsed card.
- Chips and shortcuts: unchanged from perform view and still interactive, so a pad can be retargeted
  while editing.
- Sliders: `PadFxParameter` rows, label width 64 (`Theme.space8`), row height
  `Theme.padfxControlHeight` (26). Show only the parameters that have a label for the selected
  effect: three rows for Vinyl Brake, five for Echo Out, none for Off.
- Expanded height with five sliders is 25 (header) + 26 (effect) + 26 (mode) + 25 + 25 (chips) +
  5 x 26 (sliders) + 9 x `Theme.space1` (gaps) + 2 x `Theme.space2` (padding) = **309 px**.
  Vinyl Brake is 249 px; Off is 159 px.

Exit with `Done`, by pressing the `✓` rail key again, by pressing another pad's pencil, or with
Escape. Entering another pad collapses the current one. No global Edit mode exists, so the other
deck columns stay in perform view.

## 5. No-stems empty state

When `stem_count == 0`, the column header stays (deck name, bank keys disabled) and the four pad
cards are replaced by one centered panel. Height is the full 608 px body (the 648 px column content
minus the 32 px header and 8 px gap).

```text
  ┌──────────────────────────────────────────────────────────┐
  │ DECK 1                                     [A][B][C][D]  │  32; bank keys disabled
  │                                                          │
  │                  This deck has no stems                  │  32  Theme.space6
  │            "Track title" is loaded, but it has no        │  24  Theme.space5
  │                    separated stems yet.                  │  24
  │                                                          │
  │                  [ Separate stems ]                      │  26  padfxControlHeight
  │                                                          │
  └──────────────────────────────────────────────────────────┘
```

Variants, all driven by existing APIs:

| Condition | Copy | Action |
|---|---|---|
| no track | "Load a track to use Pad FX." | none |
| track, `!StemService.available` | "Stem separation is unavailable." | none |
| track, `!StemService.hasModel` | "Stem separation model not installed." | none |
| track, status `NotSeparated`, `MissingStemFile` or `Failed` | "This deck has no stems." + `messageForTrack(track)` when Failed | `Separate stems` |
| track, status `Queued` | "Queued for stem separation." | none |
| track, status `Separating` | "Separating stems - N%" using `StemService.progress` | none |
| track, status `Ready` but `stem_count` still 0 | "Stems are ready; waiting for the deck to reload them." | none |

The key calls `Mixxx.StemService.generateStems(deckPlayer.currentTrack)`. It is enabled only when
`StemService.available && StemService.hasModel && track !== null` and the track status is
`NotSeparated`, `MissingStemFile` or `Failed`. The panel uses `Theme.space4` (16) padding, a
`Theme.space6` (32) title row, `Theme.space5` (24) body rows and a `Theme.padfxControlHeight` (26)
key. No new backend method is added; there is no per-deck cancel because the proxy only exposes the
global `cancelAll()`.

When `stem_count` becomes positive, the column swaps to the perform view with the persisted bank.

## 6. Two-deck and four-deck behaviour

`PadFxWindow` receives `deckCount` from the existing `MainWindow.show4decks ? 4 : 2` property at
its current instantiation. This is the only wiring outside the pad panel; it is a property on the
existing window, not a new toolbar control.

### 6.1 Window at 1240 (default)

```text
┌ Pad FX ────────────────────────────────────────────────────────────────────────────┐
│ Pad FX    Per-stem performance effects                          Decks 1-2 of 4  [Close] │  32
├────────────────────────────────────────────────────────────────────────────────────┤
│ [ Deck 1 300 ] [ Deck 2 300 ] [ Deck 3 300 ] [ Deck 4 300 ]                         │  664
└────────────────────────────────────────────────────────────────────────────────────┘
   8 margins + 24 gaps + 1200 columns = 1240
```

### 6.2 Two-deck main window

```text
┌ Pad FX ────────────────────────────────────────────────────────────────┐
│ Pad FX    Per-stem performance effects                           [Close]│
├────────────────────────────────────────────────────────────────────────┤
│ [ Deck 1 608 ] [ Deck 2 608 ]                                          │
└────────────────────────────────────────────────────────────────────────┘
```

Only decks 1 and 2 exist in this mode; there is no page key and no deck 3/4 column. If the main
window switches from four decks to two while Pad FX is open, the page resets to the first page and
the window relayouts to two columns.

### 6.3 Four decks in a narrow window (< 1240)

```text
┌ Pad FX ────────────────────────────────────────────────────────────────────────────┐
│ Pad FX    Per-stem performance effects                         [ Decks 1-2 ] [Close] │
├────────────────────────────────────────────────────────────────────────────────────┤
│ [ Deck 1 488 ] [ Deck 2 488 ]                                                       │  at 1000
└────────────────────────────────────────────────────────────────────────────────────┘
```

- Two columns per page; page 0 = decks 1-2, page 1 = decks 3-4.
- The page key is `Theme.padfxHitHeight` (25) high, auto width, text `Decks 1-2` or `Decks 3-4`.
- The window minimum width stays 760, so at the narrowest size two columns are 368 wide and the
  chips are ~85 wide.
- Paging is presentation only. It never writes `[ChannelN_PadFX]`; a pad left running on page 1
  keeps running while page 0 is shown. Returning to the page re-reads `padN_active`/`padN_muted`.
- A page change while a page is out of range clamps to a valid page (e.g. four decks -> two decks
  resets to page 0).
- 1240 is the scale-1 threshold. The implementation compares the actual columns' implicit width,
  so a larger font scale pages earlier instead of clipping chip labels.

The page state is a local QML property, like the existing `TopBarEffects` page before F11. It is
not a control.

## 7. State table

| Visual | Control / source | Rule |
|---|---|---|
| Bank key lit `A`..`D` | `[ChannelN_PadFX] bank` | `Math.round(bank) == keyIndex`; write on click. No forced reset to 1. |
| Pad face lit | `[ChannelN_PadFX] padN_active` | `> 0`: `Theme.accent` fill + `Theme.textOnAccent`; otherwise `Theme.surface2`/`Theme.textPrimary`. |
| Pad trigger press | `[ChannelN_PadFX] padN` | Write 1 on press, 0 on release, exactly as today. |
| Amber `⊘ MUTING` pill | `[ChannelN_PadFX] padN_muted` | `> 0`: visible, `Theme.warning`; independent of `padN_active`, so a finished one-shot still shows it. |
| Mode badge `H` / `L` / `1×` | `[ChannelN_PadFX] modeN` | 0/1/2; tooltip and `Accessible.description` carry the full name. |
| Effect name on pad | `[ChannelN_PadFX] effectN` | 0 Off (dim, "No effect configured"), 1 Vinyl Brake, 2 Echo Out. |
| Stem chip on | `[ChannelN_PadFX] targetN` | Bit set -> stem-color frame/fill + labelled chip. |
| Shortcut lit | `[ChannelN_PadFX] targetN` | `All` at 15, `Instrumental` at 7, `Vocals` at 8; no other mask lights a shortcut. |
| Edit panel open | Local `editingPad` (1..4) | Exactly one pad per deck; the pencil key toggles it. |
| Slider position | `[ChannelN_PadFX] padN_param1..5` | Two-way as `PadFxParameter` already does; hidden rows for unused parameters. |
| Perform grid vs empty state | `[ChannelN] stem_count` | `> 0` perform; `0` empty state. |
| Empty-state key | `Mixxx.StemService` + `currentTrack` | Calls `generateStems`; disabled when no service/model/track or a job is queued. |
| Page key | Local `page` + `deckCount` + width | Visible only for four decks below 1240; never writes a control. |

## 8. Accessibility

Every interactive control gets `Accessible.role`, `Accessible.name` and a 500 ms `ToolTip`. Dynamic
values go in `Accessible.description`, not `Accessible.value` (not available on the Qt 6.4 target
per the design system). Glyph badges are `Accessible.ignored: true` and their text is folded into the
pad description.

| Control | Role | `Accessible.name` | Tooltip |
|---|---|---|---|
| Bank A..D | Button, checkable | `Pad FX bank A for deck 1` | `Bank A - four pad configurations` |
| Pad trigger | Button | `Pad 1: Vinyl Brake, Toggle, Drums and Bass` | `Pad 1 - Vinyl Brake (Toggle, Drums + Bass). Press to trigger; press again to release.` |
| Pad trigger, active | Button | same, description `Active` | same |
| Pad trigger, muting | Button | same, description `Muting` | same + `Press the pad to release the mute.` |
| Mode badge | ignored | - | full mode sentence from §3.3 |
| Muting marker | ignored | - | `This pad is holding its target stems muted. Press the pad to release.` |
| Stem chip | CheckBox | `Pad 1 target Drums` | `Include or exclude drums in pad 1` |
| Shortcut | Button | `Set pad 1 target to All` | `Set pad 1 to all stems` |
| Edit rail key | Button, checkable | `Edit pad 1 effect, mode and parameters` | `Edit pad 1` / `Done editing pad 1` |
| Effect selector | ComboBox | `Pad 1 effect` | `Effect for pad 1` |
| Mode selector | ComboBox | `Pad 1 mode` | `Press behavior for pad 1` |
| Slider | Slider | `Pad 1 Echo Time` | `Echo Time for pad 1` |
| Done key | Button | `Done editing pad 2` | `Return pad 2 to perform view` |
| Empty-state key | Button | `Separate stems for deck 1` | `Start stem separation for the loaded track` |
| Page key | Button | `Show decks 3 and 4` | `Switch to the other deck page` |
| Close | Button | `Close Pad FX` | `Close Pad FX` |

The pad button, chips, shortcut keys, edit key and page key are all at least 24 px in both
dimensions: `padfxHitHeight` is 25 at the default scale and floors at 24, and the pad trigger is
72 high. The slider row is 26 high (24 floor). No control relies on color alone: every chip and key
has a text label.

## 9. Rejected alternatives

| Alternative | Why rejected |
|---|---|
| Deck-level `Edit` toggle that expands all four pads | Four expanded editors do not fit 720 px, so the user would scroll to compare pads; the perform view would lose its "four pads at a glance" purpose. |
| Always-visible combo boxes and sliders (current) | Mixes configuration with performance and leaves only 150 px for the actual pad; this is the problem F06 exists to fix. |
| A second window per deck | Splits one performance surface into four; paging and one window are enough. |
| Bank selector as a ComboBox | Hides the A-D state and costs a click; four keys show the current bank at a glance. |
| Single-letter stem keys `D B O V` | Ambiguous and below the 24 px target; labelled colored chips are the point of the change. |
| Writing `padN_active`/`padN_muted` from QML | Both are read-only backend state; the UI must only render them. |
| Using `padN_active` as the mute indicator | Wrong state: a one-shot's active bit drops while `padN_muted` stays 1. The amber marker must follow `padN_muted`. |
| Text `ON/OFF` mode labels | They do not say whether a press holds, latches or fires once; the H/L/1× badge plus tooltip does. |
| New backend "separate this deck" control | `Mixxx.StemService.generateStems(track)` already exists and is the honest path. |
| Keep four columns at every width | Below 1240 the chips fall under 64 px and the labels clip; two-up paging keeps every target readable. |
| Always show `padfxDeckMinWidth` at 300 even with two decks | Two decks have room for 608 px columns; the column should grow and give the effect names and chips more space. |

## 10. Approval checklist

- [ ] Approve the perform-first card and per-pad disclosure (the recommendation at the top).
- [ ] Approve the five proposed Theme metrics (or name substitutions).
- [ ] Approve passing `deckCount` from `MainWindow.show4decks` at the existing PadFxWindow
      instantiation.
- [ ] Confirm no backend change is expected; the mock only reads `padN_active`/`padN_muted` and
      writes `bank`, `padN`, `effectN`, `targetN`, `modeN` and `padN_paramN` in the existing schema.
