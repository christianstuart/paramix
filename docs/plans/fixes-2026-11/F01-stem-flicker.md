# F01 - Stem waveform flickers while the track is paused

- **Branch:** `bradley/fix-stem-flicker`
- **Worktree:** `C:\Users\Bradl\paramix-fx-waveform`
- **Lane / wave:** Waveform lane, wave 1
- **Depends on:** wave 0 (synced `main`)
- **Blocks:** F05 and F12 (same lane)
- **Needs its own build:** yes (C++)

## Owned files

- `src/waveform/renderers/allshader/waveformrendererstem.{h,cpp}`
- `src/waveform/stemdetail/stemdetailcache.{h,cpp}`, `stemdetailbuilder.{h,cpp}`
- `src/qml/qmlwaveformdisplay.{h,cpp}`
- `src/waveform/renderers/waveformwidgetrenderer.cpp` (only if the cause is in `onPreRender`)
- `src/test/stemdetailrenderertest.cpp`

Do not edit `res/qml/WaveformDisplay.qml` (owned by F13, then F05, then F12). If a QML change is
unavoidable, hand it to the integrator as a one-line patch for the lane that currently owns the file.

## What is known

- PR #55 (`fix/stem-waveform-stability`, merged 2026-11-19) already retains the last stem picture
  during a detail rebuild and skips stale negative visual positions on a paused deck. The report in
  `Fixes.md` may predate it. **Step 1 is to re-test on current `main`.**
- The display repaints on every presented frame even when paused (`slotFrameSwapped` -> `update()`),
  and the stem renderer re-runs its preprocess each time.
- Ranked candidates if it still reproduces:
  1. The stem texture uses nearest filtering and a fractional UV column offset, while column pick
     uses `llround(sample / detailSamplesPerColumn)`. A sub-column wobble in the visual position
     alternates texels between frames.
  2. `StemDetailCache` finishing a debounced rebuild and swapping the picture (`detailReady` ->
     `markDirtyGeometry()`).
  3. `samplesPerColumn` twitching by rounding, which changes the cache key and forces a full
     texture rebuild.
  4. The adaptive 30 fps cap while stems or analysis are running (`backgroundWorkActive`).
  5. `VisualPlayPosition` buffer selection: the engine still writes a non-zero `positionStep`
     every callback while paused.

Line numbers in the research were taken from a stale tree; re-locate them on current `main`.

## Steps

1. Reproduce on current `main` with a stem track: paused at several zoom levels, with and without
   a separation job running. Record whether #55 fixed it. If it no longer reproduces in any case,
   stop, write the result to the worklog and close this plan with a regression test only (step 4).
2. Add temporary logging in the stem renderer of the values that feed the picture each frame
   (visual position, first column, UV offset, cache key, texture generation). A paused deck must
   produce identical values frame to frame; the one that changes is the cause.
3. Fix at the cause:
   - Position wobble: snap the column start to whole detail columns when the play rate is zero, and
     early-out of the geometry/UV update when position, zoom, size and texture generation are
     unchanged since the last frame.
   - Cache swaps: do not issue a new `request()` while the key is unchanged; keep the #55
     retain-last-picture behavior.
   - Key twitch: derive `samplesPerColumn` from the zoom setting, not from the rounded viewport.
4. Add a test to `stemdetailrenderertest.cpp`: N consecutive preprocess passes at a fixed position
   and rate 0 produce identical geometry and no texture rebuild.
5. Remove the temporary logging.

## Verification

- `mixxx-test --gtest_filter=StemDetail*` passes; new test fails without the fix.
- Manual: paused stem track at min, default and max zoom, on both decks, during a separation job.
- No frame-time regression while playing (compare QSG timing before/after for 30 s).

## Done when

A paused stem deck renders a stable picture in all the cases above, and a test pins it.
