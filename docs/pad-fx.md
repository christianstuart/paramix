# Pad FX

Pad FX are eight effect presets per bank, triggered from a deck. The
assignments are **shared by every deck**: editing pad 3 changes pad 3 on all
decks. Each deck keeps its own active bank, pad triggers and active/mute state.

Each deck has two banks, **FX 1** and **FX 2**. Every bank is an 8-pad 4 x 2
grid, the same shape as the Cue pane. Clicking the deck's Pad FX tab opens
FX 1; shift-clicking it opens FX 2 for that deck. The Pad FX window has the
same grid per deck and a `FX 1` / `FX 2` key with the same click and
shift-click behaviour.

## Effects

A pad stores an effect and its parameter values. The effect list contains
Vinyl Brake, Echo Out and every other effect manifest available from the
effect backends. Pad FX use the existing effect chains: a pad loads the
selected manifest into the pad's slot and enables it. No effect DSP is
duplicated.

Up to eight normalized parameter values are stored per pad. The editor labels
them from the selected effect's manifest. Vinyl Brake and Echo Out keep their
original labelled parameters. A Beat FX also gets its quantized step grid, so
the pad exposes the same beat-step control as the top effect chain.

The effect dropdown uses the model's `display` role, so every entry has a
visible label.

## Behavior

Each pad is either **momentary** or **toggle**. The pad editor (right click)
has a Momentary / Toggle switch, and `[PadFX],fxB_modeN` sets it from a
controller.

| Mode | Press | Release |
|---|---|---|
| Momentary | engage | stop |
| Toggle | engage, or stop if already engaged | ignored |

The default depends on the effect, and choosing a new effect resets the mode:

| Effect | Default |
|---|---|
| Vinyl Brake, Echo Out (release effects) | Toggle |
| Every other effect | Momentary |

The pad lights (and `padN_active` is 1) while the effect is engaged, so a
latched toggle pad stays lit. There is no one-shot mode and no pad-owned mute.

- **Vinyl Brake** ramps the target to a stop while engaged. Stopping it winds
  the target back up.
- **Echo Out** stops feeding the target into the delay while engaged, so the
  captured window repeats and decays. Stopping it returns to the dry signal.

Changing a parameter (from the editor or a controller) updates a playing pad
without stopping it. Changing its effect, target or mode stops it.

An effect the Pad FX list cannot show (for example a plugin that is no longer
installed) is treated as Off: the pad shows Off and plays nothing. The pad
keeps the assignment and plays it again once the effect is available.

## Targets

| Target | Value |
|---|---|
| Deck (whole deck) | 0 |
| Drums | 1 |
| Bass | 2 |
| Other | 4 |
| Vocals | 8 |
| Instrumental (drums + bass + other) | 7 |
| All stems | 15 |

The `Deck` target works on tracks without separated stems. Stem targets only
play on a deck whose `stem_count` is positive; the pad keeps its assignment
either way.

## Control mapping

Shared assignments live in one global group:

```text
[PadFX]
  fx1_effect1..fx1_effect8       effect index, 0 = Off
  fx1_target1..fx1_target8       0 = deck, 1..15 = stem mask
  fx1_mode1..fx1_mode8           0 = momentary, 1 = toggle (a press flips it)
  fx1_pad1_param1..fx1_pad8_param8
  fx2_effect1..fx2_effect8
  fx2_target1..fx2_target8
  fx2_mode1..fx2_mode8
  fx2_pad1_param1..fx2_pad8_param8
```

Each deck owns its performance state:

```text
[ChannelN_PadFX]
  bank                           1 = FX 1, 2 = FX 2
  pad1..pad8                     level triggers
  pad1_active..pad8_active       read-only
```

A controller mapping only needs `bank` and `pad1..pad8` for performance. Pad
presses send `1`; releases send `0`. Send both even for toggle pads: the
toggle happens in Paramix, so the mapping never needs to know the mode.

If your controller releases with a note-off (`0x80`) rather than a note-on with
velocity 0, map the release too. Without it the pad never sees `0` and a
momentary pad stays engaged.

Example MIDI input:

```xml
<control>
  <group>[Channel1_PadFX]</group>
  <key>pad1</key>
  <status>0x90</status>
  <midino>0x30</midino>
  <options><normal/></options>
</control>
<!-- Release as note-off: same pad, status 0x80 -->
<control>
  <group>[Channel1_PadFX]</group>
  <key>pad1</key>
  <status>0x80</status>
  <midino>0x30</midino>
  <options><normal/></options>
</control>
```

Example LED output:

```xml
<output>
  <group>[Channel1_PadFX]</group>
  <key>pad1_active</key>
  <status>0x90</status>
  <midino>0x30</midino>
  <on>0x7F</on>
  <off>0x00</off>
</output>
```

Effect, target and parameter controls are shared: one mapping changes the
preset on every deck. The Pad FX entries appear in the MIDI Learning
wizard under **Effects → Pad FX Presets** and **Effects → Deck N Pad FX**.

## Configuration

The shared preset is saved in `mixxx.cfg` under `[PadFX]` as versioned JSON.
Each deck's active bank is saved separately.

On upgrade from the old per-deck banks:

- Deck 1's old configuration becomes the shared preset.
- Old banks A + B become FX 1 pads 1-8.
- Old banks C + D become FX 2 pads 1-8.
- Old mode values are ignored; each pad takes its effect's default mode.
  Presets saved before modes existed (version 3) do the same.
- Deck 2-4 pad configurations are discarded because assignments are shared.

## Limitations

- Only the active bank is loaded on a deck. Switching banks stops active pads
  and restores any mute they owned.
- A pad targeting several stems loads one effect instance per stem. Deck
  targeting adds one more instance.
- Only the eight exposed parameter values are stored per pad.
