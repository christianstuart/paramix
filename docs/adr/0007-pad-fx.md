# ADR-0007: Pad FX presets

**Status:** Accepted
**Date:** 2026-11-21

## Context

Paramix already has standard effect units, one Quick Effect chain per deck and
per stem, and Beat FX. All of them are useful for continuous effects, but they
are not a convenient performance surface for eight saved effects per deck.

The original per-stem Pad FX implementation exposed four banks of four pads per
deck and only two pad-only effects. That made Pad FX a separate effect
catalogue and limited them to tracks with stems. It also meant every deck had
to duplicate the same pad assignments.

## Decision

Make Pad FX a shared preset layer over the existing effects system.

1. One global `PadFxPreset` owns two banks of eight pads in `[PadFX]`. Every
   deck reads the same assignments. Each deck keeps its own active bank
   (`[ChannelN_PadFX] bank`), pad triggers and active LEDs.
2. A pad stores an effect manifest ID and up to eight normalized parameter
   values. The chooser contains Vinyl Brake, Echo Out and every other effect
   manifest available from the backends. Nothing is reimplemented as
   Pad-FX-only DSP.
3. `PadEffectChain` exposes eight slots and is created for each stem and for
   the deck itself. Deck-targeted pads run after pregain, so they work on
   ordinary tracks. Stem-targeted pads still run before the stem fader.
4. Targets are `0` for the whole deck and `1..15` for a stem mask.
5. Each pad is **momentary** (held) or **toggle** (tap on, tap off), stored per
   pad as `[PadFX],fxB_modeN`. Release effects (Vinyl Brake, Echo Out) default
   to toggle; every other effect defaults to momentary, and choosing a new
   effect resets the mode to that effect's default. There are no pad timers or
   pad-owned mutes. *Amended 2026-11-22 (F14): the first version made every pad
   momentary; toggle came back for release effects, which are hard to hold
   through a brake or an echo tail on a controller.*
6. The deck pane and the Pad FX window show the same 4 x 2 grid. The Pad FX tab
   selects FX 1 on click and FX 2 on shift-click.
7. The old per-deck A-D banks migrate from Deck 1: A+B become FX 1 pads 1-8,
   C+D become FX 2 pads 1-8. Old mode values are ignored; migrated pads take
   their effect's default mode.

## Consequences

- Effect, target and parameter mappings move from `[ChannelN_PadFX]` to the
  shared `[PadFX]` group. Pad triggers and bank keys stay per deck.
- Every deck can use the same eight-pad performance layout without repeating
  the setup.
- Memory use grows: each deck can have eight pad slots per stem plus eight
  deck slots. Only the active bank is loaded, and only targeted stems allocate
  effect instances.
- Deck-targeted pads work without stems; stem-targeted pads remain inactive on
  decks with `stem_count` 0.
- Pad-only effects are no longer hidden from the regular effect selector's
  metadata; they are simply not added to the normal visible-effects list.

## Alternatives rejected

- **Keep per-deck assignments and mirror them**: duplicate state and drift when
  a deck is added late or a write fails.
- **Keep four banks of four**: does not give the requested Cue-like 8-pad
  layout or the FX 1 / FX 2 split.
- **Reuse a standard effect unit for deck targets**: consumes a mixer FX slot
  and shares its parameter controls.
- **Reuse the Quick Effect chain**: overwrites the user's Quick Effect setup.
- **Write a separate DSP graph for every pad effect**: duplicates the existing
  effect framework and its parameter handling.
