# Audio dropout diagnostics

Issue #61 reports a full audio pause under CPU load. It was seen once and has
not been reproduced. This document describes the diagnostics and load-shedding
that ship for it, and the manual recipe for capturing the next occurrence.

## What is logged

The audio callback must not log, lock or allocate. Instead it only bumps
lock-free counters:

- `CachingReader::cacheMissCount()` counts chunk cache misses, i.e. the deck
  asked for audio that the background reader had not decoded yet. This is the
  most likely cause of a *sustained* pause, because a miss is filled with
  silence.
- `mixxx::audio::diagnostics::recordUnderflow(code)` stores the code passed to
  `SoundManager::underflowHappened()` (`src/soundio/audiodiagnostics.h`). The
  PortAudio callback used to `qWarning()` this code in developer mode; that
  print moved out of the callback.

`StemAnalysisCoordinator`, which already ticks every 500 ms on the GUI thread,
watches `[App],audio_latency_overload_count`. Whenever it advances, it writes
**one rate-limited line per second**:

```text
Warning [StemAnalysisCoordinator] Audio dropout: underflow_code= 19 [App],audio_latency_usage= 0.94 cache_miss_delta= 0 stems= active load_analysis= active batch_analysis= suspended stem_detail= idle audio_latency_overload_count= 7
```

A reader underrun can feed silence through a healthy device callback, in which
case PortAudio never raises an underflow code. To keep that case visible, the
same line is also written as `Audio starvation (no device underflow):` when at
least 4 cache misses arrive in one 500 ms tick and the overload counter did not
advance. That threshold is a heuristic; a seek can also miss a chunk or two, so
a single starvation line is not proof of a pause. It is not a measurement of
silence.

| Field | Meaning |
| --- | --- |
| `underflow_code` | Last code passed to `underflowHappened()` by the device callback. |
| `[App],audio_latency_usage` | The existing 1 s callback-time average. `1.0` means the callback used its whole buffer period. |
| `cache_miss_delta` | Cache misses since the previous dropout line. |
| `stems` | Stem separation worker has queued or running work. |
| `load_analysis` | Ad-hoc analysis of a loaded track: `active`, `idle` or `suspended`. |
| `batch_analysis` | Batch analyzer: `active`, `idle` or `suspended`. |
| `stem_detail` | Stem detail builder: `active`, `idle` or `suspended`. |
| `audio_latency_overload_count` | The monotonic overload counter at the time of the report. |

The line is written to `mixxx.log` in the Mixxx settings directory (on Windows
usually `%LOCALAPPDATA%\Mixxx\mixxx.log`).

The line is a report, not a verdict. A full pause can also come from a driver
that stops calling the callback, in which case no dropout line appears at all.
Absence of lines is therefore not proof that the pause did not happen.

## Reading the counters

- `cache_miss_delta > 0`: reader starvation. The file I/O or decode worker fell
  behind, or the worker did not get CPU. The engine then fed silence to the
  device.
- `cache_miss_delta == 0` and usage near `1.0`: the callback itself ran out of
  time. Look at CPU contention, a lock on the callback path or GPU/render
  contention.
- Low usage with codes `7`, `11` or `19`: the device or its driver underflowed
  outside the time the callback is given.

## Load-shedding

`StemAnalysisCoordinator` uses one hysteresis state machine for all background
owners:

1. Every 500 ms it reads `[App],audio_latency_usage`.
2. Above `0.85` for 3 consecutive checks (1.5 s), it pauses stem separation,
   load analysis, batch analysis and stem-detail builds, and records that it
   owns the pause.
3. Below `0.60` for 2 consecutive checks (1 s), it releases exactly the pauses
   it owns. Values in between reset both counters and do not change state.

Each owner is idempotent and is only called on a transition:

- `StemService::setBackgroundPaused()` composes with the user pause. The worker
  stops at its next block; the job stays queued.
- `PlayerManager::setBackgroundAnalysisSuspended()` suspends the ad-hoc
  analyzer. Tracks scheduled while suspended stay queued, and a track loaded
  during the suspension does not clear it.
- `StemDetailCache::setBackgroundSuspended()` stops new detail builds. A
  running build is allowed to finish; a deferred request stays pending and is
  started exactly once after resume.
- `AnalysisFeature::setBackgroundSuspended()` receives the union of "stems are
  busy" and "audio is overloaded", so one owner cannot clear the other.

Load analysis now uses the same worker limit as batch analysis:
`max(1, min(8, idealThreadCount / 2))`, with `AnalyzerModeFlags::LowPriority`.
The stem detail builder thread also runs at `QThread::LowPriority`.

Post-separation re-analysis is deferred by 2 s after the stem cache becomes
ready, instead of starting waveform and gain analysis while the separation
worker is still handing the job back.

## Manual reproduction recipe

The harness creates load only; the audio workload is still manual.

1. Use a build with stem separation enabled and a real audio device, not the
   dummy device.
2. Load lossless 44.1 or 48 kHz tracks into decks 1 and 2 and play both. Leave
   the crossfader and EQ neutral, no effects.
3. Queue two or three tracks for stem separation and let it run for the whole
   test.
4. Every 10 seconds load a different track into a stopped deck or a sampler.
   This runs the load-triggered analyzer repeatedly.
5. Use a small buffer: 128 frames (~2.9 ms at 44.1 kHz) if the device accepts
   it, otherwise 256 frames. Try the device API that was in use when the pause
   was heard:
   - Windows: WASAPI shared, WASAPI exclusive, WDM-KS
   - Linux: ALSA and JACK
   - macOS: CoreAudio
   Record the API and buffer size in issue #61.
6. While audio is playing, run:

   ```bash
   python tools/paramix_audio_stress.py --cores <logical cores - 1> --duration 180
   ```

   The tool is standard-library only and does not use the network. It starts
   one busy Python process per core, runs for the duration, and then parses the
   part of `mixxx.log` that was appended during the run.

7. Attach the `Audio dropout:` or `Audio starvation` lines and the device
   API/buffer size to the issue. If the tool cannot find the log, pass
   `--log <path>`; if no report line appears, say so explicitly rather than
   reporting "no dropout".

The tool reports the overload-count delta, the underflow codes, the peak
latency usage, the largest cache-miss delta, and the longest gap between
dropout reports. The last value is a proxy only: reports are capped at one per
second, so it is not a sample-accurate silence measurement.

## What was not verified

- No C++ toolchain, Qt, build or audio device was available in the worktree
  that produced this change. Nothing here was compiled or executed.
- The pause has not been reproduced, so no root cause is claimed.
- The counters and the 1 Hz line were written against the existing engine and
  device code, not observed on a live dropout.
- The dropouts are not persisted in the database, so the harness reads only the
  log.

## Known callback-path finding

`CachingReader::read()` still contains the pre-existing
`Counter("CachingReader::read(): Failed to read chunk on cache miss")++` on the
same cache-miss branch. Constructing that `Counter` allocates a `QString` on
the audio callback and `Stat::track()` looks the tag up in the stats manager.
It is left unchanged in this change (scope: counters only) and reported in the
F03 worklog. The new `CachingReader::cacheMissCount()` counter next to it is a
plain relaxed atomic.

## Resolved: allocation on the reader cache-miss path

The pre-existing Counter(...)++ on CachingReader::read()'s miss branch built a
QString and a StatReport on the audio callback. It has been replaced by a
relaxed atomic increment; the developer stat is now updated from the 1 Hz
diagnostics tick. The reader path no longer allocates when the reader starves.

