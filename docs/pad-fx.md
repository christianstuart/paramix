# Per-stem Pad FX

Paramix adds four **Pad FX banks** per deck. A bank is a page of four
independent pad effects. Each pad has its own effect, stem target, mode, and
parameters, so a bank can contain, for example:

| Pad | Effect | Target |
|---|---|---|
| 1 | Vinyl Brake | Instrumental (drums + bass + other) |
| 2 | Vinyl Brake | Vocals |
| 3 | Echo Out | Instrumental |
| 4 | Echo Out | Vocals |

Only one bank is active per deck. Switching banks unloads the previous bank's
effects and loads the new bank's effects. Pad FX are separate from the four
standard effect units and from the per-stem Quick Effects.

## Behavior

### Vinyl Brake (default: Toggle)

1. Press the pad. The affected stems slow down like a turntable losing power.
2. When the brake reaches zero, the pad sets the stems' native **Mute**. The
   pad stays lit while it owns the mute.
3. Press the pad again. The previous mute states are restored and the stems
   spin back up over the configured wind-up time before returning to live
   audio.

Because Pad FX run before the stem fader and mute, the mute is applied only
after the brake ramp, so the brake sound itself is audible.

The brake is an audio-domain tape stop. The deck transport and beatgrid keep
running. Releasing the brake crossfades back to the live position; it does not
rewind to where the brake started.

### Echo Out (default: One Shot)

1. Press the pad. The dry signal is cut and the stem repeats into the delay
   line. The delay line is continuously pre-filled, so the first repeat is
   immediate.
2. The echo tail runs through the plate reverb using the configured echo and
   reverb times.
3. When the configured tail ends, the pad sets the affected stems' native
   **Mute** and releases. The pad LED turns off.
4. Pressing the same pad again clears the pad-owned mute without triggering a
   second echo.

### Momentary mode

A pad can be set to **Momentary** to engage only while held. Momentary pads do
not set a native mute; releasing the pad returns to the dry signal.

### Target masks

| Target | Mask |
|---|---|
| Drums (stem 1) | 1 |
| Bass (stem 2) | 2 |
| Other (stem 3) | 4 |
| Vocals (stem 4) | 8 |
| Instrumental (1 + 2 + 3) | 7 |
| Vocals | 8 |
| All | 15 |

Use the Pad pane to tick the stems for each pad. Pressing a pad that targets
several stems applies the effect to each of them independently.

## Software controls

Pad FX live in a dedicated window, closed by default. Open it from the
**Pad FX** toolbar key or **View → Pad FX**. The window has one column per deck,
so all four decks are addressable at once; each column has its own A–D bank
selector, four pad cards, stem target toggles, effect and mode selectors, and
parameter knobs. Pad FX are disabled for decks without a stem track.

## Control mapping

Every deck exposes one stable control group:

```
[Channel1_PadFX]
  bank                1..4        active bank (A-D)
  pad1..pad4          0/1         level triggers for the active bank
  pad1_active..pad4_active        read-only pad state for LEDs
  effect1..effect4    0..2        Off / Vinyl Brake / Echo Out
  target1..target4    1..15       stem mask for each pad
  mode1..mode4        0..2        Momentary / Toggle / One Shot
  pad1_param1..pad1_param5        normalized effect parameters
  pad2_param1..pad2_param5
  pad3_param1..pad3_param5
  pad4_param1..pad4_param5
```

Parameter meaning:

- Vinyl Brake: `param1` = wind-down time, `param2` = curve, `param3` =
  wind-up time.
- Echo Out: `param1` = delay in beats, `param2` = feedback, `param3` = tail
  in beats, `param4` = reverb send, `param5` = reverb decay time.

A controller mapping only needs `bank` and `pad1..pad4`. A pad press sends
`1`; release sends `0`. The pad layer decides whether that is a toggle, a
momentary hold, or a one-shot.

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

The Pad FX entries also appear in the MIDI Learning wizard under
**Effects → Deck N Pad FX**.

## Configuration

Pad FX banks are saved in `mixxx.cfg` under `[PadFX]`, keyed by deck. The format
is versioned JSON internal to Paramix; use the Pad pane rather than editing it
by hand. Changing a bank, effect, target, mode, or parameter is saved
immediately.

## Limitations

- Pad FX are disabled for non-stem tracks.
- Only the active bank is loaded. Effects in other banks do not run, and
  switching banks cancels any gesture in progress.
- A pad effect targeting several stems allocates one effect instance per stem.
  Keep target masks focused if memory use matters on large stem sets.
