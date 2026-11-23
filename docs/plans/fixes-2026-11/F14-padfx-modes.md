# F14 - Pad FX: stuck / "Off but plays" pads, per-pad momentary or toggle

- **Branch:** `bradley/fix-padfx-modes`
- **Base:** local `main` at `3616c75` (shared momentary Pad FX). That commit is not on `origin/main` yet; push it or stack on it.
- **Needs its own build:** yes (C++ in `src/effects/padfx/`)
- **Blocks:** F15 only for `PadFxPadFace.qml` (do F14's QML edits first, or let F15 own that file and rebase)

## Owned files

- `src/effects/padfx/padfxdefs.h`, `padfxpreset.{h,cpp}`, `padfxunit.{h,cpp}`
- `src/controllers/controlpickermenu.cpp` (Pad FX entries only)
- `res/qml/PadFxPadEditor.qml` (mode switch), `res/qml/PadFxPadFace.qml` (tooltip / accessible text only)
- `src/test/padfxtest.cpp`
- `docs/pad-fx.md`, `docs/adr/0007-pad-fx.md`

## Reported symptoms (owner, 2026-11-22)

1. A pad **stays engaged** after it is let go (mouse or MIDI).
2. A pad **shows "Off" (or the wrong name) but still plays** an effect.

## What reading the code found

### "Off but plays": two resolvers disagree (confirmed by reading)

- The UI name comes from `[PadFX],fxB_effectN`, an index set by `PadFxPreset::effectIndexForId()` (`padfxpreset.cpp`). An id that is not in `m_effects` gets **index 0 = "Off"**.
- The engine loads through `PadFxUnit::manifestForId()` (`padfxunit.cpp:224-237`). That function falls back to `getBackendManager()->getManifestFromUniqueId(effectId)`, so the **same id still loads and plays**.
- The id is missing from the catalog when an LV2/backend effect is not listed by `getPadEffectManifests()`, when the stored id is stale, or briefly while the catalog is being rebuilt.

### "Stuck on": not reproduced from reading; suspects in order

1. **MIDI release never reaches `padN`.** `docs/pad-fx.md` only shows a `0x90` input. Controllers that release with `0x80` note-off need a second `<control>` entry. Without it the control stays at 1 and the pad stays engaged. This fits a pad that sticks from the controller but not from the mouse.
2. **Runtime reset while a pad is held.** `stopAllPads()` and `slotPresetPadChanged()` zero `m_runtime[pad]` while `[ChannelN_PadFX],padN` is still 1. These run on a bank switch, a track load, a stem-count change, or any param/target edit from the editor or a controller. Afterwards the runtime and the control disagree. With toggle mode added this can latch the wrong way, so it has to be fixed whatever the bug turns out to be.
3. **Release dropped by the early return.** `slotPadChanged()` (`padfxunit.cpp:424-427`) returns before the release branch when the effect is empty or the target is unavailable. Today a reset usually runs first, but a release must never depend on the current preset.
4. **Mouse path.** `PadFxPadFace` writes `padN` from `onPressedChanged`. The deck pane is only hidden, never destroyed (`Deck.qml:422`), and `AbstractButton` ungrabs on hide. Closing the Pad FX **window** while a pad is held is untested.

Ruled out: Vinyl Brake and Echo Out stay slot-enabled while idle but pass dry audio through (`dryMix` = 1, wet gain 0). Pad chains are not written to effects.xml, so there is no stale restored state. The deck pane draws all 8 pads.

## Design: per-pad mode

- `PadConfig::mode`: `0` = Momentary, `1` = Toggle. Stored as `"mode"` in the shared JSON.
- Default by effect, in `defaultPadParams`' sibling `defaultPadMode(effectId)`. **Release FX are Toggle**: `isPadFxOnly()` covers Vinyl Brake and Echo Out. Everything else is Momentary. Picking a new effect resets the mode to that effect's default, as `setEffectDefaults()` already does for params.
- Migration: a pad saved without `"mode"` takes its effect's default. Legacy v1 per-deck modes stay ignored, as `3616c75` decided.
- Control: `[PadFX],fxB_modeN` (`ControlPushButton`, Toggle button mode) so a controller can flip it. It is shared like the other assignment controls and listed in the control picker under Pad FX Presets.
- Engine (`PadFxUnit::slotPadChanged`):
  - Momentary: unchanged. Press engages, release stops.
  - Toggle: each **press edge** flips `m_runtime[pad].active`; releases are ignored. `padN_active` shows the latched state, so the controller LED and the face highlight show it with no new indicator.
  - Release FX in Toggle: the first press brakes or captures; the second press winds up or returns to dry.
- Editor: a Momentary / Toggle segmented key in `PadFxPadEditor.qml` bound to `fxB_modeN`. Per the owner's answer the pad face gets **no** mode badge. The tooltip and accessible text say "Tap to toggle" or "Hold to engage".

## Steps

1. **Repro tests first** (`padfxtest.cpp`), red before any fix:
   - An id the catalog lacks → `fxB_effectN` reads 0 **and** the pad slot does not load on press.
   - Press, then a bank change / track load / param edit, then release → `padN_active == 0` and the slot is inactive, with no stale latch.
   - Press, then the effect goes to Off, then release → the slot is inactive.
2. **One resolver.** `PadFxUnit::manifestForId()` resolves only against `m_pPreset->effectManifests()`; delete the backend fallback. An unknown id behaves as Off in both the UI and the engine. Keep the id in the config so the pad comes back when the backend reappears; `slotVisibleEffectsChanged` already reloads all pads.
3. **Release is unconditional.** In `slotPadChanged()`, handle `!down && m_runtime[pad].active` (momentary) before the effect/target early return.
4. **Param edits don't stop a pad.** In `PadFxPreset::slotParamChanged()`, emit a new `padParamsChanged(bank, pad)`. `PadFxUnit` answers it with `applyPadParameters(pad)` and skips the stop/unload/reload. Effect and target changes still reload. Without this, a latched toggle pad drops every time a knob is turned in the editor.
5. **Reset keeps the held state.** `stopAllPads()` and `slotPresetPadChanged()` reset `active` but set `m_runtime[pad].down` from `m_pPads[pad]->toBool()`. A still-held pad then needs a real release and a new press, and a toggle latch never inverts.
6. **Mode** as designed above: defs, preset JSON + control + migration, unit logic, editor key, control picker.
7. **Docs:** in `pad-fx.md` replace "Every pad is momentary", add the mode table, the `fxB_modeN` control and a `0x80` note-off example. Amend ADR 0007 (it records "no toggle"; this reverses that for release FX).
8. **Manual check on the owner's controller.** If pads still stick from MIDI after step 3, log `padN` values in `slotPadChanged` and check which release message the mapping drops.

## Verification

- New and existing `PadFxTest.*` and `BeatFxTest.*` pass.
- Manual: Vinyl Brake and Echo Out pads latch on a tap and release on a second tap. A generic effect (e.g. Flanger) is momentary. Flipping `fxB_modeN` from the editor and from a mapped controller both work, and the LED follows the latch.
- An effect removed from the catalog shows Off and stays silent.

## Notes, not in scope

- Echo Out ramps back to dry within `kDryWetRampSeconds` once it is released, so the repeats stop instead of decaying. If you want the tail to ring out after the second toggle press, that is a separate DSP change.
