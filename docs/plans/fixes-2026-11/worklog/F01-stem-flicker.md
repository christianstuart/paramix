# F01 worklog - stem flicker while paused

## Re-test on current `main` (848c185)

Manual visual re-test is not possible on this machine: the QML tests and the app cannot render
off-screen here (`QmlStartupSmokeTest` and `QmlResponsiveBoundsTest` both die with an access
violation in the test body on unmodified `main`, and there is no display). The owner must do the
paused-deck re-test at min, default and max zoom, on both decks, with and without a separation job.

## Code review against the ranked candidates

The fixes from #55 are present on `main`:

1. Sub-column wobble: `WaveformRendererStem` keeps a `GeometryKey` and returns before rebuilding the
   quads when it is unchanged (`waveformrendererstem.cpp`, the `key == m_geometryKey` early-out).
   A repeated frame at a fixed position and rate 0 therefore does not update the geometry.
2. Detail cache swaps: `StemDetailCache::request()` remembers the last scheduled key and returns the
   current picture without posting another queued build (`m_lastScheduledKey`). `onDetailReady()`
   only marks the geometry dirty.
3. Key twitch: `samplesPerColumn` is still derived from the rounded viewport
   (`lround((lastSample - firstSample) / pixelLength)`), but `firstSample` and `lastSample` are
   constant while paused, so the key is stable. No evidence of a twitch, so it is left unchanged.
4. Frame cap: unchanged.
5. Negative visual position on a paused deck: guarded by the `getTruePosSample(...) < 0` early-out,
   which retains the last picture.

No code change is justified without a reproduction: the only F01 edits left would be guesses.

## Regression test

Step 4 asks for "N consecutive preprocess passes at a fixed position and rate 0 produce identical
geometry and no texture rebuild". The current test binary has no `WaveformWidgetRenderer` fixture
and no render-graph context, and `preprocessInner()` needs both plus a stem detail cache with a
built picture. Building that harness is a separate piece of work; the existing detail-pipeline test
(`stemdetailrenderertest.cpp`) already pins the picture itself, including the golden fingerprint.

## Decision

Leave the renderer as is unless the owner reproduces the flicker. If it reproduces, the next step is
the temporary per-frame logging from the plan (position, first column, UV offset, cache key,
texture generation) on the owner's machine.
