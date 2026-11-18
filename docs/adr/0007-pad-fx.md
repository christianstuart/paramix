# ADR-0007: Per-stem Pad FX banks

**Status:** Proposed
**Date:** 2026-11-18

## Context

Paramix already has four standard effect units and one Quick Effect chain per
deck and per stem. Both are useful for continuous effects, but they are not
good fits for performance-pad gestures such as a vinyl brake or an echo out:

- The standard units have one set of parameter controls shared by every input
  assigned to them, and using one for a pad gesture would consume a mixer FX
  slot.
- The Quick Effect chain is a single user-assigned chain per stem. Reusing it
  would overwrite the user's Quick Effect setup and there is no bank concept.
- `engine.brake()` is a controller-script transport ramp. It drives the deck's
  `scratch2` rate, and `EngineBuffer` produces all four stems from one read
  cursor. It therefore cannot slow one stem while the others keep playing.

A per-stem pad gesture needs its own signal path, its own controls, and DSP
that can operate on one stem without touching the deck transport.

## Decision

Add a **Pad FX** layer between the stem reader and the existing per-stem Quick
Effect / gain stage.

1. A new `SignalProcessingStage::PadFx` and a `PadEffectChain` per stem
   (`[PadEffectRack1_[ChannelN_StemM]]`) with four slots. Pad FX are processed
   in `EngineDeck::processStem()` before the stem fader and Quick FX, so the
   stem fader/mute and Quick FX shape the pad result.
2. A `PadFxManager`/`PadFxUnit` control layer exposing one stable group per
   deck (`[ChannelN_PadFX]`). A bank is a page of four independent pad effects;
   each pad has its own effect, stem mask, mode, and parameters. One bank is
   active per deck, and only its effects are loaded into the engine.
3. Two pad-only built-in effects:
   - `Vinyl Brake` — an audio-domain tape stop using a ring buffer whose read
     rate ramps from 1 to 0. The deck transport and beatgrid continue.
   - `Echo Out` — a delay line that is continuously pre-filled, gated by an
     `active` parameter, and removes the dry signal while active.
4. Pad gestures end by setting the native stem `mute` for the affected stems,
   so the mixer state reflects the result. The pad remembers the previous mute
   state and restores it when the gesture is toggled off or cleared.
5. Pad-only effects are hidden from the regular effect selector and preset
   generation via `EffectManifest::isPadFxOnly()`.
6. No controller-specific script is added. Controllers bind the stable
   `[ChannelN_PadFX]` controls through MIDI Learn or XML, and the group is
   documented.

## Consequences

- Pad FX are independent of the four standard effect units and the per-stem
  Quick Effects. All three can be used together.
- The brake is a signal-domain tape stop, not a transport stop. Releasing it
  crossfades back to the live position; the deck does not jump back to the
  position where the brake started.
- Only the active bank is loaded, which bounds memory. The maximum is four
  pads times four stems per deck; typical configurations use four to eight
  effect instances per deck.
- Pad gestures intentionally change the stem mute state after the audible
  gesture. This is what makes an echo out stay out and a brake stay stopped.
  The saved mute state is restored when the pad is released.
- Non-stem tracks unload all pad effects and disable the Pad FX controls.

## Alternatives rejected

- **Per-stem transport varispeed**: requires independent read cursors in
  `EngineBuffer`, resampling, and independent loop/beatgrid state. It would
  desynchronize stems and is too invasive for this feature.
- **Muting all but one stem and using `engine.brake()`**: only works if the
  other stems are meant to be inaudible. It cannot brake vocals while the
  instrumental continues.
- **Reusing the standard effect units**: shared parameter controls, only four
  units, and a direct conflict with the mixer FX workflow.
- **Reusing the per-stem Quick Effect chain**: destroys the user's Quick
  Effect assignment and has no bank or pad gesture model.
- **A direct DSP block in `EngineDeck::processStem()`**: simpler, but loses
  effect lifecycle management, parameter smoothing, and future effect
  extensibility. The PadEffectChain reuses the proven effects framework while
  keeping a separate processing stage.
