# ADR-006: Stem detail renderer

- **Status:** Proposed
- **Date:** 2026-11-17
- **Deciders:** Paramix maintainers
- **Supersedes:** none (replaces the heuristic implementation inside
  `allshader::WaveformRendererStem`)
- **Source material:** `~/Developer/stemwave/spec/stem-waveform-spec.md` §28, §34, §35, §36;
  `docs/paramix-stem-detail-plan.md`
- **Related:** ADR-002 (stem cache format), ADR-003 (grid format)

---

## Decision summary

1. Render the four stem lanes from **decoded audio**, not from the waveform summary. The
   reducer and the colour consumer are a direct port of the verified stemwave
   implementation, including all constants and the order of float operations.
2. Keep the algorithm in a self-contained module, `src/waveform/stemdetail/`, instead of
   depending on the stemwave repository.
3. Reduce on a **worker thread** (`StemDetailBuilder`), publish an immutable
   `std::shared_ptr<const StemDetailTrack>`, and cache at most two pictures in
   `StemDetailCache`, owned by `WaveformWidgetFactory`.
4. Emit the picture with **geometry only** (`rendergraph::GeometryNode` +
   `RGBAVertexUpdater`): one opaque `#181818` lane base, then one rectangle per contiguous
   row run of every column. No textures and no GL calls, so the same source compiles for
   the GL rendergraph and the QML scene-graph backend.
5. A stem track always shows **four lanes**, whatever `splitStemTracks` says. A track
   without stem audio keeps the regular waveform renderer and its existing picture.
6. Keep the analysis format unchanged. `WaveformStemColumn` and the analyzer still feed the
   other renderers and the library.

---

## 1. Context

`allshader::WaveformRendererStem` synthesized the Traktor look from the per-stem
peak/RMS/packed-band summary. That pipeline had a hand-tuned HSV family palette
(`kStemLowHue`, `kStemFamilyThrow`, `smoothStep`, per-lane fill normalisation) and never
matched the shipping picture in quiet passages or at high zoom.

The stemwave project reverse-engineered the shipping pipeline to the pixel: a stateful
four-lane state-variable filter and envelope bank turn PCM into a 21-row image of four
floats per column, and a colour consumer maps each entry to one `0xffRRGGBB` pixel. The
port was verified bit-exactly against 226,000 live pixels (225,999 exact; the one
exception is a pixel in a beat-grid overlay column).

Paramix's own stems are an 8-channel FLAC cache (`stems.flac`, drums/bass/other/vocals
interleaved) produced next to the user-facing four-file export. The deck, analyzer and
`SoundSourceProxy` already resolve that cache for an 8-channel request, and Traktor
`.stem.mp4` files use the same interleaved layout. The builder therefore reads both native
sources through the analyzer's open/read pattern.

## 2. The ported algorithm

Per column of `samplesPerColumn` frames and per stem:

- `x = (L + R) * 0.5 * magnification`, with
  `magnification = 1 / (gainDb / 24 + 0.5)` (2.0 at 0 dB).
- Four Chamberlin SVF lanes, `a[k] = 2*pi*f[k]/fs`, damping `d = 2`,
  `f = {150, 671, 3000, 671} Hz`; lane 3's `b` state is replaced by lane 1's shipped
  quirk.
- Envelope followers with attack/release `exp(-1/(fs*tau))` and
  `tau = {0.01, 0.1, 0.005, 0.035} s`.
- Entry weights `{1.0, 0.6, 1.0, 1.2}` and vertical inset `1`.
- Column value = current split plus the previous column's interpolation tail, divided by
  `samplesPerColumn`.

Per entry, colour mode 3 (Spectrum):

- `s = 0.01 + low + mid + high`, `w = band * 3.5 / s`, `lv = coverage * 3.5 * 255`.
- Alpha from the `pow(n/255, 1/3.5)` LUT with `LUT[0] = 0`.
- `R/G/B = 255 * (w * alpha + 24/255 * (1 - alpha))`, opaque pixel.

The empty cell is the **zero** entry. `StemDetailEntry` defaults `coverage` to 1.0, so a
zero entry must be constructed explicitly; otherwise every cell outside the band renders
black instead of `#181818`.

## 3. Provenance addresses

| element | address |
|---|---|
| `WaveformDataGenerator` | `0x100adb1e0` |
| splitter | `0x100adca80` |
| column assembly | `0x100adc870` |
| colour consumer, gain and base | `0x10087b600` (`+0x240`, `+0x228`) |
| colour selection | `0x10087ba60` |
| Spectrum functor | `0x100adcdf0` |
| pixel write | `0x100885ff0` |
| alpha epsilon 0.01 | `0x103496b20` |
| high-band scale 1.0 in mode 3 | `0x10352c8b4` |
| vertical inset | `0x1008783a0` |

## 4. Rendering and cache decisions

- **Key:** `(trackId, sampleRate, samplesPerColumn, laneDeviceHeight, magnification)`.
  `samplesPerColumn` is the frame count per device pixel of the viewport, rounded.
- **Value:** four lanes of `StemDetailColumn { rgba, mask, wave }`. A non-uniform column
  additionally stores the exact per-row `QRgb` colours in a flat lane arena.
- **Memory:** the compact form is 16 bytes per column per lane; the per-row arena is
  `height * 4` bytes per column per lane and is allocated on the first non-uniform column
  of a lane. Loud material is non-uniform in most columns, so the arena is the normal case,
  not a rare exception. A full-track reduction is capped at 600,000 columns; deeper zoom
  levels keep the previous picture until the windowed rebuild (plan M5).
- **Threading:** the builder lives on its own `QThread`. `request()` may be called from a
  render thread; it only reads the cache under a mutex and posts the debounced schedule
  onto the cache's thread. Muting, reordering and opacity never touch the cache.
- **Geometry:** device-pixel offsets are divided by `getDevicePixelRatio()` before they are
  emitted. The lane strip is centered in `breadth`, with a one-logical-pixel gap. The base
  rectangle is clipped to the columns the picture actually covers, so pre/post-track stays
  window background.
- **Mute/solo:** both the base and the band rectangles are multiplied by
  `kGhostedStemOpacity` (0.32).
- **Opacity:** the shipped `stem_opacity` default (0.75) maps to a fully opaque lane so
  the picture keeps its exact `#181818` base and pixel colours. Lower values fade the
  lanes. `setOutlineOpacity()` and `setStemVisualGain()` are documented no-ops: modulating
  either would change the verified colour values.

## 5. Rejected alternatives

- **Render with a texture.** Traktor blits a precomputed image, but textures are not
  available in both rendergraph backends with one code path.
- **Keep the HSV family palette.** It is not the shipping algorithm and cannot reproduce
  quiet passages or per-column band colours.
- **Link against stemwave.** Paramix has its own build and Qt types; the algorithm is a
  small, testable module.
- **Store entries instead of pixels.** A non-uniform column has a different entry per row;
  storing the resulting `QRgb` per row is the compact exact form.
- **Change `splitStemTracks` into a one-lane stem view.** The existing preference means
  "stacked" in the rest of the renderer stack, and the detail picture is inherently four
  lanes. The preference therefore no longer changes the stem layout.

## 6. Consequences

- The stem picture is now independent of `WaveformStemColumn`. The analysis format and
  every other renderer stay untouched.
- The first build decodes the whole track on the worker thread (target < 2.5 s for a
  five-minute track); zoom rebuilds are debounced by 150 ms and keep the previous picture.
- Deep zoom on a very long track can exceed the column cap and keep the previous picture
  until M5 adds splitter checkpoints and window-only rebuilds.
- The QML `splitStemTracks`, `outlineOpacity` and `stemVisualGains` properties remain valid
  API but have no effect on the stem picture.

## 7. Verification

- 17 live `Entry -> pixel` rows from the shipping build, bit-exact.
- 30,000 live entry/pixel pairs from `spec/reference/color/live_entry_argb.txt`,
  `exact == total`.
- Composer rules: zero entry -> `#181818`; `{coverage = 1, bands = 0}` -> black.
- Reducer property: two silent columns give `coverage == 1` at the centre row and zero
  bands.
- Builder test: a native 8-channel Paramix cache is written with four known tones, decoded
  through `SoundSourceProxy`, and every row of every column is compared against a direct
  reducer/consumer reference, plus a golden fingerprint of the whole picture.
- `mixxx-test --gtest_filter='Stem*:*Waveform*'` and
  `ctest -R 'Stem|Waveform'` are green.
- Visual check on `buildenv/test-music/trance_ALAC_24bit.stem.mp4`: four lanes in the deck
  detail view, drums magenta/orange, bass orange, other green/cyan, vocals blue/cyan; the
  deck overview and non-stem tracks keep the regular waveform.
