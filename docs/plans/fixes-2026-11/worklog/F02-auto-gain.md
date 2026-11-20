# F02 worklog - auto-gain not applied

## Confirmed causes

1. `EnginePregain` creates the shared `[ReplayGain]` controls at their neutral 0 dB default.
   Only `DlgPrefReplayGain::slotApply()` copies the stored `InitialReplayGainBoost` /
   `InitialDefaultBoost` / `ReplayGainEnabled` values into them. The dialog is built lazily,
   so on a normal session an unanalysed track played at unity and the target boost never
   reached the engine. The control's own value is not persisted either
   (`ControlPotmeter`/`ControlObject` default `bPersist == false`), so nothing restored it.
2. Late analysis results were deferred to the next stop (`m_replaygainPending`), although
   `EnginePregain::process()` already ramps a changed ratio over one second.
3. `AnalyzerEbur128` measured the raw 8-channel stem signal; `AnalyzerGain` mixes stems down
   to stereo first.

## What changed

- `EnginePregain` now takes the `UserSettingsPointer` and seeds the three shared controls from
  `ReplayGainSettings` when it creates them. The preferences dialog still writes them on apply.
- Late analysis results are applied while the deck plays: `slotSetReplayGain()` no longer defers
  to the next stop, and `m_replaygainPending` / `slotPlayToggled` are gone.
- `AnalyzerEbur128` measures 8-channel stem sources on their stereo mixdown, matching
  `AnalyzerGain`, so the stored loudness matches what the deck outputs.
- `EnginePregainTest.SeedsReplayGainControlsFromStoredSettings` now also drives
  `EnginePregain::process()` and asserts the default-boost path, not just the control values.

## Scope review (files outside the plan's owned list)

- `src/engine/channels/enginedeck.cpp` - required: it is the only `new EnginePregain(...)` call
  site and had to pass `pConfig`. `git grep "EnginePregain("` shows no other caller.
- `src/mixer/basetrackplayer.h` - required by the deletion of `slotPlayToggled()` and the
  `m_replaygainPending` member. `git grep "slotPlayToggled\|replaygainPending"` is empty, and
  `m_pPlay` is still used elsewhere, so nothing dangles.
- `src/test/signalpathtest.h` - test-harness only, see the CI diagnosis below.
- `CMakeLists.txt` - the one-line test registration the plan allows.

## CI diagnosis

### Repository checks / clang-format on changed C++ files

The job formats **whole files** (`git diff --name-only base HEAD` then
`clang-format --dry-run --Werror --style=file "${files[@]}"`), not just the changed lines.
`src/engine/enginepregain.cpp`, `src/mixer/basetrackplayer.cpp` and
`src/mixer/basetrackplayer.h` carried pre-existing deviations (`//Replay Gain things`,
a mis-indented `ConfigKey(...)`, `if(m_pConfig->getValue(`, `bool){};`), so touching them made
the whole-file check red. Fixed by running the pinned 19.1.3 formatter with the repo
`.clang-format` over every C++ file this branch changes; the dry run is now clean.

### Linux x64 / Test

No job logs are reachable for this run, so this was diagnosed from the diff and the test tree
(`ctest` exits 8 when at least one test fails). The bytes that assert on deck output are the
reference-buffer tests:

- `src/test/enginebuffertest.cpp` - `EngineBufferE2ETest` (via `SignalPathTest`) compares
  `EngineMixer::getMainBuffer()` with the checked-in goldens in `src/test/reference_buffers/`
  at a 1e-4 tolerance (`assertBufferMatchesReference` in `src/test/signalpathtest.h`).
- `src/test/stemcontrolobjecttest.cpp` - the same helper, STEM builds only.

**Most likely failure:** those goldens were recorded while the shared `[ReplayGain]` controls
sat at unity. Before this fix `EnginePregain` left `DefaultBoost` at `1.0` and
`ReplayGainEnabled` at `0.0` (both controls are non-persistent) until the preferences dialog
ran. After the fix the first deck seeds `ReplayGainEnabled = 1` and
`DefaultBoost = db2ratio(-6) = 0.5012`, and `sine-30.wav` has no ReplayGain tags, so
`EngineBuffer` sets `[ChannelN],replaygain = 0`. `EnginePregain::process()` therefore takes
the "predicted gain" branch and multiplies every sample by 0.5012, so every golden mismatches.
This is consistent with the local Windows runs passing (only the new unit test, `AudioTaperPot`,
`EngineMixerTest` - which uses mocked channels - and the non-E2E `EngineBufferTest` fixture were
run, and none of those compare deck audio) and with the failure appearing only in the Linux job,
which is the only job that runs `ctest`.

Options considered:

1. Regenerate `src/test/reference_buffers/*`: the correct end state, but it needs a built
   `mixxx-test` to emit the `.actual` files. This lane has no C++ toolchain, Qt or CMake, so it
   is left as a follow-up.
2. Pin the pregain correction in the shared fixture: `BaseSignalPathTest` now sets
   `[ReplayGain],ReplayGainEnabled = 0` right after it creates the decks. That reproduces the
   correction of `1.0` the goldens were recorded with, keeps the signal-path tests about
   buffer/rate behaviour rather than gain policy, and also covers the STEM goldens. The new
   `EnginePregainTest` covers the seeded values and the default-boost path directly, so the
   behaviour change is still tested where it belongs.

### Hypotheses checked and rejected

- Null `pConfig` in `EnginePregain`: guarded; the engine keeps its old defaults if it is null.
- Duplicate control creation: the `[ReplayGain]` creators are only made on the first deck
  (`s_pReplayGainBoost == nullptr`). A second `ControlAudioTaperPot` for the same key would
  silently bind the default control, but that needs a deck teardown followed by a new deck in
  the same process, which neither the app nor the test harness does.
- `AnalyzerEbur128`: no test initialises it for more than two channels, and the 3-7 channel
  early return mirrors `AnalyzerGain`'s existing `DEBUG_ASSERT` branch. It is not on the
  failing path; the per-buffer mixdown allocation matches `AnalyzerGain` too.
- Removed `slotPlayToggled()` / `m_replaygainPending`: no references remain, no signal is left
  connected, and `m_pPlay` is still used by eject/unload.

## Commands run

```powershell
git diff --stat origin/main...HEAD
git diff origin/main...HEAD -- src/ CMakeLists.txt
git grep -n "EnginePregain(" -- src/
git grep -n "slotPlayToggled|replaygainPending" -- src/   # no matches
clang-format -i --style=file <every changed .cpp/.h>
clang-format --dry-run --Werror --style=file <every changed .cpp/.h>   # exit 0
```

Tool: `C:\Users\chris\AppData\Roaming\Python\Python313\Scripts\clang-format.exe`
(19.1.3, matching `.pre-commit-config.yaml` and the CI pin).

## Remaining unverified

- The failing ctest test name could not be read from the run (job logs require
  authentication). The hypotheses above are ranked by evidence, not confirmed by a log.
- No build or test run was possible in this worktree (no C++ toolchain, Qt or CMake). The new
  assertions in `EnginePregainTest` and the `BaseSignalPathTest` pin were written against the
  code, not executed.
- `src/test/reference_buffers/*` still encode "ReplayGain off". Regenerating them with the new
  defaults is a follow-up if the signal-path tests should exercise the boost.
- Tracks already analysed as stems keep their old ReplayGain until re-analysed.
- No main-output limiter; the plan keeps that out of scope.
