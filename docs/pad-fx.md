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

Every pad is **momentary**. Hold the pad to engage the effect; release it to
stop. There is no toggle, no one-shot mode and no pad-owned mute.

Vinyl Brake and Echo Out are release effects:

- **Vinyl Brake** ramps the target to a stop while held. Releasing it winds the
  target back up.
- **Echo Out** feeds the target into the delay while held. Releasing it stops
  the feed and lets the effect release.

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
  fx1_pad1_param1..fx1_pad8_param8
  fx2_effect1..fx2_effect8
  fx2_target1..fx2_target8
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
presses send `1`; releases send `0`. Every press is momentary.

Example MIDI input:

```xml
<control>
  <group>[Channel1_PadFX]</group>
  <key>pad1</key>
  <status>0x90</status>
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
- Old mode values are ignored; every pad is momentary.
- Deck 2-4 pad configurations are discarded because assignments are shared.

## Limitations

- Only the active bank is loaded on a deck. Switching banks stops active pads
  and restores any mute they owned.
- A pad targeting several stems loads one effect instance per stem. Deck
  targeting adds one more instance.
- Only the eight exposed parameter values are stored per pad.
