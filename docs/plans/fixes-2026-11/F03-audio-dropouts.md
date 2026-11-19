# F03 - Audio pauses under high CPU load

- **Branch:** `bradley/fix-audio-dropouts`
- **Worktree:** `C:\Users\Bradl\paramix-fx-engine` (reused after F02 merges)
- **Lane / wave:** Engine lane, wave 2
- **Depends on:** F02 merged; F01 merged (shares `stemdetailcache.cpp`)
- **Blocks:** nothing
- **Needs its own build:** yes (C++)

This was seen once and not reproduced. The plan is investigation first; the mitigations in step 3
are safe to land even without a repro, and the diagnostics make the next occurrence explainable.

## Owned files

- `src/mixer/playermanager.cpp` (analyzer thread count only)
- `src/stems/stemanalysiscoordinator.{h,cpp}`
- `src/soundio/soundmanager.{h,cpp}`, `src/soundio/sounddeviceportaudio.cpp` (logging only)
- `src/engine/cachingreader/*` (counters only)
- `src/waveform/stemdetail/stemdetailcache.cpp` (thread priority only)
- New `tools/paramix_audio_stress.py` and a short `docs/audio-dropout-diagnostics.md`

## What is known

- A full pause, as opposed to crackle, is most likely sustained silence from reader starvation:
  a `CachingReader` miss fills the buffer with silence, and a PortAudio underflow outputs silence.
- Load analysis on track load uses `max(1, idealThreadCount / 2)` threads with no cap
  (`playermanager.cpp`), unlike batch analysis, which is capped at `min(8, cores / 2)` and runs at
  low priority.
- `StemAnalysisCoordinator` pauses stem separation above 0.85 `[App],audio_latency_usage`, but does
  not throttle load analysis or stem-detail builds.
- A finished stem job triggers re-analysis (waveform, gain), which stacks on top of playback.
- Existing signals: `[App],audio_latency_usage`, `[App],audio_latency_overload`,
  `[App],audio_latency_overload_count`, and `underflowHappened(code)`.
- The audio callback thread is `TimeCriticalPriority`; no lock on the callback path was found, but
  this was not traced exhaustively.

## Steps

1. **Diagnostics.** Log each underflow with its code, the latency usage, cache-miss count and
   which background jobs were active (stems, load analysis, batch analysis, stem detail), rate
   limited to one line per second. Add a cache-miss counter to the reader. No allocation or
   locking on the callback thread: count there, log from the existing ~1 Hz update.
2. **Repro harness.** `tools/paramix_audio_stress.py` starts external CPU load on N cores and
   reports the app's overload count over a run; document the manual recipe (two decks playing,
   stem separation running, track loads every 10 s, small buffer). Try to reproduce on the
   owner's machine with the device API and buffer size from the day it happened.
3. **Mitigations.**
   - Cap load-analysis threads at the same `min(8, cores / 2)` and run them at low priority.
   - Extend the coordinator so high latency usage also suspends load analysis and stem-detail
     builds, with hysteresis so they resume when usage falls.
   - Stagger the post-separation re-analysis instead of starting it at once.
4. If the repro shows something else (a lock on the callback path, device API behavior, GPU
   contention from the render thread), stop and report to the integrator before widening scope.

## Verification

- Stress run before and after: overload count and longest silent gap reported by the harness.
- No change in analysis results; batch analysis time not more than 10 % slower when audio is idle.
- Real-time rules hold: nothing added to the callback allocates, locks or logs.

## Done when

Diagnostics are in, background work backs off under audio pressure, and either the pause is
reproduced and fixed or the document states what was tried and how to capture it next time.
