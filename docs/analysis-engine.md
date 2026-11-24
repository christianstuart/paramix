# Paramix Analysis Engine

**Status:** opt-in beat detection plugin. **Plugin id:** `paramix-analysis-engine:1`.
**Detector name in Preferences:** "Paramix Analysis Engine".

This document describes the self-contained BPM, downbeat and constant-tempo
grid engine that ships in `lib/paramix-analysis/` and the thin Qt adapter in
`src/analyzer/plugins/analyzerparamixbeats.*`. It explains what the engine
does, how accurate it is, what it persists, and where its limits are.

The engine is **not** the default detector. Queen Mary remains the default.
Select "Paramix Analysis Engine" under **Preferences > Beat detection** to use
it. `main` behaves identically for anyone who does not select it.

---

## 1. What it is

For any track Paramix can decode, the engine produces:

- a raw, unsnapped BPM,
- the phase of the first downbeat (the grid anchor),
- a constant-tempo beat grid with `beatsPerBar = 4`, the downbeat phase and a
  confidence,
- internally, the transient list (positions, weight, bassWeight).

It never reads or writes Traktor cache files, never drives the Traktor UI, and
contains no Native Instruments code, binaries or assets. The behavior was
reverse engineered from a black-box reference and reimplemented from scratch.

It runs on the normal analysis worker thread and consumes the same
interleaved float stream as the other beat plugins. Analysis is offline; the
real-time engine path is untouched.

---

## 2. Provenance and scope

| Item | Status |
|---|---|
| Algorithms | Ported from the JavaScript reference in `C:\Users\chris\traktor-re` (`frontend_v10.js`, `bpm_sim.js`, `phase_exact.js`, `resampler_exact.js`) and the 174 ledger claims L-138 to L-174. |
| FFT | Portable double-precision radix-2 FFT with the reference's butterfly order. The Intel IPP/FMA kernel is deliberately **not** ported. |
| Record weights | `weight` and `bassWeight` can differ from Traktor's cache by 1-2 ULP because of the FFT difference. Positions, BPM and anchor do not depend on it. |
| Traktor interop | None by design: no cache reads or writes, no `.nml` access. |
| Dependencies | Zero new third-party dependencies. GPL-2.0 compatible. |
| Material | Constant tempo, 4/4 only. Variable tempo, odd meters and non-4/4 tracks are out of scope. |

---

## 3. Architecture

```text
lib/paramix-analysis/            Qt-free, C++20, no Mixxx dependencies
  include/paramix/analysis/      public facade (Engine, Result, Transient)
  src/window.*                   half-amplitude Hann window, 512 points
  src/fft.*                      portable 512-point real FFT
  src/logmag.*                   log10(1+v) and the four-lane 80-bin sum
  src/onset.*                    fine/coarse rectified flux, envelope, bass
  src/refine.*                   43-frame threshold, 7-point gate, refinement
  src/fields.*                   weight and bassWeight finalize pass
  src/tempo.*                    three-pass variance tempo scan
  src/phase.*                    weighted comb phase and downbeat choice
  src/kernel.*                   Kaiser-windowed sinc, exact mode-6 taps
  src/resampler.*                streaming resampler to 22050 Hz
  src/engine.*                   streaming facade and finalize orchestration
  test/                          GTest fixtures, literals from the reference

src/analyzer/plugins/analyzerparamixbeats.*   Qt adapter (AnalyzerBeatsPlugin)
```

The internal analysis rate is 22050 Hz, the hop is 32 samples and the window
is 512 samples. The window ends at the current burst (it is zero padded before
the start of the track). Flux is the rectified log-magnitude difference over
bins 0-79, evaluated every burst (fine) and every eighth burst (coarse). The
coarse envelope is batched in groups of 43 frames; each batch is centered with
a `-1.25 * sum / 43` threshold and gated with a strict 7-point comparison.
Peaks are refined by an argmax over a +/-12-burst window of the fine onset
array. Positions are stored in seconds as `float32` values promoted to double.

The finalize pass distinguishes full batches from the partial last batch: the
partial batch uses a frame base one higher, matching the reference. Weight is
`1 / maxFlux * flux`; bassWeight is a +/-68-burst antisymmetric difference of
the log-magnitude bass envelope.

### Public API

```cpp
namespace paramix::analysis {
struct Transient { double positionSeconds; float weight; float bassWeight; };
struct Result {
    bool ok;
    float bpm;              // raw, unsnapped
    float anchorSeconds;    // first downbeat, the grid origin
    int beatsPerBar;        // 4
    int downbeatPhase;      // 0 for a freshly analyzed grid
    float confidence;       // heuristic, [0, 1]
    int bpmRangeMin;        // window that produced this result
    int bpmRangeMax;
    bool bpmHintMismatch;   // existing BPM not reproduced by any preset
    std::vector<Transient> transients;
};
class Engine {
  public:
    struct Config {
        int sampleRate;
        int channelCount;
        std::int64_t totalFrames;
        bool keepTransients;
        int bpmRangeMin;               // Traktor preset, default 88/175
        int bpmRangeMax;
        double existingBpm;            // 0 = none
        bool matchExistingBpm;         // try other presets to reproduce it
        bool preserveGridOnBpmMismatch;// leave ok=false instead of overwriting
    };
    explicit Engine(Config config);
    void pushInterleaved(const float* frames, int frameCount);
    Result finish();     // idempotent
    void reset();
};
}
```

Input is interleaved float at the source rate. Mono is passed through; stereo
is downmixed as `f32(f32(left) + f32(right)) * 0.5`; more than two channels
are averaged. The stream is resampled to 22050 Hz, analyzed incrementally, and
the tempo/phase stages run once in `finish()`.

---

## 4. Sample-rate and format matrix

Paramix already decodes MP3, AAC/M4A, WAV, AIFF, FLAC, OGG Vorbis, Opus, WV and
MOD through its existing sources. This engine adds no formats; it accepts
whatever the analysis stream delivers at 8, 11.025, 16, 22.05, 24, 32, 44.1,
48, 88.2, 96, 176.4 or 192 kHz, mono or stereo.

| Source rate | 22050/rate | Resampler path | BPM error | Anchor error |
|---|---|---|---|---|
| 8000 | 2.75625 | general polyphase (mode 1 equivalent) | 0.000107 | 1.702 ms |
| 11025 | 2.0 | general polyphase (mode 5 equivalent) | 0.000107 | 1.693 ms |
| 16000 | 1.378125 | general polyphase | 0.000107 | 1.687 ms |
| 22050 | 1.0 | copy (mode 2) | 0.000107 | 1.700 ms |
| 24000 | 0.91875 | general polyphase | 0.000107 | 1.687 ms |
| 32000 | 0.6890625 | general polyphase | 0.000107 | 1.692 ms |
| 44100 | 0.5 | exact mode-6 half-band (verified 4096/4096 bit-exact against the native oracle) | 0.000107 | 1.308 ms |
| 48000 | 0.459375 | general polyphase | 0.000107 | 1.692 ms |
| 88200 | 0.25 | cascaded exact mode-6 (mode 4 equivalent) | 0.000107 | 1.127 ms |
| 96000 | 0.2296875 | general polyphase | 0.000107 | 1.692 ms |
| 176400 | 0.125 | cascaded exact mode-6 (mode 4 equivalent) | 0.001846 | 1.211 ms |
| 192000 | 0.11484375 | general polyphase | 0.000107 | 1.692 ms |

Measured on a 30-second synthetic click track at 140 BPM with the downbeat at
0.25 s, one run per rate, mono. The anchor error is the distance to the
generator's downbeat phase; it is dominated by the detector's STFT window
alignment, not by the resampler. All values are far inside the 5 ms acceptance
limit, and every rate is inside the 0.002 BPM limit.

The exact mode-6 decimator uses the 16 native tap bit patterns and the
reference accumulation order. The general polyphase path uses the same
Kaiser-windowed sinc kernel (`beta = 8`, radius 8 in tap units), with the
cutoff at the lower of the two Nyquist frequencies and per-phase normalized
taps. Modes 4 (integer decimation) and 5 (2x interpolation) are realized with
the mathematically equivalent paths because their worker internals were not
recovered; the measured end-to-end deviations are the table above.

Mono handling: the analyzer stream for mono files may be duplicated to stereo
by `AudioSourceStereoProxy`; either way the engine uses the same downmix rule,
so a duplicated mono input is a no-op (`f32(2x) * 0.5 = x`). A dedicated mono
fixture covers this.

---

## 5. Accuracy

### 5.1 Against the reference on synthetic audio

A 30-second stereo click track at 120 BPM with a downbeat at 0.25 s, generated
at 44100 Hz and analyzed end to end:

| Quantity | Result |
|---|---|
| Transient count | 72 (reference: 72) |
| Transient positions | 72/72 bit-identical |
| BPM | `0x42f0002c` = 120.000336 (reference identical) |
| Anchor | 0.2499 s (generator phase 0.25 s; comb resolution 0.3 ms) |
| Onsets / envelope / bass | all values bit-identical |

The deterministic finalize pass and the three tempo passes reproduce the
reference bits on this fixture. The committed GTest literals in
`lib/paramix-analysis/test/literals.h` were generated once from the reference.

### 5.2 Against Traktor on the local reference tracks

The BPM is reproduced bit for bit from the decoded FLACs. The anchor is
compared against the AutoGrid marker Traktor itself stored in `collection.nml`
(`CUE_V2 NAME="AutoGrid" TYPE="4"`, milliseconds), which is the grid Traktor
shows to the user:

| Track | BPM (engine = reference) | Anchor (s) | Traktor AutoGrid (s) | Error | Transients |
|---|---|---|---|---|---|
| Lady Gaga - Poker Face | 119.001587 | 0.629459 | 0.627293 | +2.2 ms | 1151 (cache 1151) |
| Playboi Carti - Rockstar Made | 150.051865 | 0.029837 | 0.034810 | -5.0 ms | 1055 |
| Rihanna - S&M | 128.000870 | 1.569390 | 1.568879 | +0.5 ms | 1217 |
| JAY-Z - Run This Town | 173.555801 | 0.042166 | 0.042501 | -0.3 ms | 1669 |
| Tay-K - I <3 My Choppa | 159.995972 | 0.078240 | 0.080335 | -2.1 ms | 613 (cache 613) |
| Zeddy Will - Bow Bow Bow | 130.000122 | 0.032626 | 0.032167 | +0.5 ms | 757 (cache 757) |
| Young Thug - Millions | 99.000282 | 0.173548 | 0.173256 | +0.3 ms | 891 |
| Lil Skies - Welcome to the Rodeo | 170.000977 | 0.002614 | 0.002489 | +0.1 ms | 982 |
| TWOFACED - I Need You | 142.000900 | 0.027702 | 0.026931 | +0.8 ms | 988 |
| Justin Bieber - Sorry | 100.000778 | 0.161366 | 0.161799 | -0.4 ms | 1059 |
| JAY-Z - Ni**as In Paris | 139.999893 | 0.693054 | 0.699348 | -6.3 ms | 1271 |
| Ne-Yo - Time of Our Lives | 124.000595 | 1.090104 | 1.089592 | +0.5 ms | 1221 |
| Sexyy Red - Looking For The Hoes | 151.008881 | 0.343421 | 0.338915 | +4.5 ms | 717 |
| The Black Eyed Peas - Rock That Body | 125.000801 | 0.075312 | 0.075253 | +0.1 ms | 1617 (cache 1617) |

The transient counts for the three tracks with paired caches match exactly
(1151, 613, 757); Rock That Body matches 1617/1617 as well. Travis Scott -
SICKO MODE is discussed in section 6: the native cache analyzes to 154.963882
with every window, its stored 139 is not an analysis result, and its grid is
preserved.

The median anchor error is 0.5 ms. Nine tracks are within 1 ms, four are
between 2.1 and 5.0 ms (Poker Face +2.2, I <3 My Choppa -2.1, Looking For The
Hoes +4.5, Rockstar Made -5.0) and the worst case is 6.3 ms (Ni**as In Paris),
about 1.5% of a beat at 100 BPM. The BPM meets "at most 0.002" exactly.

### 5.3 Phase stage

The anchor is not a plain average of the detected transients: hi-hats and
vocals are dense enough to pull such an average up to half a beat off. The
phase stage instead scans the transient list for the offset that maximizes
transient support, with each transient weighted by its detected weight and a
linear taper over a 30 ms window, then refines the winner with the
triangular-window circular mean. The four downbeat candidates are still chosen
by bass salience, so the bar phase follows the kick.

Over the 14 tracks in section 5.2 this reproduces the AutoGrid marker to within
1 ms on 9 of them and 6.3 ms at worst. The transient average it replaced missed
by up to 370 ms (Rock That Body +0.37 s, Poker Face -0.22 s, Ni**as In Paris
-0.22 s, Sorry -0.15 s), which is what made grids look wrong in the deck.

### 5.4 Determinism

The engine is single-threaded and uses only `float`/`double` arithmetic with
no hash-order or address-dependent behavior. The determinism test analyzes the
same fixture three times and compares BPM, anchor, confidence, every position,
weight and bassWeight bit for bit.

---

## 6. BPM search range

Traktor does not scan a fixed window. The analysis dialog offers a list of
minimum/maximum pairs and the scan is centred on their midpoint with half width
`(max - min) / 2`. The engine reproduces the table exactly; it was read from the
native preset array (`0x140f48ed0`, ten 0x28-byte entries) and each entry was
verified against the native tempo chain:

| Preset | Window | Notes |
|---|---|---|
| 0 | 95 - 115 | |
| 1 | 58 - 115 | |
| 2 | 68 - 135 | |
| 3 | 78 - 155 | reproduces Rock That Body 125.000801 |
| 4 | 88 - 175 | installation default |
| 5 | 98 - 195 | |
| 6 | 108 - 215 | |
| 7 | 118 - 235 | |
| 8 | 128 - 255 | |
| 9 | 110 - 220 | labelled "Automatic"; never inferred from a stored BPM |

`bpmRangeForPreset()` maps a preset index to the pair; the engine derives
`centre = f32((min + max) * 0.5)` and `halfWidth = f32((max - min) * 0.5)` from
it. An out-of-range value falls back to preset 4. The window only changes which
tempo the three passes lock onto: the same transient list fed with another
window reproduces the native BPM for that window bit for bit.

Selecting the window matters: Rock That Body gives 166.667130 with 88-175 and
125.000801 with 78-155, and only the latter is native. That is why the same
engine looked "right" on 13 tracks and "wrong" on two.

### 6.1 Reproducing an existing BPM

A track that already carries a BPM was usually analyzed by Traktor with *some*
window, and that choice is not stored in the collection. Paramix therefore
offers **"Use an existing BPM to select the Traktor BPM range when
re-analyzing"** (`[BPM] MatchExistingBpmRange`, default on). With
`Config::existingBpm` set, the engine:

1. runs the configured window first, and accepts it when its BPM is within
   0.5 BPM of the existing value;
2. otherwise runs every fixed preset and picks the closest result;
3. accepts that result when it is within 0.5 BPM of the existing BPM;
4. otherwise reports `bpmHintMismatch`. If
   `Config::preserveGridOnBpmMismatch` is set (the analyzer sets it when the
   track already has a grid) the result is left invalid, `finalize()` returns
   false, and the existing grid is kept.

The 0.5 BPM window covers importers that round BPM to an integer: Run This Town
is stored as 174.0 but analyzes to 173.555801, and Rockstar Made is stored as
150.0 but analyzes to 150.051865. Both are accepted and replaced with the
Traktor-exact value. A deliberately halved or doubled BPM is far outside the
window and is left alone.

The tenth preset ("Automatic") is never inferred from a stored BPM. It stands
for a window Traktor chooses itself, so it carries no information about what
was used; including it would let its wide 110-220 window win the closeness
comparison by accident (it produced 124.999596 for Rock That Body when the
correct 78-155 window gives 125.000801).

Rock That Body (stored 125.0) selects 78-155 and reproduces
125.000801/1.884526134. SICKO MODE stores 139.000000, which no window
reproduces (the native cache analyzes to 154.963882 with every window from
68-135 upward), so the stored grid is preserved instead of being overwritten
with a value Traktor never wrote.

The hints are only available while the grid still exists: Mixxx's **Reanalyze**
action clears the beats before queuing the track, so that path always runs a
fresh analysis with the configured window. Both outcomes are logged:

```text
Paramix BPM hints: track 27 range 88 - 175 existing BPM 125 has grid true match existing true
Paramix Analysis Engine stored native grid: BPM 125 anchor 83094 beatsPerBar 4 phase 0 confidence 1
```

```text
Paramix BPM hints: track 28 range 88 - 175 existing BPM 139 has grid true match existing true
Paramix Analysis Engine kept the existing beat grid: no Traktor BPM range reproduced the stored BPM
```

---

## 7. Persistence

The native grid bypasses `BeatFactory::makePreferredBeats()`. That factory
re-derives a tempo region, snaps the BPM through a rounding ladder and
re-anchors the grid, all of which would discard the raw engine result.

`AnalyzerBeats::storeResults()` instead builds:

```cpp
mixxx::Beats::fromConstTempo(sampleRate, plugin.getFirstBeat(),
        mixxx::Bpm{plugin.getBpm()}, subVersion)
```

- `library.bpm` stores the raw float BPM.
- The grid is `BeatGrid-2.0` (unchanged version, so older builds still read
  it) with its terminal marker on the anchor.
- `beats_sub_version` carries the normal analyzer fragments plus:

```text
db=1|dbbar=4|dbphase=<0..3>|dbconf=<3 decimals>|dbsrc=paramix-analysis-engine:1|dbv=2
```

`Beats::getDownbeatInfo()` parses these fragments on construction, so the
downbeat metadata survives a restart and an unmodified build ignores it
without destroying the grid. `beats_version` is never bumped: an unknown
version is replaced with a frame-0 placeholder by the loader.

`AnalyzerBeats::shouldAnalyze()` ignores the `db*` fragments when checking
whether the settings changed, so a stored native grid is not re-analyzed on
every launch. The one exception is `dbv`: a grid written by the old anchor
stage (no `dbv=2`) is re-analyzed even when re-analysis is disabled, because
its phase could sit up to half a beat off the music.

The plugin appears in the existing Preferences detector list automatically
because the UI enumerates `AnalyzerBeats::availablePlugins()`. It is appended
last, so `defaultPlugin()` remains Queen Mary.

---

## 8. Confidence

The confidence is a heuristic, not a probability. The phase stage scores four
downbeat candidates. Let `best` and `second` be the winning and runner-up
scores:

```text
margin     = max(0, (best - second) / max(best, 1e-9))
confidence = min(1, 4 * margin) * min(1, transientCount / 100)
```

Short tracks are therefore penalized, and a close four-way race produces a low
value. On the local reference tracks the value is typically 1.0; on the 30 s
synthetic fixtures it is about 0.5. It is stored as three decimals in the
sub-version.

---

## 9. Operational notes

- **Fast analysis is ignored.** The engine needs the whole track for the
  global envelope and the finalize pass. When fast analysis is enabled while
  this plugin is selected, `AnalyzerBeats` logs that fact once and processes
  the full track. The `fast_analysis` fragment is not written for this plugin.
- **BPM lock is honored.** The existing `AnalyzerBeats` gate returns before
  the plugin is created for locked tracks.
- **Constant tempo only.** The grid is `BeatGrid-2.0`; no tempo map is
  produced. The BPM editor still works normally.
- **Stems.** `AnalyzerBeats` already reduces an 8-channel stem source to
  stereo before calling the plugin, so the engine always sees mono or stereo.
- **BPM range.** The window selected in Preferences (default 88-175) is used for
  this plugin only; Queen Mary and SoundTouch ignore it. The existing-BPM
  matching described in section 6 is applied when the track still has a grid.
- **No cache interop.** The engine does not read or write any Traktor file and
  does not use the Traktor peaks blob.

---

## 10. Tests

`lib/paramix-analysis/test/` contains GTest coverage for:

| Test file | Coverage |
|---|---|
| `frontend_test.cpp` | window, `log10(1+v)`, 512-point FFT magnitudes, fine/coarse flux, envelope, bass, 7-point gate, refinement, all against reference bits |
| `resampler_test.cpp` | exact mode-6 reference, copy mode, declared frame count for every rate, DC gain, impulse alignment, determinism |
| `tempo_phase_test.cpp` | three-pass tempo scan, circular mean, weighted comb phase, downbeat choice, perfect-grid behavior, explicit window, dense off-beat transients |
| `engine_test.cpp` | end-to-end synthetic click tracks for all 12 rate buckets, mono/stereo agreement, determinism over three runs, idempotent finish, empty input, transient retention, Traktor window selection, existing-BPM matching, grid preservation on mismatch, recovered preset table |
| `mixxx_integration_test.cpp` | Qt plugin adapter end to end: plugin output, `Beats` construction, additive downbeat round-trip and monotonic beat iteration (built only inside Paramix) |

The test binary is `paramix-analysis-test`; its tests are registered with CTest,
and the existing `mixxx-test` suite is unchanged.
