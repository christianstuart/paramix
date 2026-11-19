# F02 - Auto-gain not applied; playback clips without a manual gain change

- **Branch:** `bradley/fix-auto-gain`
- **Worktree:** `C:\Users\Bradl\paramix-fx-engine`
- **Lane / wave:** Engine lane, wave 1
- **Depends on:** wave 0
- **Blocks:** F03 (same lane, same worktree)
- **Needs its own build:** yes (C++)

## Owned files

- `src/engine/enginepregain.{h,cpp}`
- `src/preferences/replaygainsettings.{h,cpp}`
- `src/preferences/dialog/dlgprefreplaygain.cpp`
- `src/mixer/basetrackplayer.cpp` (ReplayGain apply logic only)
- `src/analyzer/analyzerebur128.{h,cpp}`
- New test file under `src/test/` (plus its one-line `CMakeLists.txt` entry, see master plan)

No QML in this fix. A deck gain indicator is a follow-up, not part of this plan.

## What is known

The ReplayGain path is intact: analysis runs on load (`PlayerManager` -> scheduler), the ratio is
stored, `EngineBuffer` writes `[ChannelN],replaygain` on load, and stem decks still go through
`EnginePregain`. Ranked causes:

1. **`[ReplayGain]` engine controls are only initialized by the preferences dialog.**
   `InitialDefaultBoost` (-6 dB) and `InitialReplayGainBoost` are read in
   `DlgPrefReplayGain::slotApply`. The `[ReplayGain],DefaultBoost` control itself starts at 1.0
   (0 dB). If Paramix constructs the classic dialog lazily, the stored values never reach the
   engine on a normal session: unanalyzed tracks play at unity and the target boost is not
   applied. Whether the dialog is lazy is the first thing to confirm.
2. **Analysis finishing while the deck plays is deferred** (`m_replaygainPending` in
   `basetrackplayer.cpp`), so a track started immediately stays at the default until it is stopped.
3. **EBU R128 on stem tracks analyzes the raw 8-channel signal**; the v1 analyzer mixes down first
   (`analyzergain.cpp`) but `AnalyzerEbur128` does not.
4. There is no main-output limiter. That is by design in Mixxx and is not changed here.

## Steps

1. Confirm cause 1: on the owner's profile, read `[ReplayGain],DefaultBoost`,
   `[ReplayGain],ReplayGainBoost`, `[ReplayGain],ReplayGainEnabled` and `[Channel1],replaygain`
   from the developer tools before and after opening Preferences once. Record the values, and
   check where `DlgPrefReplayGain` is constructed in the QML startup path.
2. Move the initialization out of the dialog: `EnginePregain` (or a small helper called from
   `CoreServices`) sets the shared `[ReplayGain]` controls from `ReplayGainSettings` at startup.
   The dialog keeps writing them on apply.
3. Deferred apply: when analysis completes during playback, ramp to the new gain over a short,
   fixed time instead of waiting for a stop.
4. Stem tracks: make `AnalyzerEbur128` measure the stereo mixdown for 8-channel sources, matching
   what the deck actually outputs. The stored format does not change; affected tracks pick up the
   new value on re-analysis only.
5. Tests: startup with a config that has only `Initial*` keys yields the expected control values;
   pregain output for ratio 0 uses the default boost; an 8-channel source and its stereo mixdown
   measure within 0.5 LU.

## Verification

- New tests pass; existing `EnginePregain`/ReplayGain tests still pass.
- Manual on a fresh profile: load an unanalyzed loud track, play immediately; meters stay out of
  clip, and the gain settles once analysis completes.
- Manual on the owner's profile without opening Preferences.

## Done when

A fresh session applies the stored ReplayGain settings without the dialog being opened, late
analysis results reach a playing deck, and stem tracks are measured on their mixdown.
