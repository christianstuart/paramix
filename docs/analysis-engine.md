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
  src/phase.*                    gap-search phase guess and downbeat choice
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
    std::vector<Transient> transients;
};
class Engine {
  public:
    struct Config {
        int sampleRate;
        int channelCount;
        std::int64_t totalFrames;
        bool keepTransients;
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
| Anchor | `0x3e7eb95a` = 0.248754 s (reference identical) |
| Onsets / envelope / bass | all values bit-identical |

The deterministic finalize pass and the three tempo passes reproduce the
reference bits on this fixture. The committed GTest literals in
`lib/paramix-analysis/test/literals.h` were generated once from the reference.

### 5.2 Against Traktor on the local reference tracks

The engine was run from the decoded FLACs and compared with the Traktor 4.1.1
values recorded in the reference ledger (L-170, L-173, L-174). All 13 paired
tracks reproduce the BPM **and** the anchor bit for bit:

| Track | BPM (engine = reference) | Anchor (s) | Transients |
|---|---|---|---|
| Lady Gaga - Poker Face | 119.001587 | 0.407065481 | 1151 (cache 1151) |
| Playboi Carti - Rockstar Made | 150.051865 | 1.592082739 | 1055 |
| Rihanna - S&M | 128.000870 | 1.385426402 | 1217 |
| JAY-Z - Run This Town | 173.555801 | 0.037959907 | 1669 |
| Tay-K - I <3 My Choppa | 159.995972 | 0.006986653 | 613 (cache 613) |
| Zeddy Will - Bow Bow Bow | 130.000122 | 0.018958662 | 757 (cache 757) |
| Young Thug - Millions | 99.000282 | 0.023769572 | 891 |
| Lil Skies - Welcome to the Rodeo | 170.000977 | 0.002598705 | 982 |
| TWOFACED - I Need You | 142.000900 | 0.024070634 | 988 |
| Justin Bieber - Sorry | 100.000778 | 0.011644487 | 1059 |
| JAY-Z - Ni**as In Paris | 139.999893 | 0.476622730 | 1271 |
| Ne-Yo - Time of Our Lives | 124.000595 | 0.961584270 | 1221 |
| Sexyy Red - Looking For The Hoes | 151.008881 | 0.354333133 | 717 |

The transient counts for the three tracks with paired caches match exactly
(1151, 613, 757). Two additional local tracks without a reference pairing
(The Black Eyed Peas - Rock That Body, Travis Scott - SICKO MODE) also analyze
successfully.

Because the reference itself reproduces Traktor bit for bit, these numbers
meet the acceptance row "BPM at most 0.002" and "downbeat at most 5 ms with
the correct beat index" with zero measured error on the corpus.

### 5.3 Determinism

The engine is single-threaded and uses only `float`/`double` arithmetic with
no hash-order or address-dependent behavior. The determinism test analyzes the
same fixture three times and compares BPM, anchor, confidence, every position,
weight and bassWeight bit for bit.

---

## 6. Persistence

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
db=1|dbbar=4|dbphase=<0..3>|dbconf=<3 decimals>|dbsrc=paramix-analysis-engine:1
```

`Beats::getDownbeatInfo()` parses these fragments on construction, so the
downbeat metadata survives a restart and an unmodified build ignores it
without destroying the grid. `beats_version` is never bumped: an unknown
version is replaced with a frame-0 placeholder by the loader.

`AnalyzerBeats::shouldAnalyze()` ignores the `db*` fragments when checking
whether the settings changed, so a stored native grid is not re-analyzed on
every launch.

The plugin appears in the existing Preferences detector list automatically
because the UI enumerates `AnalyzerBeats::availablePlugins()`. It is appended
last, so `defaultPlugin()` remains Queen Mary.

---

## 7. Confidence

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

## 8. Operational notes

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
- **No cache interop.** The engine does not read or write any Traktor file and
  does not use the Traktor peaks blob.

---

## 9. Tests

`lib/paramix-analysis/test/` contains GTest coverage for:

| Test file | Coverage |
|---|---|
| `frontend_test.cpp` | window, `log10(1+v)`, 512-point FFT magnitudes, fine/coarse flux, envelope, bass, 7-point gate, refinement, all against reference bits |
| `resampler_test.cpp` | exact mode-6 reference, copy mode, declared frame count for every rate, DC gain, impulse alignment, determinism |
| `tempo_phase_test.cpp` | three-pass tempo scan, circular mean, phase guess, downbeat choice, reference bits, perfect-grid behavior |
| `engine_test.cpp` | end-to-end synthetic click tracks for all 12 rate buckets, mono/stereo agreement, determinism over three runs, idempotent finish, empty input, transient retention |
| `mixxx_integration_test.cpp` | Qt plugin adapter end to end: plugin output, `Beats` construction, additive downbeat round-trip and monotonic beat iteration (built only inside Paramix) |

The test binary is `paramix-analysis-test`; its tests are registered with CTest,
and the existing `mixxx-test` suite is unchanged.
