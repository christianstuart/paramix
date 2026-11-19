# F12 - Downbeat lines on stem and non-stem waveforms

- **Branch:** `bradley/fix-downbeats`
- **Worktree:** `C:\Users\Bradl\paramix-fx-waveform` (reused after F05 merges)
- **Lane / wave:** Waveform lane, wave 3
- **Depends on:** F01 and F05 merged (lane order and `WaveformDisplay.qml` ownership)
- **Blocks:** nothing
- **Needs its own build:** yes (C++)

## Owned files

- `src/waveform/renderers/allshader/waveformrenderbeat.{h,cpp}`
- `src/qml/qmlwaveformrenderer.{h,cpp}` - the beat renderer wrapper only
- `res/qml/WaveformDisplay.qml` - the `WaveformRendererBeat` block only
- `src/track/beats.{h,cpp}` - only if a beat-index helper is missing; ask the integrator first,
  this path is collision-prone
- New test under `src/test/`

## What is known

- One beat renderer serves every deck, stem or not. It walks `trackBeats->iteratorFrom(start)` and
  draws 1 px rectangles; in split-stem mode it draws one segment per lane. It has no notion of
  bars.
- The analysis engine (PR #54, on `main`) adds `Beats::getDownbeatInfo()` returning
  `DownbeatInfo { valid, beatsPerBar, phase, confidence, source }`. It is stored as fragments in
  the beats sub-version string (`|db=1|dbbar=..|dbphase=..`), so older builds ignore it. Nothing
  reads it yet.
- The engine is an **opt-in** beat plugin (`paramix-analysis-engine:1`, listed last; Queen Mary
  stays the default). Tracks analyzed with the default plugin have no downbeat info.
- ADR-003 says a missing bar field means unknown, not 4/4.

## Decisions (defaults; integrator confirms)

- With valid downbeat info: thick line where `(beatIndex - phase) mod beatsPerBar == 0`.
- Without it: `Fixes.md` asks for a thicker line every four beats, so count from the grid's first
  beat in 4/4 and draw those lines in the same width but lower opacity, to mark them as assumed.
  This keeps ADR-003's meaning in the data and still gives every track bar lines.
- Changing the default beat plugin, or re-analyzing the library, is not part of this plan.

## Steps

1. Read the merged `beats.h` on `main` and confirm the `DownbeatInfo` semantics: what `phase`
   counts from, and how it behaves for beat maps with tempo changes and after grid translate /
   scale / halve / double operations.
2. Get a stable beat index for each drawn beat. The iterator starts at the first visible beat, so
   the index must come from the grid (frames from the anchor divided by beat length for constant
   grids; marker counts for beat maps), not from a loop counter that restarts with the viewport.
3. Renderer: draw downbeats about 3 px wide (scaled by device pixel ratio) in the beat color at
   higher opacity; keep ordinary beats as they are. Both stem lanes and the non-stem path go
   through the same code.
4. Expose `downbeatColor` / `downbeatWidth` on the QML wrapper and bind them in
   `WaveformDisplay.qml` from `Theme`.
5. Tests: index and downbeat selection for a constant grid with phase 0-3, for 3/4, for a grid
   whose first beat is before frame 0, and for the no-info fallback; index stable across
   scrolling.
6. Check what the grid-edit controls (shift, halve, double) do to the phase and file an issue if
   editing desynchronizes the bar lines; do not fix grid editing here.

## Verification

- New tests pass.
- Grabs: a track analyzed with the Paramix engine (thick lines on audible downbeats), a Queen Mary
  track (assumed lines), stem and non-stem, at three zoom levels. At far zoom the thick lines must
  not merge into a band; thin them out if they do.
- No measurable frame-time change.

## Done when

Every deck shows a thicker line at each bar, aligned with the analysis engine's downbeats when the
track has them.
