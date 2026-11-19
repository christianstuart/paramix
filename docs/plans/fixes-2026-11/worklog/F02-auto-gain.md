# F02 worklog - auto-gain not applied

## Confirmed cause

`EnginePregain` creates the shared `[ReplayGain]` controls at their neutral 0 dB default. Only
`DlgPrefReplayGain::slotApply()` copies the stored `InitialReplayGainBoost` / `InitialDefaultBoost`
/ `ReplayGainEnabled` values into them. The dialog is built lazily, so on a normal session an
unanalysed track played at unity and the target boost never reached the engine. This is cause 1 in
the plan.

## What changed

- `EnginePregain` now takes the `UserSettingsPointer` and seeds the three shared controls from
  `ReplayGainSettings` when it creates them. The preferences dialog still writes them on apply.
- Late analysis results are applied while the deck plays: `slotSetReplayGain()` no longer defers to
  the next stop, and `m_replaygainPending` / `slotPlayToggled` are gone. `EnginePregain::process()`
  already ramps a changed replaygain over one second, so the change is smooth.
- `AnalyzerEbur128` measures 8-channel stem sources on their stereo mixdown, like `AnalyzerGain`
  does, so the stored loudness matches what the deck outputs.
- New test `EnginePregainTest.SeedsReplayGainControlsFromStoredSettings` fails without the first
  change.

## Not done

- No main-output limiter; the plan keeps that out of scope.
- Tracks already analysed as stems keep their old ReplayGain until re-analysed.

## Verification

- `cmake --build build` passes in `C:\Users\Bradl\paramix-fx-engine`.
- `EnginePregainTest.SeedsReplayGainControlsFromStoredSettings` passes with the fix and fails
  without it (verified by toggling the seeding block), and `AudioTaperPotTest` still passes.
- `PlayerManagerTest.UnEjectReplaceTrackTest` fails when run after the engine suites, and it does
  the same on unmodified `main`; it passes alone. Pre-existing order dependency, not caused by this
  change.
- Manual: fresh profile with only the `Initial*` keys set, play an unanalysed loud track, confirm
  the gain settles without opening Preferences.
