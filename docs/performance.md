# Performance work — 2026-11-18

This pass targets end-user responsiveness without changing the final look of the
stem waveform. The stem detail reducer, colour mapping, lane layout and texture
contents are unchanged; only when and how they are scheduled changed.

## Goals

- Stop work that is not needed (preferences pages, per-cell UI, paused waveform
  repaints, duplicate cover requests).
- Remove work from the GUI/render thread and from the real-time audio callback.
- Keep the library data path proportional to visible cells, not to every
  column of every row.
- Add regression tests for the new caches.

## Changes

### Library track list

- `res/qml/Library/TrackList.qml`
  - Cells no longer declare `cover_art`, `decoration` or `file_url` roles.
    Only the cover column asks the model for its colour and image, through the
    new `getDecorationColor(row)` / `getCoverArtUrl(row)` invokables. The cover
    cell also reads a small model `revision` property so `dataChanged` still
    refreshes it after a track colour or cover edit.
  - Track proxies are shared per track id; the weak cache sweeps periodically
    instead of on every insert.
  - The drag preview moved from every cell to one shared, off-screen item in
    the list. A viewport no longer creates a preview, an image and a shadow per
    cell.
  - Model capabilities are resolved once per model, not once per cell.
- `res/qml/Library/Cell.qml`
  - Removed the per-cell drag preview, `DropShadow` and cover `Image`.
  - The drag payload uses `track.trackLocationUrl` instead of the `file_url`
    role.
- `res/qml/Library/SourceTree.qml`
  - The cover image requests a small `sourceSize` (2× the cell) instead of
    decoding a full cover for every visible row.
- `src/qml/qmllibrarytracklistmodel.{h,cpp}`
  - Track proxies are shared per track id (weakly cached with `QPointer`), so
    eight cells of one row reuse one `QmlTrackProxy` instead of constructing
    eight proxies and their cue/beat/stem models.
  - The `Track` role answers from the cache by track id before loading the
    track.
  - `has_stems` uses the track id set and the source model's id lookup; the
    fallback only loads the track when the id is not in the set.
  - `refreshHasStems` uses the track model's track-id → rows map instead of
    scanning the entire library for every finished separation job. A proxy
    model keeps the scan fallback.
- `src/qml/asyncimageprovider.cpp`
  - Added a bounded (32 MB), mutex-protected cache of decoded/scaled cover
    images, keyed by URL, requested size and the track file's modification
    time. Embedded cover art therefore refreshes after a tag edit; a separate
    cover image edited in place is refreshed on the next restart.
  - The provider pool is capped at 2–6 threads so a fast scroll cannot queue
    dozens of threads blocked on the main-thread track lookup.

### Waveform display

- `src/qml/qmlwaveformdisplay.{h,cpp}`
  - The update loop stops when there is no track, so an empty deck no longer
    repaints at the panel rate.
  - A paused deck runs at 30 Hz instead of the monitor rate after a 300 ms
    interaction grace period. Play, scratch, cue, loop and zoom changes wake
    the full-rate loop immediately.
  - Zoom and external position changes request a paint directly.
  - `track_samples` changes reconnect the loop after a track load through an
    item-owned `ControlProxy`, so rebuilding the renderer stack cannot drop the
    wake-up connection.
  - A group change requests a frame immediately.
- `src/qml/qmlspinnyposition.{h,cpp}`
  - The platter position item only requests a new frame while the deck is
    playing or scratching. A paused platter still refreshes its QML angle when
    another frame happens, but it no longer drives the window at vsync.
- `res/qml/main.qml`
  - The splash dot animations stop once the splash fades out. They previously
    ran forever in the item tree and kept waking the render loop.
- `res/qml/Library/TrackList.qml`
  - The stem badge is created by a `Loader` only on rows that have stems; the
    previous unconditional `Repeater` and three rectangles existed in every
    cell of every column.

### Stem detail pipeline (visual output unchanged)

- `src/waveform/renderers/allshader/waveformrendererstem.cpp`
  - Caches the track's stem count, duration and sample rate on the render
    thread and refreshes them once a second (or when the track changes). This
    removes several `Track` mutex acquisitions from every rendered frame while
    still picking up stems that finish separating after the track was loaded.
- `src/waveform/stemdetail/stemdetailbuilder.cpp`
  - Decode blocks are 4× larger (16,384 frames). The reducer is sample-by-sample,
    so the result is identical; fewer read calls reduce per-chunk overhead.
  - A column is kept in the compact form when all of its non-zero rows quantise
    to the same pixel colour, not only when their float entries are identical.
    This avoids allocating the whole-track `rowColors` arena for the common
    case while writing exactly the same pixels. The golden renderer
    fingerprint test covers the change.
- `src/waveform/stemdetail/stemdetailcache.{h,cpp}`
  - `request()` is called from the render thread every frame. It now remembers
    the last queued key and does not post another queued invocation while that
    build is pending or running.
  - Builds are cancelled by generation token. A cancel issued before a queued
    build starts now aborts that build instead of being cleared at entry, so a
    superseded key cannot run a full-file decode.
  - The debounce is 250 ms for a shape change (zoom) and 800 ms when only
    magnification or lane height changed. The final picture is still built for
    the final key; a pregain sweep or live resize no longer starts a full
    decode in the middle of the gesture.
  - Completed/failed builds clear the queued-key marker so a later revisit can
    schedule again.

### Real-time audio

- `src/engine/channels/enginedeck.cpp`
  - The stem mix buffer is allocated once in the constructor for the maximum
    engine block size. `processStem()` no longer allocates on the audio thread.
    A debug assertion documents and enforces the bound.

### Startup

- `src/main.cpp`
  - The pixmap cache limit is applied before either UI path; it was previously
    only set when the classic widgets UI was loaded.
- `src/qml/qmlapplication.cpp`, `src/qml/qmldlgpreferencesproxy.{h,cpp}`
  - The preferences dialog is created on first `PreferencesDialog.show()`
    instead of during startup. The dialog builds every preference page, which
    is unnecessary until the user opens it.
  - Added `QmlDlgPreferencesProxyTest` to assert that the factory is called
    once and only on the first show.

## Verification

```bash
# Formatting and repository checks
./tools/paramix_verify.sh

# New model cache test
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='*QmlLibraryTrackListModelTest*'

# New lazy-preferences test
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='*QmlDlgPreferencesProxyTest*'

# UI startup and layout
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='*QmlStartupSmokeTest*'
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='*QmlResponsiveBoundsTest*'

# Populated-library layout audit (optional)
PARAMIX_QML_PROFILE_DB=/path/to/mixxxdb.sqlite \
PARAMIX_QML_DUMP_DIR=/tmp/paramix-dump \
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='*QmlResponsiveBoundsTest*'
python3 tools/qml_layout_audit.py /tmp/paramix-dump

# Library and engine regression tests
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='*Library*-*Qml*'
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='*EngineMixer*:*EngineBuffer*:*EngineDeck*'
```

The QML tests create process-wide QML singletons; run each QML test filter in
its own process. The two-parameter startup test is known to abort when both
parameters share a process.

## Measurements and expectations

- The QML tree for the empty 1792×1008 window is about 1,780 visible items.
  With a 29-track library (`PARAMIX_QML_PROFILE_DB` audit mode) it is about
  3,132 visible items, including 336 `Text` and 268 `Loader` items. The cell
  changes remove roughly 14 items, one shader effect and one image from every
  cell that previously paid for a hidden drag preview, a cover image, a shadow
  and an unconditional three-bar stem badge. The populated tree still has only
  19 shader effects (the same as the empty window), so shadows no longer scale
  with the number of rows.
- `QmlStartupSmokeTest` is about 4.5–4.7 s on this x86_64 macOS build after the
  changes. Startup timing is noisy on the first run after a link (binary and
  font caches); compare warm runs and record the power/thermal state.
- The populated-library responsive bounds test and the QML layout audit pass
  at all five audited sizes. The audit's remaining findings (knob shadow
  images a few pixels outside their item, small caption boxes and header
  elision) predate this pass.
- The 30 Hz paused-waveform path is a behaviour change in refresh rate, not in
  appearance. A paused waveform is static; play/scratch/marker/zoom changes
  still run at full rate.
- The stem detail cache still rebuilds when the viewport (zoom, lane height) or
  magnification (deck pregain) changes. The scheduling and decode changes only
  reduce the cost; they do not alter the generated picture.

## Remaining performance backlog

1. Build the stem detail texture off the render thread (the `QImage` fill and
   upload are still in `updatePaintNode`). Keep the bytes identical and move
   only the fill; QSG texture creation must stay on the render thread.
2. Separate stem detail shape from display magnification so pregain changes stop
   re-decoding the file. This needs a pixel-exact golden test before landing.
3. Move library search/sort off the GUI thread or add an FTS5 index after
   measuring 10k/50k-track libraries.
4. Check stored analyses before opening the audio source for ad-hoc analysis on
   load; avoid a duplicate 8-channel decode per loaded track.
5. Compile the skin QML ahead of time (`qt_add_qml_module` or qmlcachegen) in
   packaged builds and measure cold start.
6. A/B the Qt 6.4 TableView compatibility mode on Qt 6.5+ and keep the faster
   implementation if the QML behaves.
7. Add a repeatable perf harness (`--perf-report`) with frame-time histograms,
   RSS, xrun counts and startup milestones; gate microbenchmarks and RT
   allocation in CI.
8. Evaluate release-only LTO/PGO after those measurements; do not ship
   `-march=native`.
