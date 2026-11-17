# Handoff: stem integration

**Date:** 2026-11-13
**Repository:** `C:\Users\Bradl\paramix` (GitHub `christianstuart/paramix`)
**Purpose:** continue the Paramix stem work in a fresh chat without replaying the
previous conversation.

Read this first, then:

- `docs/stem-file-export.md` - the new native stem set (FLAC/ALAC/AAC).
- `docs/stems-onnx.md` - the ONNX Runtime backend and the local model setup.
- `docs/adr/0001-inference-runtime.md`, `0002-stem-cache.md`,
  `0003-grid-format.md`.
- `docs/parallel-work.md` - coordination rules and the current backlog.
- `AGENTS.md` - repository rules (real-time audio, branch naming, no direct
  pushes to `main`).

---

## 1. Current state (2026-11-13, post-merge)

The implementation stack is **merged to `main`** by squash merge. All required
checks were green before each merge. `main` is at `1cd8221` at the time of
writing.

| PR | Branch | What it contains | Status |
|---|---|---|---|
| [#17](https://github.com/christianstuart/paramix/pull/17) | `bradley/stems1-demucs-stft` | Host HTDemucs STFT/iSTFT (CaC boundary), 10 tests | Merged (`bd63846`) |
| [#18](https://github.com/christianstuart/paramix/pull/18) | `bradley/stems1-dsp-postprocess` | Normalization, bag averaging, shared limiter, `demucsReconstruct`; 27 tests | Merged (`9852835`, rebased after #17) |
| [#19](https://github.com/christianstuart/paramix/pull/19) | `bradley/stems2-build-option` | `STEM_SEPARATION` AUTO/ON/OFF, ONNX Runtime discovery, vendored demucs-onnx runtime, backend + factory; 7 tests | Merged (`8b0f49c`) |
| [#20](https://github.com/christianstuart/paramix/pull/20) | `bradley/stems3-stem-files` | Native stem set export: FLAC/ALAC/AAC + sidecar; 10 tests | Merged (`7cda9ed`) |
| [#21](https://github.com/christianstuart/paramix/pull/21) | `bradley/docs-stems-status` | Backlog, Traktor correction, this handoff | Merged (`1cd8221`) |

The feature branches remain on the remote but are no longer needed. The
placeholder `STEM_SEPARATION` option and the ORT backend are now in `main`.

**Program board:** <https://github.com/users/christianstuart/projects/2>

| Card | Status |
|---|---|
| Stems 1 - host STFT/iSTFT | Done |
| Stems 2 - ONNX Runtime backend | In progress |
| Stems 3 - native stem set export | In progress |
| Stems 4 - background queue/service | Todo |

Item IDs (for `gh project item-edit`): Stems 1 `PVTI_lAHOBD3Hks4BrZB2zhIXi6s`,
Stems 2 `PVTI_lAHOBD3Hks4BrZB2zhIXjDI`, Stems 3 `PVTI_lAHOBD3Hks4BrZB2zhIXjJ0`,
Stems 4 `PVTI_lAHOBD3Hks4BrZB2zhIXjP8`. Project ID
`PVT_kwHOBD3Hks4BrZB2`.

---

## 2. Architecture snapshot

- `StemSeparatorBackend` / `StemSeparation` (merged in #13) are the generic
  windowed backend seam. They are still used by their own tests.
- `lib/demucs-onnx/` is a vendored copy of the **MIT header-only** runtime from
  `HRSadeghi/demucs-onnx`, commit `757ab9f`. `demucs::Separator` owns the
  whole-track loop: chunking, track-level normalization, overlap-add and bag
  blending.
- `DemucsOnnxSeparator` (`#19`) wraps `demucs::Separator`, converts its planar
  `[sources][channels][count]` output to Paramix's interleaved eight channels
  (drums/bass/other/vocals), and exposes a factory that returns null plus a
  reason when the build has no ONNX Runtime.
- `demucsstft`, `demucsnormalization`, `demucsbag`, `demucslimiter`
  (`#17`/`#18`) are the model-agnostic DSP. The ONNX wrapper currently uses the
  vendored runtime's own STFT; the two implementations coexist. Decide later
  whether the standalone DSP is used by another backend or retired.
- `stemfilewriter` (`#20`) writes four stereo files plus `stem-set.json` in
  FLAC, ALAC or AAC. It is **not yet called** by a service or the UI.
- The playback cache in ADR-002 (one 8-channel FLAC per track) is **not
  implemented yet**. The `#20` export is a separate user-facing artifact; the
  cache remains the planned playback path.

---

## 3. Local environment and commands

**Windows x64, Visual Studio 2022 Build Tools, Ninja, Qt 6.10.**

Build from the MSVC developer environment; CMake is configured in `build/`:

```powershell
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"" -arch=x64 -host_arch=x64 >nul && cmake --build C:\Users\Bradl\paramix\build --target mixxx-test"
```

Run a suite:

```powershell
cd C:\Users\Bradl\paramix
$env:PATH = "C:\Users\Bradl\paramix\build;$env:PATH"
.\build\mixxx-test.exe --gtest_filter=StemFile*:StemSeparationTest.*
```

ONNX Runtime backend (only on the `#19` branch):

```powershell
cmake -S . -B build `
  -DSTEM_SEPARATION=ON `
  -DSTEM_SEPARATION_ORT_ROOT=C:/Users/Bradl/paramix/buildenv/onnxruntime-win-x64-1.32.0/onnxruntime-win-x64-1.32.0
cmake --build build --target mixxx-test

py tools\make_test_demucs_onnx.py --out-dir build\testmodel
$env:PARAMIX_TEST_DEMUCS_ONNX = "C:\Users\Bradl\paramix\build\testmodel\testdemucs.onnx"
.\build\mixxx-test.exe --gtest_filter=DemucsOnnxSeparatorTest.*
```

Real model spike (gitignored, on this machine):

```powershell
$env:PARAMIX_TEST_DEMUCS_ONNX = "C:\Users\Bradl\paramix\models\htdemucs.onnx"
.\build\mixxx-test.exe --gtest_filter=DemucsOnnxSeparatorTest.SeparatesTinyModelAndReportsProgress
```

The real graph is the demucs-onnx `htdemucs.onnx` (160.8 MB) plus
`htdemucs.json`; one padded 7.8 s window separates in about 6 s on the CPU
provider. `models/`, `buildenv/` and `build/` are gitignored.

**Important:** `STEM_SEPARATION` only exists on the `#19` branch. On `main` the
variable is ignored. Reconfigure after switching branches if the build cache was
last configured with `ON` and ORT is not present.

---

## 4. What is done

### Stems 1 - host transform and DSP (`#17`, `#18`)

- `src/stems/demucsstft.*` - 4096 FFT, 1024 hop, periodic Hann, normalized
  forward transform, Nyquist dropped, reflective padding and edge-frame trim;
  CaC layout `[4, 2048, ceil(frames/1024)]`.
- `src/stems/demucsnormalization.*` - unbiased statistics,
  `(x - mean) / (1e-5 + std)` and `x * std + mean`.
- `src/stems/demucsbag.*` - weighted bag averaging of denormalized CaC and
  waveform outputs, with `reset()` for buffer reuse.
- `src/stems/demucslimiter.*` - one shared gain over every stem and the stem
  sum, preserving phase and the mix reconstruction.
- `demucsReconstruct()` - residual + iSTFT, aliasing-safe.
- 37 new tests across the two PRs; full-window round trip 160 dB interior.

### Stems 2 - ONNX Runtime backend (`#19`)

- `STEM_SEPARATION` AUTO/ON/OFF; `cmake/modules/Findonnxruntime.cmake` honors
  `STEM_SEPARATION_ORT_ROOT` and `CMAKE_PREFIX_PATH`; nothing downloads at
  configure time.
- Vendored MIT runtime under `lib/demucs-onnx/`.
- `DemucsOnnxSeparator` and `createDemucsOnnxSeparator()`; bounded worker
  threads (`min(4, max(1, cores/2))`); four-source models only.
- Tiny synthetic ONNX generator `tools/make_test_demucs_onnx.py`; 7 tests.
- `.github/workflows/stems-onnx.yml` downloads ORT on Linux, generates the tiny
  model, builds with `STEM_SEPARATION=ON` and runs the backend tests. It is
  green (37m 53s).

### Stems 3 - native stem export (`#20`)

- Traktor `.stem.mp4` **writing is dropped**; the reader remains.
- `src/stems/stemfileformat.*` - `flac`/`alac`/`aac`,
  `[Stems],FileFormat` and `[Stems],AacBitrate`.
- `src/stems/stemfilewriter.*` - four stereo files plus `stem-set.json`;
  FLAC/ALAC via libsndfile (24-bit), AAC via the existing FDK encoder;
  atomic temp-directory write; refuses an existing set.
- 10 tests: FLAC/ALAC round trips, AAC ADTS smoke test, sidecar, existing-set
  refusal, invalid arguments, temp cleanup.

---

## 5. Next work, in order

### A. Merge status

The implementation and docs stack is merged to `main`. Start at B.

### B. Stems 3 integration (done)

- Schema revisions 41 (`stem_set`) and 42 (`stem_queue`) with
  `StemSetDao`/`StemQueueDao` and DAO tests.
- Settings -> Stems binds `[Stems],FileFormat`, `[Stems],AacBitrate` and
  `[Stems],ModelPath`.
- "Save Stems As..." is in the track context menu; one track asks for a set
  name, several tracks derive unique names.
- `StemService` applies `demucsLimiterGain()` before `writeStemSet()` and
  inserts the `stem_set` row.

### C. Stems 4 - background queue (done, except the cache trigger)

- `StemService` is owned by `CoreServices`, runs at `QThread::LowPriority`,
  persists jobs in `stem_queue`, resumes them at startup and supports pause,
  resume and cancel.
- Per-track statuses are `NotSeparated`, `Queued`, `Separating%`, `Ready`,
  `MissingStemFile` and `Failed`; `statusForTrack()` falls back to the
  `stem_set` index after a restart.
- Free-space preflight and the format/bitrate settings are shared with the
  export path.
- The 8-channel `AnalyzerWaveform` trigger is **blocked on ADR-002**: there is
  no cache file to analyze until the cache writer exists. Wire it there, not to
  the exported four-file set.

### D. Backend follow-ups

- Wire the shared limiter and 44.1 kHz resampling into the ONNX wrapper. The
  vendored `lib/demucs-onnx/include/demucs/resample.hpp` is available; the
  input track may be 48/96 kHz.
- Add Windows and macOS ORT CI jobs once runtime distribution is decided.
- Model manager and ADR-005: the **HTDemucs weights are CC BY-NC 4.0**, not
  MIT. A distribution decision is required before shipping weights. The
  `paramix-models` repository is the intended host.

### E. Verification before opening the next PR

```powershell
.\build\mixxx-test.exe --gtest_filter=StemFile*:StemSeparationTest.*:DemucsOnnxSeparatorTest.*
```

Also run `clang-format --dry-run --Werror --style=file` on changed C++ files,
and keep new workflow files separate from `.github/workflows/ci.yml`.

---

## 6. Risks, gotchas and ownership

- **Weights licence:** CC BY-NC 4.0. Do not commit or bundle them. The runtime
  code is MIT.
- **Traktor:** writing is dropped; reading `.stem.mp4` stays. Do not
  reintroduce the NI `moov/udta/stem` writer.
- **ALAC is CAF:** libsndfile writes ALAC in `.caf`, not `.m4a`. AAC is an ADTS
  `.aac` stream, not an M4A container.
- **FDK AAC** is loaded dynamically; the AAC test skips when the library is
  unavailable. The recording feature already has the same dependency.
- **ONNX Runtime** is not in the standard buildenv. The local root is
  `buildenv/onnxruntime-win-x64-1.32.0/onnxruntime-win-x64-1.32.0`; CI
  downloads it.
- **Two STFT implementations:** the standalone `demucsstft` (double precision,
  qm-dsp FFT) and the vendored runtime's `Spectrogram` (float, own FFT). The
  ONNX path uses the latter. Do not mix them in one output without a reason.
- **Shared files:** `CMakeLists.txt`, `res/schema.xml`, `src/track/track.h`,
  `src/engine/engine.h` are collision-prone. `docs/agent-context.md` and
  `.github/workflows/ci.yml` are main-owned; do not edit them.
- **Branches:** `bradley/<topic>`; PR plus checks; no direct pushes to `main`.
- **Docs edits:** the docs PR includes the two pre-existing consolidated
  planning edits plus this handoff. Merge it last.

---

## 7. Copy-paste prompt for a fresh chat

```text
Continue the Paramix stem integration in C:\Users\Bradl\paramix.

Read docs/handoff-stems-integration.md first, then docs/stem-file-export.md,
docs/stems-onnx.md, docs/adr/0001-inference-runtime.md, docs/adr/0002-stem-cache.md
and docs/parallel-work.md.

The implementation stack is merged to `main` (#17-#20); #21 updated the docs.
Start Stems 3 integration at section 5B: schema revision 41, the
Settings -> Stems format controls, the context action, and calling
writeStemSet() from the separation service after applying the shared limiter.
Follow the repository rules in AGENTS.md and docs/parallel-work.md.
```

---

## 8. Stem detail renderer (2026-11-17)

The deck stem picture no longer uses the analysis summary or the HSV family palette. It is
now the ported Traktor detail pipeline (`docs/adr/0006-stem-detail-renderer.md`):

- `src/waveform/stemdetail/` holds the entry/config types, the reducer, the colour
  consumer, the column cache types, the background builder and the two-entry cache.
- The builder opens the track with `SoundSourceProxy` and
  `OpenParams::setChannelCount(kAnalysisMaxChannels)`, exactly like `AnalyzerThread`. That
  resolves both native Paramix stems (`<cache>/<trackId>/stems.flac`, eight interleaved
  channels in drums/bass/other/vocals order) and Traktor `.stem.mp4`. Stem `s` is
  de-interleaved from channels `(2s, 2s + 1)`.
- `allshader::WaveformRendererStem` keeps its class and slot API but now emits four lanes
  from the cache. `splitStemTracks` no longer changes the lane layout; `opacity` scales
  lane alpha (0.75, the shipped default, means opaque); `outlineOpacity` and
  `stemVisualGain` are no-ops.
- The analysis format is unchanged: `WaveformStemColumn` and the analyzer still serve the
  other renderers.

### Performance of the detail renderer

The first implementation emitted per-pixel geometry (one rectangle per colour run per
column). On the Retina QML skin that is ~388,000 vertices / 9.3 MB per frame; profiling
showed ~55 % of the render thread in the Metal host-to-GPU vertex copy, and the process sat
at ~100 % CPU with visible stutter. `StemDetailRendererStem` now rasterises the coloured
rows into one RGBA8 image at device resolution and draws it with four textured quads
(nearest filtering, pixel-aligned UVs); the lane bases remain geometry. The render thread
is back to waiting for the drawable and the process runs around 30–40 % while playing.
The texture image covers the viewport plus a 25 % margin, so it is rebuilt only a few times
per second; between rebuilds only the quads' UVs change. `QmlWaveformDisplay` also allows a
1 ms pacing tolerance so a 60 Hz display updates the waveform every frame instead of
slipping through the millisecond timer. The remaining optional step is in-place ring-buffer
updates for the texture (only newly exposed columns) if longer tracks or two simultaneous
stem decks need more headroom.

### Iterating on the build

`build-main/.ninja_deps` was found corrupt (ninja printed
`premature end of file; recovering`). With a corrupt deps log, every build rebuilt the PCH
and all ~860 objects (~6 minutes). Delete the file once and rebuild; incremental edits are
then ~20 seconds. If it recurs after an interrupted build, remove it again:

```bash
rm build-main/.ninja_deps
cmake --build build-main --parallel   # once, full rebuild
```

