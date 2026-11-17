# Paramix — Working Context (agent handoff)

**Purpose:** this is the living handoff document. If a session runs out of context, read this
file first, then `docs/paramix-stem-beatgrid-ui-plan.md` (the program plan) and
`docs/old-context.md` (raw transcript of the original planning session).

**Last updated:** 2026-11-15, after the stem waveform and deck layout pass (spectrum colors, label strip, 160 px waveform band).
**Update rule:** every agent that changes repo state must append to §9 (Work log) and correct
§3 (Current state) before finishing.

---

## 1. What this project is

`paramix` is a fork of **Mixxx 2.7.0-alpha** (GPL-2.0, C++20, Qt 6, CMake ≥ 3.22) being turned
into a commercial-grade DJ application with four headline features:

1. A new beatgrid engine (downbeats, confidence, tempo maps, non-destructive editing).
2. Offline 4-stem separation comparable to Traktor (HTDemucs via ONNX Runtime).
3. A 4-stem waveform (stacked lanes **and** tinted composite).
4. A general UI overhaul, rebranded as **Paramix** (new QML skin as default).

- Local path: `/Users/christianstuart/Documents/paramix`
- Remote: `https://github.com/christianstuart/paramix` (branch `main`) — **public** as of 2026-11-11
- No `upstream` remote by explicit user request: **do not sync with mixxxdj/mixxx.**
  The imported tree is the source of truth.
- Upstream basis recorded as "Mixxx 2.7.0-alpha"; keep GPL notices and attribution.

**AI policy (AGENTS.md):** this fork is aided by AI agents; disclosure of AI involvement is not
required. Architecture conventions still apply: `ControlObject`/`ControlProxy` with
`[Group], key_name`; the engine thread is real-time (no allocation/locks, may emit but not
receive Qt signals); `parented_ptr`/`make_parented` for Qt ownership.

### 1.1 Writing conventions (apply to every text file from now on)

- **American English spelling** in all prose: `color`, `behavior`, `license`, `customize`,
  `customizable`, `analyze`, `analyzer`, `canceled`, `labeled`, `localized`, `normalize`,
  `artifact`, `center`, `optimize`, `modeling`, `gray`. This covers docs, README, code
  comments, commit messages and PR text. Do not rename existing code identifiers to match —
  only prose.
- **Repository owner is `christianstuart`** (renamed from `Sl1pstr3am` on 2026-11-11). Use
  `christianstuart/paramix` and `github.com/christianstuart/...` in every new link, badge, git
  remote and `gh` command. Old URLs redirect, but never write new ones with the old name.
- **`docs/old-context.md` is a verbatim historical transcript** and is exempt: it keeps its
  British spelling and the pre-rename username. Do not "fix" it, and do not treat its wording
  as the project's style.

---

## 2. Locked decisions (do not re-litigate)

| # | Decision |
|---|---|
| 1 | Default separator `htdemucs_ft` (~630 MB fp16); single `htdemucs` only as low-end fallback |
| 2 | Separation on demand + optional batch; **no** auto-separate on import |
| 3 | Downbeats/bars are **required in v1** |
| 4 | `.stem.mp4` **export** (Traktor interop) in v1 |
| 5 | New **Paramix QML skin** is default and highly customizable |
| 6 | Windows x64, Linux x64 (+arm64 where feasible), macOS arm64 **and** x86_64 |
| 7 | Models are **download-only via GitHub Releases**; nothing in git or installers (MIT license prompt, SHA-256, resume, install-from-file) |
| 8 | Rebrand as **Paramix**; brand-only in v1 (keep `~/.mixxx` data dir) |

Program option chosen: **B "Traktor-class"** (~20–24 weeks, 6–8 agents), with Option A as the
first shippable milestone. Option C (real-time separation) is a deferred spike.

Decisions resolved on 2026-11-11 (supersede the plan's §11 defaults):

| # | Decision |
|---|---|
| 9 | **Model hosting:** dedicated `paramix-models` repository (keeps 630 MB assets out of the app repo/releases) |
| 10 | **Data directory:** **migrate to `~/.paramix`** with a first-run migration wizard (supersedes the plan's brand-only default); keep a downgrade fallback |
| 11 | **Versioning:** start at `0.1`, milestone releases (`0.1`, `0.2`, …); About shows "based on Mixxx 2.7.0" |
| 12 | **GPU:** auto-enable CoreML/DirectML/CUDA with a CPU opt-out in Settings |
| 13 | **`.stem.mp4` codec:** user-selectable in Settings (ALAC or AAC), not a fixed default; export dialog must show size/time trade-off |
| 14 | **Local toolchain:** install Qt 6 via MacPorts on this workstation so builds can be verified locally |

Still-open defaults: RoFormer "Max" tier in v1.1; full runtime QML overrides (experimental);
no telemetry; macOS 13+ / Win10 21H2+ / Ubuntu 22.04+ / Fedora 39+ as the supported floor.

---

## 3. Current state (truth as of the last update)

### 3.1 Git / repo
- Commits: `96f541b` Initial commit → `8c67ba8` Update repository (import, 6035 files) →
  `0901e4b` Update repository (docs) → `2ef4670` ci: fix baseline workflow flags and hygiene.
- History is linear on `main`. Direct commits were the rule during Phase −1; since 2026-11-12
  `main` is **protected** (PR + all five CI jobs required, force pushes and branch deletion
  blocked). Work on `bradley/<topic>`-style branches and merge with
  `gh pr merge --squash --auto`. Current commits: `96f541b` Initial commit → `8c67ba8` import →
  `0901e4b` docs → `2ef4670` → `55c4b11` → `c41bb96` → `18c817b` → `91b72cd` → `7cbd5fc` →
  `a48cacc` → `72b1965` → branch protection.
- `.DS_Store` was tracked and is now untracked + gitignored.
- Tracked size ≈ 180 MB; `.git` ≈ 82 MB. Largest blobs are legitimate stem test fixtures
  (`src/test/stems/stem02/trance_ALAC_24bit.stem.mp4`, 12 MB).

### 3.2 Build environment on this machine
- macOS x86_64 (Darwin 25.6), Apple clang 21, Xcode CLT only.
- **No** CMake, Ninja, Homebrew, clang-format, or Qt. **MacPorts 2.12.6 IS installed** (`port`).
  147 GB free. A local Qt install (`sudo port install qt6 …`) is possible but needs user
  approval and 20–40 min + a 30–60 min compile.
- `python3` is 3.9.6 (system). `gh` CLI authenticated as `christianstuart`.

### 3.3 Phase −1 status (baseline gate: "CI green, formatting enforced")
| Item | Status |
|---|---|
| Commit the Mixxx import | **done** |
| Dotfiles present (`.clang-format`, `.clang-tidy`, `.gitignore`, `.gitattributes`, `.pre-commit-config.yaml`) | **done** |
| `.github/workflows/ci.yml` functional | **fixed** (phantom `-DQT6=ON` removed, `WARNINGS_FATAL` dropped, timeouts + ccache added, PEP 668 tolerated) |
| Repo hygiene (`.DS_Store`, ignore rules) | **done** |
| Pre-commit suite usable | **done** — missing root configs added (`.codespellignore`, `.codespellignorelines`, `.cmakelintrc`, `.gersemirc`, `.prettierrc.yaml`, `.markdownlint-cli2.yaml`); `no-commit-to-branch` no longer blocks `main`. `black`/`flake8` hooks need Python ≥3.10 (local 3.9 can't build them; CI 3.12 can) |
| Local verification harness | **done** — `tools/paramix_verify.sh` (6 checks, PASS/SKIP/FAIL), `tools/paramix_bootstrap.sh` (dry-run by default, `--yes` to install), `docs/baseline-local-verification.md` |
| CI actually green on 4 targets | **DONE** — run `43100769404` on `18c817b` is green on all five jobs: `checks`, Linux x64 (**1302/1302 tests**), Windows x64, macOS arm64 (39m49s), macOS x86_64 (1h15m46s). Details in `docs/baseline-report.md` §4. Test binary sets `QT_QPA_PLATFORM=offscreen` itself (`src/test/main.cpp`), so no xvfb is needed |

### 3.4 Feature work
**In progress.** Present: `docs/adr/` (ADR-001 Proposed, ADR-002 and ADR-003 accepted for
review), `src/stems/` (`StemSeparatorBackend`, `StemSeparation`), and the proven 8-channel FLAC
container. Still absent: `SoundSourceStemCache`, the `STEM_SEPARATION` flag,
`src/analyzer/plugins/beatml/`, and the `stem_cache`
table (schema is at revision 40; the plan needs 41). The QML skin lives in `res/qml` (there is no
`res/skins/Paramix` directory; QML skins are discovered from `res/qml` in developer mode). Present
from upstream: `src/track/beat*`,
`analyzerqueenmarybeats`, `analyzersoundtouchbeats`, `waveformrendererstem`, `qmlstemsmodel`,
`soundsourcestem`.

### 3.5 Current progress (2026-11-12)

The baseline is closed and feature work is landing. Current state:

| Area | Status |
|---|---|
| W1 controller mappings | **Merged** (#3): Hercules Mk2 + Traktor F1 stem controls |
| W2 QML accessibility/localization | **Merged** (#4): QML a11y labels, keyboard navigation, i18n scaffolding |
| W4 test infrastructure | **Merged** (#6): beatgrid/stem golden fixtures, F-measure helper, fixture generators |
| W5 model hosting | **Merged** (#5): `paramix_model_fetch.py`, manifest spec, self-tests |
| W6 QML Qt-API guard | **Merged** (#15): CI workflow + rule self-tests |
| ADRs 001-003 | **Merged** (#8, #9, #14): inference runtime, stem cache, additive `BarInfo` grid format |
| 8-channel FLAC spike | **Merged** (#12): libsndfile round-trip at 44.1/48/96 kHz, 16/24-bit |
| Stem separation engine | **Merged** (#13): streaming `StemSeparation`, backend seam, 15 tests. No ONNX backend yet |
| rbxport beat grid engine | **PR #16**: C++ port of rbxport `rbl-analysis`, third beat detector; green on all CI targets |
| BeatGrid `BarInfo` | Not started (ADR-003 design only) |
| Stem cache / schema 41 | Not started (ADR-002 design only) |
| ONNX runtime + model backend | **Merged** (#19): ONNX Runtime behind `StemSeparatorBackend` |
| Paramix QML skin overhaul | **In progress** (`feature/ui-overhaul`): design tokens, flat base controls, shell, Stems settings page; deck/mixer/library work landing |
| Packaging/installers | **In progress**: Paramix identity, executable name, desktop/AppStream metadata, installer artwork and app icons |

Build status: the Windows x64 local build (`mixxx`, `mixxx-test`) is green. CI is green on
Linux x64, Windows x64, macOS arm64 and macOS x86_64 for the merged stem engine and the
rbxport PR. The Linux-only stem failure was an uninitialized `SampleBuffer` accumulator,
fixed before #13 merged.

### 3.6 Current state (2026-11-15) — QML feature parity

The Paramix QML skin now exposes the classic Mixxx functionality. Verified by build, QML startup
smoke test and runtime grabs on a 2,433-track profile.

| Area | State |
|---|---|
| Track context menu | **Done.** `Mixxx.LibraryTrackMenu` (`src/qml/qmltrackmenuproxy.*`) builds the real `WTrackMenu` for the current selection and serializes its `QAction` tree; `res/qml/Library/TrackMenu.qml` renders it. All actions work: AutoDJ queue (bottom/top/replace), Load to (decks/samplers/preview/stems), playlists, tri-state crates, Adjust BPM, rating, color, Metadata (tags, cover, MusicBrainz, find-on-web), Hotcues, Clear (13), Analyze/reanalyze modes, **Generate/Remove Stems**, hide, move to trash, open in file browser, Properties. |
| Library sources | **Done.** `QmlSidebarModelProxy` mirrors `Library::sidebarModel()`, so Tracks (+Missing/Hidden children), Auto DJ, Playlists, Crates, Computer, Recordings, History, Analyze and the external collections are present. Lazy children expand; icons render. Widget-only views (AutoDJ/Recordings/Browse/Missing/Hidden) still switch to their classic widget views, not a QML list. |
| Library interaction | **Done.** Source activation fixed (`index(row, column)`), multi-select (Ctrl/Cmd, Shift range, select-all), page/home/end navigation, focus retention, debounced search with clear button, search restored on source switch, per-row `has_stems` badge. |
| Stem generation | **Done in UI.** `Mixxx.StemService` (`src/qml/qmlstemserviceproxy.*`): availability/model, queue and active counts, live 0–100 % progress, current track, generate/remove/cancel. Global task strip (`res/qml/TaskStatusStrip.qml`) with queue count and Cancel. Settings → Stems now exposes the ONNX model path and output directory. |
| Analysis progress | **Done.** `Mixxx.Library.analysisActive/analysisProgress/analysisCurrent/analysisTotal/analysisTrackTitle`; second task-strip row. |
| Background threading | **Done.** Batch analyzer capped at `min(8, cores/2)` low-priority workers; `StemAnalysisCoordinator` suspends analysis while stems separate (user pauses preserved) and pauses stems when `[App],audio_latency_usage` stays above 0.85; startup audit line logs worker counts/priorities. |
| Deck panes | **Done.** MOVE / CUE / GRID tab stack (reference: `docs/reference/traktor-Move.jpg`, `traktor-Cue.jpg`, `traktor-Grid.jpg`), one constant 112 px (96 compact) band; beatjump and beatgrid/BPM moved into panes; selection persisted per deck via `[Skin],deckN_pane` (CUE default). |
| Mixer | **Done.** Master (gain/balance/booth), headphone (gain/mix/split), main VU + clip LEDs, width-responsive 3→2→1 columns; mic/aux rack is a bounded, collapsible horizontal strip. |
| Fonts | **Done.** Deck and mixer typography resolves through `Theme.fontFamily` (Helvetica Neue by default; Helvetica override in customization). |

Fixes landed with the parity work: the profile-guard path no longer calls `exit()` from the
`QmlApplication` constructor (it crashed in Qt's SQL teardown), zero-width menus (the Basic style
takes menu width from the background), a use-after-free when switching library sources, a null
`m_pSidebarWidget` dereference in Create Crate, and `WheelHandler` failing to load under
`import QtQuick 2.12`.

Still open: preview deck and vinyl-control deck UI are placeholders; the skin-settings menu
(Ctrl+1) is absent; AutoDJ/Recordings/Browse are classic widget views; CoreML is not enabled, so
stem separation is CPU-only; per-column hide/reorder and search history/completion are missing.

### 3.7 Current state (2026-11-15) — stem waveform and deck layout

| Area | State |
|---|---|
| Stem waveform data | **Done.** The waveform analyzer opens the 8-channel stem cache, computes per-stem peak / RMS / band (low, mid, high, attack) per visual frame and stores it in the additive protobuf field `signal_stem_details = 6` (one `StemDetail` per stem, packed 2-bit band fields). `Waveform` builds a multi-resolution column pyramid once, on the analyzer thread; the renderer only samples it. Verified on `Wannabe`: 8 channels opened, 76,305 columns stored, `hasStemColumns` true in the renderer. |
| Stem waveform look | **Done.** Split lanes are solid mirrored fills: constant opacity per column (no peak-dependent fade), a darker peak envelope with a brighter RMS core, and a 0.75 px floor so quiet columns never rasterize away. Each lane is normalized by its own 95th percentile (`getStemPeak`) so all four lanes fill their height; muted lanes are ghosted (desaturated, 32 % opacity) instead of hidden. |
| Stem waveform color | **Done.** Color is computed per column from that column's own low / mid / high energy: a spectrum hue (amber lows, green mids, magenta highs, with the high-band *share* pulling to magenta) clamped to +/-0.09 hue around the lane's reference family (drums magenta, bass amber, other cyan, vocals green). This reproduces the Traktor reference palette while the content still moves the color inside the family. There is no flat per-stem fill color. |
| Stem label strip | **Done.** `res/qml/StemLaneStrip.qml` overlays the left edge of each stem waveform: one row per stem (quarter-height, matching the renderer's lanes) with a colour key that doubles as the mute toggle, the uppercase stem name, and a gain knob bound to `[ChannelN_StemX],volume`. It sits on a solid panel that fades out to the right and swallows presses/wheel so the labels never scrub the track. |
| Waveform resolution | **Improved.** Drawn columns went from a 24 px / minimum 6-frame aggregation to 8 px / minimum 1.5 frames, so close-up waveforms show the stored detail instead of wide blocks. The analysis rate is still 441 visual frames/s (2.27 ms per frame, `mainWaveformSampleRate`); going finer needs a waveform version bump and re-analysis of stem tracks. |
| Pre-/post-roll marking | **Removed.** `WaveformRendererPreroll` is no longer instantiated by `res/qml/WaveformDisplay.qml`; the zone before the track start is empty background. The tiled triangle texture was what looked like repeated junk at the start of a track, four rows deep in split-stem mode. |
| Waveform band height | **Done.** With the library visible the deck waveform strip is 160 px (`SplitView.preferredHeight`), i.e. 80 px per deck and 20 px per stem lane; the extra height comes from the library pane, which is the only flexible item in that split. |
| Four-deck waveform strip | **Done.** Only channels 1 and 2 get a waveform row in the top strip; with four decks enabled those two keep the full half height instead of splitting into four. Decks 3-4 stay fully usable from their deck panes. |

---

## 4. Where the code already supports the plan (don't rebuild these)

- **Stems playback exists:** `SoundSourceSTEM` (`src/sources/soundsourcestem.cpp`, 8 channels,
  NI 5-stream layout), `EngineDeck::processStem()` (`src/engine/channels/enginedeck.cpp`),
  per-stem control groups `[ChannelN_Stem1..4]` (`volume`, `mute`, `color`, VU),
  `kMaxSupportedStems = 4` (`src/engine/engine.h`), `WaveformData::stems[4]`,
  `allshader::WaveformRendererStem`, `src/qml/qmlstemsmodel.*`, `WTrackStemMenu`,
  fixtures in `src/test/stems/`.
- **Beatgrid model is tempo-map capable:** `mixxx::Beats` (`src/track/beats.h`) = markers +
  terminal tempo; serialized as `BeatGrid-2.0` / `BeatMap-1.0`; `BeatGrid-1.0` readable.
  Detection: `AnalyzerBeats` → Queen Mary or SoundTouch only (string `if/else`, no dynamic
  loading). `BeatUtils` irons beats and snaps BPM heuristically. No downbeats/confidence.
- **Integration point for generated stems:** hook `SoundSourceProxy::openAudioSource()` (it
  already holds the `TrackPointer`) to return a stem source when a cache exists; the existing
  8-channel engine path then works unchanged. `CachingReaderWorker` already requests 8 channels.
- **Waveform constraint:** scene-graph backend uses `kPixelPerStrip = 2` (precision compromise);
  stem renderer is 6 verts × 4 stems × 2 layers = 48 verts/pixel column, CPU-built per frame.
- **Skin reality:** legacy XML skins are parsed by `legacyskinparser.cpp`; `LateNightQML` is the
  only QML skin; library and preferences are still Qt Widgets (`Library.qml` embeds
  `Mixxx.LegacyLibraryItem`). QML skin has 0 `qsTr()` calls — localization debt to fix.

---

## 5. Known blockers and gotchas

1. **GitHub Actions runner acquisition is very slow (first-run cold start).** Runs sat in
   `queued` with jobs created but no runner for ~8–10 minutes after the repo was made public;
   then runners appeared and jobs executed. This is no longer a blocker — but expect several
   minutes of latency before a run starts. Runs from 20:22/20:26 that predate the visibility
   change may remain permanently queued; ignore them.
2. **Whitespace gate was wrong and is fixed.** `git show --check HEAD` flagged the imported
   Mixxx tree's pre-existing trailing whitespace on every run. The workflow now runs
   `git diff --check HEAD^ HEAD`, so only the pushed commit is gated.
3. **`-DQT6=ON` is not a CMake option** in this tree (Qt 6 is mandatory). The real switch is
   `QML` (`cmake_dependent_option`, CMakeLists.txt:2357). `WARNINGS_FATAL` (CMakeLists.txt:2210)
   defaults OFF — do not turn it on for the baseline.
4. **macOS/Windows buildenv env propagation** is handled by the buildenv scripts writing to
   `$GITHUB_ENV` themselves; no cross-step `source` needed.
5. macOS x86_64 CI builds with `-DQML=OFF` (carried over). This is a **gap** against decision 6
   (QML skin is default); revisit once CI runs.
6. No local compiler toolchain beyond Apple clang: **any "build is green" claim must come from
   CI**, not from this machine, until Qt is installed.
7. `docs/old-context.md` is a raw chat transcript (≈1,267 lines, mostly UI padding). Use it only
   for historical intent; the plan document is authoritative.
8. **`README.md` describes the target feature set as if it already ships.** This is an explicit
   owner decision (2026-11-11), not an oversight. Do not treat the README as a specification or
   as evidence that a feature exists — check the plan (§1 acceptance criteria) and the code.
   When a feature lands, verify the README claim still matches the implementation.
8. `concurrency: cancel-in-progress: true` works: a new push cancels the previous run's jobs,
   which can look like a spurious "canceled" status.
9. **A second contributor team works in this repo concurrently.** `docs/parallel-work.md` defines
   file ownership and the claimable workstreams. Before editing anything outside the main
   workstream's paths, check its ownership map; never push to `main` from a contributor branch.
10. **The beatgrid has no downbeat field, and a new `beats_version` string is destructive.** An
    unrecognized version makes `trackdao` synthesize a frame-0 grid and save it over the real blob
    (`trackdao.cpp:1292-1296`, `beats.cpp:266-270`). Extend the proto additively and keep the
    version strings. Details and the ordered work list: `docs/beatgrid-audit.md`.
11. **`lib/qm-dsp/dsp/tempotracking/DownBeat.cpp` is already compiled and unused** — a working
    downbeat detector sits in the binary today. Do not treat downbeat detection as greenfield.

---

## 6. Next work (in priority order)

Phase -1 is closed (see §4 and `docs/baseline-report.md`), the stem waveform pass is in (§3.7),
and the tree builds and runs locally on `buildenv/` + `build-fast/`. Remaining, in order:

1. **Waveform hitching while scrolling — open bug, needs a repro with the owner.** A deck can
   blank for a moment and come back (or stay blank) while scrolling. Candidates: the scene-graph
   texture upload path, `kPixelPerStrip = 2`, geometry rebuild cost (48 verts per pixel column) and
   GPU contention with the stem separator (already mitigated by forcing the CPU provider while a
   window is up). Reproduce on a non-stem track and on the Simple renderer first to separate
   "stem renderer" from "waveform stack". Do not attempt the textured-renderer rewrite until the
   repro is pinned down.
2. **Waveform detail at deep zoom.** 441 visual frames/s is the ceiling today. Decide whether stem
   tracks should analyze at 2-4x that rate (waveform version bump, re-analysis of the stem library,
   ~2-4x blob size and memory) or whether the 8 px column floor plus 1.5-frame aggregation is
   enough. Measure the blob size on a 4-minute stem track before deciding.
3. **Stem cache silence runs.** One stem of `Wannabe` has ~28 % zero columns with runs up to
   ~0.3 s of digital silence (audible as gaps in that lane). Looks like a separator/cache artifact,
   not quiet material. Check the 8-channel cache directly (ffmpeg `astats`, then compare with the
   separator's output buffers) before changing the renderer.
4. **Stem label strip follow-ups.** Icons per lane (drums/bass/other/vocals) matching the reference,
   a visible mute affordance (today the colour key doubles as the toggle), and per-lane separators
   across the waveform so the four lanes read as rows.
5. **Other skins and controller screens.** They still instantiate `WaveformRendererPreroll`
   (`res/skins/LateNightQML/...`, `res/controllers/...`). The owner asked to leave them alone for
   now; revisit when the Paramix deck is final.
6. **FDK AAC encoder crash (independent bug).** `EncoderFdkAac::processFIFO()` segfaults with real
   audio; AAC recording/broadcast is affected even though stem export is FLAC-only. Fix or disable
   the encoder deliberately rather than leaving a latent crash.
7. **`build-asan/` (4.7 GB, untracked) needs a keep/delete decision.** It is ignored by git; nothing
   in the tree depends on it.
8. **Phase 0 ADRs 001-005 and the plan §7.3 spikes** remain from the program plan (ONNX
   `htdemucs_ft` benchmark, Beat This! F-measure, 8-channel FLAC round-trip, four-lane 60 fps check,
   GitHub Releases model-download dry run).
9. Revisit the macOS x86_64 `-DQML=OFF` CI gap as its own scoped commit, with the 120-minute timeout
   already in place.

---

## 7. Useful commands

```bash
cd /Users/christianstuart/Documents/paramix

# CI state
gh run list -R christianstuart/paramix -L 5
gh api repos/christianstuart/paramix/actions/runs/<id>/jobs \
  --jq '.jobs[] | "\(.name): \(.status) \(.conclusion // "")"'
gh workflow run ci.yml -R christianstuart/paramix

# Local checks (no Qt needed)
python3 -c "import yaml;yaml.safe_load(open('.github/workflows/ci.yml'))"
git grep -nE '^(<<<<<<< |======= |>>>>>>> )' -- ':!*.md'
git show --check --format= HEAD

# Toolchain (only with user approval)
tools/paramix_bootstrap.sh --yes          # cmake, ninja, clang-format, optional Qt via aqtinstall
tools/paramix_verify.sh                   # repo checks + clang-format + CMake flag sanity
```

---

## 8. Agent ownership map (from plan §7.1)

A0 architect/integrator (docs/adr, CMakeLists, contracts) · A1 beatgrid core (`src/track/beat*`,
`src/proto/beats.proto`) · A2 beat ML (`src/analyzer/plugins/beatml/`) · A3 stem runtime
(`src/stems/` separator/model) · A4 stem pipeline/DB (`stemcache`, DAO, `res/schema.xml`) ·
A5 audio engine (`src/engine/`, `src/mixer/`) · A6 waveform (`src/waveform/`,
`src/rendergraph/`, `res/shaders/`) · A7 QML/UX (`res/skins/Paramix/`, `res/qml/`, `src/qml/`) ·
A8 QA (tests, benchmarks) · A9 compliance/packaging (`.github/`, `packaging/`, model releases).

Shared single-owner files (route through A0): `CMakeLists.txt`, `res/schema.xml`,
`src/track/track.h`, `src/engine/engine.h`.

---

## 9. Work log

- **2026-11-14** — **Exit-path crash fixed.** `QmlApplication` called `exit(-1)` from inside its
  constructor when the existing-profile guard fired, which ran static destructors while
  `QCoreApplication` and `CoreServices` were alive and crashed in
  `QSqlDatabasePrivate::removeDatabase`. It now clears `m_loadSucceeded` and returns, and
  `runMixxx()` reports the startup error after normal teardown. Verified: exit code 1, no crash
  report.
- **2026-11-14** — **Track menu restored through a C++ bridge.** `QmlLibraryTrackMenu`
  (`src/qml/qmltrackmenuproxy.*`) serializes the real `WTrackMenu` action tree (submenus,
  separators, enablement, shortcuts, rating/color/checkbox widget actions) for QML rendering, and
  invokes the underlying `QAction`. `res/qml/Library/TrackMenu.qml` renders it with the Paramix
  look; the duplicate menu in `Library/Track.qml` was removed. Fixes required: menu backgrounds
  must declare an `implicitWidth` (Basic style derives menu width from it), `Menu.title` instead of
  `text`, `addItem` only for items and `addMenu` for submenus, and `MenuSeparator` created as a
  component.
- **2026-11-15** — **Library parity.** `QmlSidebarModelProxy` now mirrors the library's real
  sidebar (fixing a model use-after-free on source switch and a null-deref in Create Crate),
  `Browser.qml` uses `index(row, column)` (the old call activated only row 0), icons map to a real
  role, and the track list gained multi-selection, range selection, select-all, page/home/end
  navigation, focus retention, debounced search with a clear button, and a `has_stems` badge
  backed by the exported `stem_set` index.
- **2026-11-15** — **Progress and background-work isolation.** `QmlStemServiceProxy` exposes
  queue/active counts, current job title and 0–100 % progress, with lazy attachment to
  `StemService` (it is created after QML loads). `AnalysisFeature` publishes aggregate analysis
  progress, forwarded by `QmlLibraryProxy`. `TaskStatusStrip.qml` shows both jobs. Analyzer workers
  are capped at `min(8, cores/2)`; `StemAnalysisCoordinator` suspends analysis while stems run and
  pauses stems when audio latency usage stays high; a startup audit line records the thread
  layout. Also fixed: `__STEM_SEPARATION__` was not defined for `mixxx-qml-lib`, so
  `Mixxx.StemService.available` reported false.
- **2026-11-15** — **Stem export is FLAC only.** ALAC was removed first (libsndfile CAF/ALAC
  aborted in `psf_close` with heap corruption), then AAC followed: with enough audio to reach the
  encoder, `EncoderFdkAac::processFIFO()` segfaults (deterministic, `mixxx-test` crash report +
  lldb show an invalid `vector<short>::data()` access), and the same encoder backs AAC recording and
  broadcast. `StemFileFormat` now has only `Flac`, `writeAacStem`/`StemAacSettings`/
  `FileEncoderCallback` are gone, the sidecar no longer emits `aac_bitrate_kbps`, and both the
  legacy Qt Stems dialog and `Settings/Stems.qml` show a static "FLAC (lossless, 24-bit)" line with
  no bitrate control. Legacy `alac`/`aac` values in `[Stems],FileFormat` and queued rows parse to
  FLAC, so existing profiles keep working.
- **2026-11-15** — **VU meters moved to a scene-graph item.** `LateNightQML/Controls/ImageVuMeter.qml`
  was 13 instances of a repeating 10 ms QML Timer plus binding-driven clipped items (level -> height
  -> anchors), i.e. ~1300 binding/layout updates per second. It is now a thin wrapper over the new
  `Mixxx.VuMeter` (`src/qml/qmlvumeteritem.{h,cpp}`): one shared 30 Hz ticker decays the peak in C++,
  the level/clip sources are QSG textures drawn as five quads, and a level change costs a single
  `update()` instead of a binding storm. The wrapper only maps the LateNight theme assets and the
  legacy `micAux`/`backgroundVariant` properties, so no call site changed. Assets are loaded into
  QImage once per URL; nodes rebuild only when a source changes.
- **2026-11-15** — **Stem separation: CPU while a window is shown.** `StemService::enqueue()` now
  rejects the GPU provider when `QGuiApplication::topLevelWindows()` is non-empty
  (`isInteractiveUiRunning()`), logging once why. Verified: the separator reports "on cpu", the WAV
  export completes, and the worst frame during separation is 37 ms (was 1.3-2.3 s with the WebGPU
  provider). Headless runs still get the GPU. The ALAC crash below is independent of the provider.
- **2026-11-15** — **Stem separation profiling (QSG frame timing).** Recording start/stop is clean:
  120 frames per 2 s window (=60 fps), worst frame 3-12 ms, on a 60 s probe-driven run. Stem separation
  is the real stall: with the ONNX `webgpu` provider the UI drops to 3-13 fps with 1.3-2.3 s frame
  times (all on the render thread, `render=`), because the separator shares the Metal GPU with the
  scene graph; on `cpu` (`[Stems],GpuAcceleration 0`) the same job renders 30 fps with a 20 ms worst
  frame (30 fps is the adaptive cap added in `WaveformDisplay.qml`). Note config booleans must be
  written 1/0 - `false` is ignored and the default provider is used. Separately, ALAC stem writing
  reproducibly aborts in `sf_close` with malloc heap corruption on the StemService thread (3 crashes,
  WAV survived once); libsndfile ALAC writing is clean in isolation (4 sequential 60 s files), so the
  corruption likely originates in the separation/resample buffers and surfaces at the write.
- **2026-11-15** — **Waveform stutter work.** `WaveformDisplay.qml` lowers the deck refresh rate to
  `min(30, waveformFrameRate)` while `Mixxx.StemService.busy/queuedCount` or
  `Mixxx.Library.analysisActive` is set, so the render thread needs fewer slices while analysis or
  stem separation runs. Audit of the rest: analysis workers are already capped at
  `min(8, idealThreadCount/2)` and run `LowPriority`, `StemService` runs `LowPriority` with
  `StemAnalysisCoordinator` pausing separation above 85% audio-latency usage, and the waveform
  already extrapolates the play head to the next vsync (`getPlaySlipAtNextVSync`), so dropped frames
  do not accumulate lag. The textured scenegraph renderer was NOT landed: the existing
  `allshader::WaveformRendererTextured` is OpenGL-only (display lists + QOpenGLShaderProgram + FBO,
  guarded by QT_OPENGL_ES_2) and cannot be instantiated on the QRhi/Metal backend; a scenegraph port
  needs a new `rendergraph::Material`, two qsb shaders derived from `res/shaders/rgbsignal.frag`,
  and a whole-track texture upload - a rewrite rather than a wiring change. Plan and file list are
  captured in the session notes; not started to avoid an unvalidated rewrite across every deck.
- **2026-11-15** — **Waveform refresh rate follows the display.** `[Waveform],RefreshRate`
  (`Mixxx.Config.waveformRefreshRate`, 0 = Auto) sets the deck waveform update rate, and
  `WaveformDisplay.qml` uses it instead of the hidden `[Waveform],FrameRate`. Preferences > Waveform
  relabels the old frame-rate row "Monitor refresh" (0 shows as "Auto", range 0-240 Hz) and writes the
  new key, so the setting is reachable from the gear dialog rather than only the unwired QML settings
  popup. Auto reads `QScreen::refreshRate()` (120 Hz on the dev machine). The play head is
  extrapolated one *waveform* interval ahead (`getPlaySlipAtNextVSync`), which only lands on the next
  present when the waveform ticks once per vsync; 60 fps on a 144 Hz panel produced alternating
  2/3-vsync steps, i.e. the visible jumps. `QmlWaveformDisplay::slotFrameSwapped()` now refreshes on
  every presented frame when the rate matches the panel, because the elapsed-timer path could come up
  1 ms short and halve the rate; lower caps keep the timer path. Still open: during stems/analysis the
  adaptive 30 fps cap extrapolates 33 ms while presenting sooner.
- **2026-11-15** — **Mono keys became lamps.** The output bar's mono toggles are no longer square
  keys: each is a 10 px round lamp (dark ring when off, accent filled when on, hover lifts the face)
  with the caption at `fontSizeXs - 1`, and the first one now reads "OUT MONO". The lamp was later
  reduced to 8 px and given a top-lit gradient (lighter top stop, darker bottom, matching border) so
  it reads as a small dome rather than a flat dot. They still drive
  `[Master],mono_mixdown`, `[Master],head_mono_mixdown` and `[Recording],mono_mixdown`.
- **2026-11-15** — **Main gain knob moved into the master out box.** The MAIN tile left the mixer's
  output bar (it now starts at BAL); `[Master],gain` is a 24 px `Skin.ControlKnob` on the far right of
  the 216 x 38 master out box, under a GAIN caption in a 28 px column with the exact geometry of the
  record box (right margin 8, top/bottom margin 1). The L/R ladder ends 6 px before that column, the
  same gap the record box keeps to its record key, and the box size is unchanged.
- **2026-11-15** — **Master out box matched to the record box.** `MasterLevelDisplay` and
  `RecordingBox` are both 216 x 38 framed panels (verified on screen: 76 device px tall at 2x, 432
  wide). The master box legend is a single "OUT" line and both ladders start at the same 70 px
  content column; the record box reads "REC OUT" with the record key vertically centred between the
  ladder and the GAIN column. The file-size readout was dropped from the box (the proxy property
  remains for later use).
- **2026-11-15** — **Recording box redesigned.** `Mixer/RecordingBox.qml` is now a framed group:
  "RECORD" over "OUT" on the left, the REC key and the live recorded file size on the top line
  (`Mixxx.Recording.fileSizeText`, new in `QmlRecordingProxy`, fed by `RecordingManager::bytesRecorded`
  and reset to "--" when a take ends), the trimmed L/R meter on the bottom line, and a 24 px trim
  dial on the far right under a GAIN caption. The duration readout and clip dots were dropped from
  this box to keep it to the requested four elements.
- **2026-11-15** — **Recording mono key.** The output bar's mono column gained a third toggle,
  "REC MONO", under MASTER MONO and CUE MONO. It drives the new `[Recording],mono_mixdown` control,
  which mirrors `[Recording],channel_mode` so the Preferences recording channel mode and the toolbar
  key stay in sync; `EngineRecord` captures the mode once per recording so toggling it mid-take cannot
  change the encoded buffer format. The three rows are 12 px with 2 px gaps to fit the bar's 40 px
  content height without growing it.
- **2026-11-15** — **Toolbar master row rebalanced.** The tempo master box is now centred in the
  top bar (above the playback marker), the master level moved to its left with the caption changed
  from "MASTER" to "MASTER OUT" and placed before the ladder, and a new `Mixer/RecordingBox.qml`
  sits to its right. The recording box shows the trimmed recording level (`[Recording],vu_meter_*`
  with clip dots), the `Mixxx.Recording.durationText` readout, a trim dial on `[Recording],gain`
  (new engine control, unity at parameter 0.5) and a REC key driving
  `[Recording],toggle_recording` as a momentary button. Visibility: recording 1200 px, master level
  1400 px, tempo box 1100 px.
- **2026-11-15** — **Tempo master box in the toolbar.** New `Mixer/MasterTempoBox.qml` sits to the
  left of the master level readout in the top bar: a BPM readout (reads `[InternalClock],bpm`, which
  EngineSync keeps mirrored to the leader deck, so it shows the tempo decks are following in both
  modes) with an AUTO / MASTER switch underneath. MASTER sets `[InternalClock],sync_leader`, making
  the internal clock the sync leader and demoting the deck leader; AUTO clears it and then requests
  the soft leader on a deck (preferring a playing synced deck), because the engine keeps the clock as
  a soft leader when it is merely asked to step down. While MASTER is active the readout becomes an
  editor: click to focus, type a BPM, Enter writes `[InternalClock],bpm`, Escape/blur cancels, so
  synced decks retempo with the software master. Hidden below 1100 px width.
- **2026-11-15** — **Tempo-master pill in the deck header.** `Deck/InfoBar.qml` gained an inline
  `MasterPill` at the right end of the header's top row: a small "MASTER" capsule, hairline border
  and muted text by default, filled accent blue with dark text while the deck is the sync leader.
  It reads `[ChannelN],sync_mode` and treats 2 (soft leader) and 3 (explicit leader) as master, so
  it matches the red/yellow "Leader" state of the deck SYNC key. Hidden in minimized decks.
- **2026-11-15** — **Output bar section rules.** The output bar (MAIN/BAL/BOOTH/HEAD/MIX) now has
  3 px `borderStrong` rules on both its top and bottom edges instead of the panel hairline, so the
  seam between the deck/mixer controls and the library reads as a section break. The extra thickness
  came out of the tiles: the captions sit 2 px lower, the readouts 2 px higher, and the dials are
  18 px (was 20). The SPLIT key was shrunk to 34 px tall and `tileWidth - 6` wide so it keeps its
  clearance from the new rules.
- **2026-11-15** — **Library search moved into the source pane.** The full-width bar above the
  library (the "LIBRARY" caption plus the search field) is gone. The library pane now starts
  directly under the mixer, and the search field sits at the top of the source tree pane in
  `Library/Browser.qml`, above Tracks/Auto DJ, with its clear key inline. `Browser` exposes
  `searchInput` plus `searchEdited`/`searchCleared` signals; `Library.qml` keeps the debounce and
  the track-list search call, so Cmd+F focus/select-all still works.
- **2026-11-15** — **Layout toggles moved into a View menu.** The five toolbar keys on the left
  (4 Decks, Library, Effects, Aux, Sampler) were removed; a single "View" key at the right end of
  the toolbar opens a checkable dropdown (`Skin.CheckMenuItem`, new file `res/qml/CheckMenuItem.qml`)
  with the same five switches. Layout state lives on the main window as
  `fourDecksRequested`, `maximizeLibrary`, `showEffects` and `showSamplers`; `show4decks` stays a
  derived value so a short window still drops to two decks without forgetting the preference.
  `[Skin],show_maximized_library` is mirrored two ways with `maximizeLibrary`, and the Mic/Aux entry
  writes `[Skin],show_microphones` directly.
- **2026-11-15** — **Knob arcs drawn with Canvas.** The arc moved from QtQuick Shapes (which
  tessellates the curve into flat segments unless global multisampling is on, and clipped against
  the item bounds) to a 2D `Canvas` arc, so it is a true anti-aliased circle at any size, with the
  radius held one pixel clear of the item edge. Dash styling is preserved for the QuickFX knob.
- **2026-11-15** — **Knob arcs are always anti-aliased.** `Mixxx.Controls.Knob` only enabled
  multisampling for the arc when the global multisampling preference was set, and that preference
  defaults to Disabled, so the value arcs rendered jagged. The arc now always uses its own
  `layer.samples: max(4, preference)` layer.
- **2026-11-15** — **Mixer keys and mono outputs.** Channel PFL keys are now labelled CUE with a
  headphone icon above each key (the fader is a few pixels shorter to make room). The output bar
  gained independent mono toggles: `[Master],mono_mixdown` folds master and booth, and the new
  `[Master],head_mono_mixdown` folds the headphone bus only (added in `EngineMixer`, applied in
  `processHeadphones` when split cue is off). Both keys are gray/blue state squares with
  "MASTER MONO" / "Cue mono" captions; the bar and its tiles were compacted ~20%, the dials
  nudged down under their captions, and the split key moved to the far right.
- **2026-11-15** — **Output bar layout.** The headphone split toggle sits at the far right of the
  output bar, and the master level left the bar for the main toolbar (`Mixer/MasterLevelDisplay.qml`,
  a compact two-row L/R ladder whose own width is just the ladder, so the bars center exactly on
  the window's playback marker while the "MASTER" caption hangs off to the right). It hides below
  1400 px where it would collide with the layout toggles.
- **2026-11-15** — **Instant layout toggles.** Removed the spring height animations on the decks
  and mixer and the fade/anchor transitions tied to the toolbar layout toggles (4 decks, library,
  effects, aux, samplers), so the layout snaps to its new size instead of animating.
- **2026-11-15** — **Layout toggles reclaim their space.** Decks 3/4 were created with only
  bottom/left anchors inside their Loaders (zero size), which left the lower deck row blank in
  4-deck mode; they now fill the Loader. The effects, samplers and mic/aux Loaders also kept their
  last height after being toggled off, leaving a black gap above the library; their height is now
  bound to `active ? implicitHeight : 0`, so the library expands back as soon as they are hidden.
- **2026-11-15** — **Mixer FX column compacted.** The quick-effect preset dropdown now takes the
  place of the redundant "FILTER" caption (single 18 px control instead of a caption plus a
  separate dropdown row), so the channel PFL buttons and fader move up and the mixer - and with it
  the decks and waveform, whose height tracks `mixer.height` - shrinks by that row, returning the
  space to the library. The "MIXER" console label above the strips was removed for the same
  reason; the mixer background and channel tags already identify the section.
- **2026-11-15** — **Activity strip moved into the track browser.** The stem-separation and
  analysis progress rows left the top-of-window overlay and now sit at the bottom of the library's
  track-list pane (`res/qml/Library.qml`), scoped to the browser column so the source tree /
  playlist / file-system pane keeps its full height. The strip is zero-height while idle and
  shrinks the track list only while a job runs.
- **2026-11-15** — **Bottom bar layout.** The output knobs (MAIN, BAL, BOOTH, HEAD, MIX, SPLIT)
  are grouped at the far left, the crossfader is centered in the bar, and the master meter is a
  framed horizontal L/R ladder on the right (`Mixer/HorizontalVuMeter.qml`), with bigger rails,
  brighter inactive segments and 8 px clip LEDs so it reads at a glance.
- **2026-11-15** — **Master/headphone strip and crossfader moved to a bottom bar.** The four
  output tiles and the crossfader (with its deck-assignment buttons) left the narrow mixer column
  for a full-width bar directly below the deck/mixer row (`res/qml/Mixer/OutputBar.qml`,
  `res/qml/Mixer/CrossfaderUnit.qml`). The mixer now ends at the channel strips, so the decks -
  whose height tracks `mixer.height` - end exactly at the bar's top edge (the crossfader's top).
  The freed height goes to the library. Verified with a window grab.
- **2026-11-15** — **Deck panes moved beside the loop selector.** The separate full-width pane
  band was removed: the active MOVE/CUE/GRID pane now occupies the transport row to the left of
  the LOOP card, sharing its 92 px height with the loop card and the tab stack, so it is adjacent
  to the loop-size selector inside the play/cue block. The deck card still clips its content for
  short layouts. Verified with window grabs for all three panes (hotcue pads, beatjump strip,
  beatgrid/BPM controls) at 92 px.
- **2026-11-15** — **Deck panes, mixer sections, Helvetica.** The deck now uses a MOVE / CUE /
  GRID tab stack (Traktor reference: `docs/reference/traktor-Move.jpg`, `traktor-Cue.jpg`, `traktor-Grid.jpg`) with a constant 112 px
  (96 compact) pane band, persisted per deck in `[Skin],deckN_pane`; autosaved. The mixer gained
  master/headphone controls, main VU + clip LEDs and a collapsible mic/aux input strip, laid out
  responsively for the narrow mixer column. Deck and mixer typography now resolves through
  `Theme.fontFamily` (Helvetica Neue / Helvetica). Verified with runtime window grabs; build
  green, `QmlStartupSmokeTest` 2/2, QML API guard 0 violations.

- **2026-11-11** — Repo import committed; dotfiles added; CI workflow authored (hand-written,
  contained bugs). Repo found untracked with only `README.md` tracked before that.
- **2026-11-11** — This session: audited baseline; found Actions stuck; repo made **public**
  (user-approved) to unlock runners; `upstream` remote was briefly added then removed per user
  instruction not to sync upstream.
- **2026-11-11** — `2ef4670`: removed phantom `-DQT6=ON`, dropped `WARNINGS_FATAL`, PEP 668
  fallback for pyyaml, per-job timeouts, ccache on Linux + both macOS jobs, untracked
  `.DS_Store`, ignored `.DS_Store`.
- **2026-11-11** — `macos-x64` timeout raised **90 → 120 min**. On the `macos-15-intel` runner a
  clean configure took 13m54s and the build was still running at 42 minutes, versus 26m16s for
  the full arm64 build — Intel is ~1.6–2.2× slower on this tree. Current timeouts:
  `checks` 10, `linux` 75, `macos-arm64` 90, `macos-x64` 120, `windows-x64` 90.
- **2026-11-11** — Audits completed: dotfiles parse and are coherent (clang-format 19.1.3
  compatible, `.gitattributes` does not normalize binary fixtures, `/models/` and `/stems/`
  root-anchored and effective). Pre-commit suite identified as broken (missing 6 root configs,
  `no-commit-to-branch` blocks `main`).
- **2026-11-11** — `55c4b11`: whitespace gate scoped to the tip commit; `actions/checkout@v5`.
- **2026-11-11** — `c41bb96`: `fetch-depth: 2` on the checks job (a shallow `HEAD` has no
  parent, which is why `git show --check HEAD` flagged the entire import).
- **2026-11-11** — Pre-commit repaired: six root configs added, codespell scoped away from
  vendored `res/controllers` and `res/translations`, `no-commit-to-branch` retargeted to
  semver release branches. All fixer hooks verified idempotent (two passes each, zero diffs).
  One real first-party typo remains unsuppressed: a misspelled "Windows" in
  `docs/old-context.md` (kept as an exact-line exclusion with a FIXME; fix the doc, then delete
  the entry).
- **2026-11-11** — Local harness landed: `tools/paramix_verify.sh` reports PASS with one SKIP
  (clang-format) on a clean tree; its whitespace check was aligned with CI semantics.
- **2026-11-11** — Decisions 9–14 recorded and folded into the plan (§0 table, §6.9, §11):
  `paramix-models` release repo, `~/.paramix` migration, 0.1 milestone versioning, GPU auto
  with opt-out, user-selectable export codec, local Qt via MacPorts.
- **2026-11-11** — **README rebranded** to Paramix: stem/beatgrid/UI feature set, platform
  table, build instructions, GPL-2.0 attribution and a "Based on Mixxx" section. Build badge
  points at this repo's `ci.yml`; license badge added. Upstream download/donate/Zulip/forum/
  Transifex links removed; the Mixxx manual link is kept and labeled as upstream documentation.
  Per owner decision the README presents the full vision as current — see gotcha 8.
- **2026-11-11** — CI formatting gate added to the `checks` job (clang-format 19.1.3,
  changed C++ files only, `--diff-filter=ACMR`, vendored/generated paths excluded).
- **2026-11-11** — **Baseline closed.** Run `43100769404` on `18c817b` finished green on all five
  jobs; macOS x86_64 landed last at 1h15m46s (14 min under the old 90-minute limit). Recorded in
  `docs/baseline-report.md` §4. Two follow-ups it surfaced: `macos-x64` timeout is now 120 min
  (comment cites the measured 2.3× Intel slowdown), and enabling `-DQML=ON` on x86_64 would add
  ~11 min there, so it must land separately with that timeout in place.
- **2026-11-11** — Scratch worktree for the stem waveform: branch `feature/stem-waveform` at
  `/Users/christianstuart/.local/share/opencode/worktree/83d739/stem-waveform`, cut from
  `18c817b`. It exists so feature work cannot entangle the baseline commit; it needs rebasing
  onto `main` before use.
- **2026-11-11** — **Inventory finding:** the imported tree already contains a stem playback and
  visualization layer — `allshader::WaveformRendererStem` (split stem tracks, per-stem gain/mute,
  stack reorder, opacity), its QML wrapper `WaveformRendererStem`, `res/qml/StemKnob.qml`,
  `res/qml/Deck/HotcueAndStem.qml`, LateNight stem controls, per-stem waveform data in
  `WaveformData::stems[mixxx::kMaxSupportedStems]` filled by `AnalyzerWaveform` from the
  8-channel stem source, and `.stem.mp4` reading via `StemInfoImporter`/`SoundSourceStem` with
  ALAC and AAC fixtures. What does **not** exist: any stem *separation* and any `.stem.mp4`
  *write/export* path. Do not plan Phase 2/3 as if the stem UI were greenfield.
- **2026-11-11** — CI incident on `91b72cd`: `macOS arm64` failed in Configure because the 3.7 GB
  buildenv download truncated at 4% and `file(DOWNLOAD)` has no `STATUS` check, so the only
  symptom was `SHA256 checksum mismatch`. Added a pre-fetch step (curl retries + own hash check +
  `.part` rename) to both macOS jobs and the Windows job; `CMakeLists.txt` is untouched.
- **2026-11-11** — Plan corrected against the inventory: §1 gained a per-outcome **status** column,
  §3.3 gained the verified `WaveformRendererStem` API surface, §3.4 was rewritten as resolved,
  §7.3 marks Phase −1 done and drops the withdrawn "add `upstream` remote" step, and the 2.1/2.2
  contents no longer include building a stem renderer that already exists. The plan's original
  §0 and §3.2 already stated that the playback half exists — that part needed no correction.
- **2026-11-12** — Added `docs/parallel-work.md` for a second contributor team (Bradley's agents):
  ground rules, the "don't rebuild this" inventory, six claimable workstreams chosen to avoid the
  main workstream's files, an ownership map, verification commands and coordination protocol.
  Contributor branches use `bradley/<topic>` and PRs; direct pushes to `main` cancel in-flight CI.
  The stray `README.rtf` was moved out of the tree to `$TMPDIR/README.rtf.bak` (not committed).
- **2026-11-12** — Branch protection enabled on `main`: PR plus all five checks, no force push,
  no deletion, unresolved conversations block merge, admins included. Verified by a rejected direct
  push (GH006) and by PR #1, where every required check reports. `docs/parallel-work.md` and this
  file updated; the "commit directly to main" rule is superseded.
- **2026-11-12** — **Beatgrid audit** written to `docs/beatgrid-audit.md` from two source audits
  plus verification. Findings: no downbeat/bar/meter field anywhere; `BeatMarker` has two fields;
  protobuf has no bar attribute; qm-dsp `DownBeat.cpp` is compiled into the binary and unused, and
  needs only a decimating audio sink plus a 48 kHz step-size fix; `BeatUtils::makeConstBpm`
  re-anchors the grid and discards the detected downbeat; an unrecognized `beats_version` makes
  `trackdao` replace the grid with a frame-0 placeholder and persist it, so new fields must be
  additive under the existing version strings; Rekordbox `beat_number` is imported and discarded;
  no inference runtime or bundled-model precedent exists. Recommendation: DSP baseline first, ML
  downbeats as a separate later decision.
- **2026-11-12** — **Program board created** at
  <https://github.com/users/christianstuart/projects/2>, linked to `christianstuart/paramix` so it
  shows in the repository's Projects tab. 27 **draft cards** (no issues, so the issue tracker stays
  clean) with four fields: Status (including a new "In review" option), Workstream, Area and Phase.
  Content comes from `docs/parallel-work.md` (W1–W6) and the plan (O1–O4, the 15 deliverables, the
  open decisions). The two cards that contradicted the ADRs were corrected at creation: the grid
  card is the ADR-003 additive `BarInfo` work (no `BeatGrid-3.0`, no dual-write, no schema
  revision), and the cache card carries ADR-002's decisions. Bradley's board under `kalewaf` cannot
  be linked to this repository because a user-owned project can only link to a repository with the
  same owner, hence the new board under `christianstuart`. `gh` was re-logged in as
  `christianstuart` with the `project` scope; the stored credential still named `Sl1pstr3am` after
  the rename and rejected the new token.
- **2026-11-12** — **Plan reconciled with ADR-003.** `docs/paramix-stem-beatgrid-ui-plan.md` still
  recommended `BeatGrid-3.0`, dual-writing a legacy blob and adding `beats_legacy` /
  `beats_confidence` columns, which ADR-003 rejects because a new version string makes
  `trackdao` replace the grid with a frame-0 placeholder. Rewrote §5.2 Option 2, §6.2, the
  Option B summary, the schema-change comparison row, the A1 row, the contracts and build phase
  rows, the unit-test row, the downgrade risk row and the 10-day plan to the additive `BarInfo`
  design with unchanged version strings. No schema revision for the grid. ADR-002 merged as
  `f94c1de` (PR #9).
- **2026-11-12** — **ADR-002 written** to `docs/adr/0002-stem-cache.md` (status: Proposed).
  Decision: one 8-channel FLAC plus a `meta.json` sidecar per track, at the **source** sample rate
  and exact frame count so beatgrid/cue frames stay aligned, interleaved `1L1R2L2R3L3R4L4R` to
  match `SoundSourceSTEM` and `EngineDeck::processStem()`. Per-track **generation directories**:
  write to `tmp/`, rename into place, commit the DB row last, so the row is the commit point and
  orphans are swept at startup. Two keys: `source_fingerprint` ("same audio?") and `pipeline_key`
  (model weights + separation params + cache format version), so a model update adds an entry
  instead of overwriting a valid one. New `stem_cache` table in **schema revision 41** with
  `min_compatible="3"` — omitting it would default to 41 and lock the previous build out of the
  library. The JSON sidecar is authoritative and the table is a rebuildable index, so losing the
  database does not cost re-separation. Playback goes through a `SoundSourceStemCache` wrapper
  hooked into `SoundSourceProxy::openAudioSource()`; the existing 8-channel engine, analyzers and
  stem renderer are reused unchanged. Owner decisions: **16-bit** default (24-bit stays a supported
  metadata value), **no default cache cap** (`CacheLimitBytes=0`, free-space warning plus preflight
  floor instead of auto-eviction), and **best-ready-model fallback** when the preferred model has no
  entry. The cache is disposable and never migrated. Gated on the 8-channel FLAC write/read spike on
  all four targets; `EncoderWave` cannot write 8 channels, so a dedicated libsndfile writer is
  required.
- **2026-11-12** — **ADR-003 written** to `docs/adr/0003-grid-format.md` (status: Proposed), with
  `docs/adr/README.md` as the index and conventions file. Decision: add a `BarInfo` message
  (`beats_per_bar`, zero-based `first_beat_in_bar`) to `BeatGrid` and `BeatMap` **additively under
  the existing version strings**; absence of `bar` means unknown, never 4/4. Store phase relative
  to the grid anchor, not an absolute downbeat frame, so translation and scaling preserve it.
  Malformed bar info is ignored with a warning rather than discarding the grid. No schema revision
  and no migration. Acceptance requires decoded-value round-trip tests and a downgrade simulation
  that emulates an older build discarding unknown fields. The `Beats::operator==` sample-rate bug
  (`beats.h:216-221`) is a prerequisite fix, because equality-based tests are otherwise vacuous.
- **2026-11-11** — GitHub owner renamed `Sl1pstr3am` → **`christianstuart`**. Updated the git
  remote, README badges/links/clone URL, and every `gh`/repository reference in the plan,
  context doc and baseline report. `docs/old-context.md` intentionally left as historical record.
- **2026-11-11** — House style switched to **American English** for all text files; existing
  README, plan, context doc and baseline report converted. Convention recorded in §1.1.
- **2026-11-11** — `README.md` was found replaced on disk by `README.rtf` (an editor saved it as
  rich text). Recovered from the `.rtf` content, rewritten as Markdown with the new owner name
  and American spelling. The stray `README.rtf` was left in place (untracked) for the owner to
  delete; do not commit it.
- **2026-11-12** — **8-channel FLAC spike passed** (PR #12, `03ad15a`). Wrote 8192 frames of eight
  distinct tones with libsndfile at 44.1/48/96 kHz in 16- and 24-bit, read them back through
  `SoundSourceFLAC` and `SoundSourceProxy`, and asserted channel count, sample rate, frame count,
  the `fLaC` magic and every sample within two quantization steps. Linux x64 and Windows x64 ran
  the tests; both macOS jobs built and linked but do not run the suite, so the runtime half is
  unproven there (baseline gap). ADR-002's container is proven and the four-stereo-FLAC fallback is
  not needed. `mixxx-test` now links `SndFile::sndfile` explicitly, because `mixxx-lib` links it
  `PRIVATE`.
- **2026-11-12** — **Stem separation engine started** (PR #13). `src/stems/stemseparatorbackend.h`
  is the runtime-agnostic seam: one window, stereo in, eight interleaved channels out in
  drums/bass/other/vocals order. `src/stems/stemseparation.{h,cpp}` streams a whole track with 75%
  hops, a linear crossfade normalized by the summed weight, tail zero-padding, cancellation between
  windows and per-window progress. Tests use copy, per-stem-gain and failing backends and cover
  zero, one, short, exact-window and long inputs. A standalone run over 13 track lengths had a worst
  reconstruction error of 6e-8. No ORT dependency yet.
- **2026-11-12** — **W1/W2/W4/W5/W6 merged.** Controller mappings (#3), QML
  accessibility/localization (#4), beatgrid/stem golden fixtures (#6), model hosting (#5), and the
  QML Qt-API guard (#15) are on `main`. ADRs 001-003 (#8, #9, #14) and the 8-channel FLAC spike
  (#12) are also merged.
- **2026-11-12** — **Stem separation engine merged** (#13, `e0fad77`). Streaming `StemSeparation`
  with a backend seam; 15 tests. Linux CI exposed an uninitialized `SampleBuffer` accumulator in the
  first window; fixed by clearing it before accumulation and by checking the streamed output against
  the input directly. The fix landed on the PR branch before merge.
- **2026-11-12** — **rbxport beat grid engine added** (PR #16). C++ port of `rbl-analysis`
  onset/attack/tempo/downbeat/grid stages, registered as the third `AnalyzerBeats` plugin
  (`rbxport-analysis:0`) and exposed in Preferences → Beat Detection. Ported tempo tests plus
  synthetic end-to-end grid/plugin tests; 10 rbxport tests green. The PR was rebased onto current
  `main` so it carries only the rbxport change.
- **2026-11-13** — **UI overhaul started** on `feature/ui-overhaul` from `main` (`0b0a97e`).
  `res/qml/Theme/Theme.qml` is now a token layer: named schemes (`Paramix Dark`,
  `Paramix Midnight`, `Paramix High Contrast`), semantic surfaces/text/accents, a stem and phrase
  palette, type, spacing, radius, motion and layout metrics. Every legacy Mixxx property is kept
  as an alias so un-migrated components still work. `res/qml/skin.ini` is renamed to Paramix with
  a 1280×720 minimum. The flat base controls (`Button`, `ActionButton`, `FormButton`, `ComboBox`,
  `TextField`, `SectionBackground`, `SectionText`, `EmbeddedBackground`) are token-driven and keep
  their old public properties. Documented in `docs/paramix-ui-design-system.md`.
- **2026-11-13** — **Main window shell modernized.** Brand header, flat toolbar with accent active
  states, tokenized splitter and waveform edge fades. All toolbar buttons gained `Accessible.name`;
  no control bindings changed.
- **2026-11-13** — **Settings → Stems page added.** `res/qml/Settings/Stems.qml` selects the stem
  export format (FLAC/ALAC/AAC) and AAC bitrate (32–512 kbps), shows a read-only storage location,
  and a disabled model-quality section with an explanation instead of a dead control. Two new
  `Mixxx.Config` properties back it: `stemFileFormat` (`[Stems],FileFormat`, normalized through
  `mixxx::stems::parseStemFileFormat`) and `stemAacBitrate` (`[Stems],AacBitrate`, clamped). Phrase
  display was intentionally not added because beatgrid phrase storage is not implemented.
- **2026-11-13** — **Deck and stem strip overhauled.** `Deck/HotcueAndStem.qml` keeps the hotcue
  grid and adds a four-lane stem strip: colour chip, label and status dot; per-lane mute and solo;
  a vertical volume fader; a QuickFX knob with enable toggle and chain preset; acapella and
  instrumental complement buttons; and a `STEMS · N` / `NO STEMS` badge. Solo is QML-level mute
  orchestration that saves and restores the other lanes (a native engine solo control is a later
  change). Stem identity is classified from the model label with a four-stem fallback. Deck
  surfaces, header, loops, beatjump, tempo, spinny, play/cue and the utility toolbar now use the
  theme tokens; the hotcue/stem delegate grew to 132 px.
- **2026-11-13** — **Mixer, library, sampler and effects modernized.** Flat framed mixer strips with
  accent PFL and a segmented VU meter; library toolbar with the search field, a framed source tree,
  a flat track list with zebra rows, hover, accent selection and sort indicators, and a tokenized
  context menu; sampler cards with a track-colour accent; aux/mic, preview deck, effect units and
  preset menus on the same tokens. The library stem-status column is still omitted because
  `QmlLibraryTrackListColumn` has no stem role yet (TODO left in `Library/SourceTree.qml`). All
  changed QML passes `pyside6-qmllint` (ignoring unresolved `Mixxx.*` types) and the Qt 6.4 API
  guard.
- **2026-11-14** — **Rebranded the application as Paramix.** `VersionStore::applicationName()` now returns
  `Paramix`, which drives the window title, dialog titles, notifications, MusicBrainz user agent, JACK
  client name and console banner. The application domain is `paramix.org`, the package/bundle/Flatpak ID
  is `org.paramix.Paramix`, and the installed executable is `paramix`/`paramix.exe` (the CMake target
  stays `mixxx`). New assets: four-lane stem app icon, wordmark, symbolic lockup, PNG/ICO/ICNS/BMP
  output, and regenerated WiX dialog/banner and macOS DMG artwork. The About dialog keeps the Mixxx
  attribution, contributor history and GPL notice, and adds a "Based on Mixxx" line. User-visible
  strings across C++ and `.ui` files were rebranded; historical version references, upstream docs
  links, license headers and internal identifiers keep the Mixxx name. Settings now default to
  `~/.paramix` on Linux and the matching app-data directory elsewhere; the first-run migration wizard
  is still outstanding. Details: `docs/paramix-rebrand.md`.

- **2026-11-15** — **Stem waveform visuals rebuilt against the Traktor reference.** The analyzer
  gained per-stem detail (peak, RMS, low/mid/high/attack, packed into `WaveformStemColumn`,
  protobuf `signal_stem_details = 6`), a multi-resolution column pyramid built once per analysis,
  and a per-stem 95th percentile. `waveformrendererstem.cpp` now draws solid mirrored lanes
  (constant alpha, peak envelope + brighter RMS core, 0.75 px floor) scaled per stem instead of
  against a shared reference, gates muted lanes as ghosts, and derives colour from each column's
  own spectrum (amber lows, green mids, magenta highs) clamped to +/-0.09 hue around the lane's
  reference family, which reproduces the reference palette while content still drives the colour.
  `WaveformDisplay.qml` gained `res/qml/StemLaneStrip.qml`: per-lane colour key (mute toggle),
  uppercase name and gain knob on `[ChannelN_StemX],volume`, on a fading panel over the left edge.
  Drawn columns went from 6 frames / 24 px to 1.5 frames / 8 px so close-up detail is visible.
- **2026-11-15** — **The "junk at the start of a track" was the preroll marker, not the stem
  renderer.** `WaveformRendererPreroll` tiles a texture of small triangles across every frame before
  position 0 and after the end; with four lanes it is drawn four times vertically, which read as
  rows of repeating orange chevrons. A diagnostic run proved the data pipeline was healthy all
  along (8 channels, 76,305 columns, `hasStemColumns` true) and that the first ~4 s of `Wannabe` are
  genuinely near-silent in the stem cache. The marker was first replaced with a faint wash and then
  removed from the Paramix deck waveform entirely; the zone is empty background now.
- **2026-11-15** — **Deck layout: 160 px waveform band, two preview rows with four decks.** With the
  library visible the waveform strip is 160 px (was 120), taken from the library pane because the
  deck/mixer stack has fixed heights and the library is the only flexible item in that split. The
  strip now always renders only channels 1 and 2, each keeping half the height, even with four decks
  enabled; decks 3 and 4 have no waveform row (their panes are unchanged). `StemLaneStrip` rows are
  quarter-height so they stay aligned with the renderer's lanes.
- **2026-11-16** — **Qt Quick vsync is a setting.** `[Waveform],QtQuickVSync` (default true) is read
  in `main.cpp` before `QmlApplication` is constructed; when false it sets `QSG_NO_VSYNC=1`.
  Preferences > Waveform > Graphics API Status now has a "VSync" checkbox below the average frame
  rate (restart required). The pre-existing `[Waveform],VSync` key is the legacy `VSyncThread` mode
  and is left alone. Measured default: threaded render loop, D3D11 FLIP swapchain (max frame latency
  2), "Animation Driver: using vsync: 8.33 ms", 3884 frames in a 45 s run (≈120 fps once the UI is
  up). With the toggle off Qt logs "Swap interval is 0" and "animation driver switched to timer mode".
