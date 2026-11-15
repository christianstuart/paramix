# Controller & MIDI Mapping Compatibility Remediation Plan

**Status:** Proposed
**Date:** 2026-11-15
**Scope:** MIDI, HID and bulk controller mappings: loading, control binding, user-mapping migration, and parity between the classic QWidget UI and the Paramix QML UI.
**Baseline:** `main` at `db0b53b` plus the working tree as of 2026-11-15
**Related documents:** `docs/agent-context.md`, `docs/paramix-rebrand.md`, `docs/paramix-stem-beatgrid-ui-plan.md`, `docs/paramix-ui-design-system.md`

---

## 0. How agents should use this document

1. Read sections 1-3 before touching code. They define what "compatible" means and what is already known to work.
2. Pick one workstream from section 3. Do not work across workstreams without coordinating file ownership (section 4).
3. Do not rename or remove an existing `ConfigKey` to make a test pass. Compatibility work is additive or UI-side.
4. Do not edit the vendored `res/controllers` tree wholesale. Fix only controls that are provably wrong, and record every change in `docs/controller-mapping-compatibility.md` (created by W0 and updated by W4).
5. Run the commands in section 5 before opening a PR. `tools/paramix_verify.sh` is the minimum local gate.
6. Update `docs/agent-context.md` section 3 (Current state) and section 9 (Work log) in the same PR, as required by that document.
7. If a task requires a product decision, stop and record the decision in section 6 instead of guessing.

---

## 1. Executive summary

The controller engine itself is **not** the problem. The fork did not replace the controller subsystem, the mapping schema, or the legacy JavaScript engine. All 144 bundled MIDI mappings and all 18 bundled HID mappings load successfully in the current build. The remaining incompatibilities are integration issues introduced by the Paramix-specific changes:

| # | Incompatibility | Severity | Root cause |
|---|---|---|---|
| 1 | Existing user mappings, learned bindings, enabled-controller state and per-mapping settings are not found after the move to the Paramix settings directory | **P0** | Settings path changed from `~/.mixxx` / `Mixxx` app data to `~/.paramix` / `Paramix` app data; the first-run migration wizard is not implemented |
| 2 | In the QML UI, library navigation controls used by 27 stock mappings do nothing | **P0 if QML is default; P1 otherwise** | `LibraryControl` intentionally skips connecting library-navigation controls when running in QML mode; QML only binds a subset of them |
| 3 | MIDI "learn by clicking a control" does not work on QML controls | **P1** | `ControllerLearningEventFilter` only recognizes legacy `WWidget` instances |
| 4 | Some `[Skin],show_*` controls are no-ops in QML | **P1** | QML does not subscribe to every skin control; some UI equivalents are missing |
| 5 | A small number of stock mappings reference controls that no longer exist | **P1** | Pre-existing Mixxx 2.7 rot (`[Flanger],*`, `[ChannelN],flanger`) plus one mapping typo |
| 6 | There is no CI guard against new control-name or QML-binding drift | **P0** | Mapping load tests only print warnings for missing controls; there is no UI-parity check |

### 1.1 Definition of "compatible"

A mapping is compatible when all of the following are true:

- It loads without a fatal XML or script error.
- Every **input** `ConfigKey` it references resolves to an existing, non-read-only control, or is an intentional script binding.
- Every **output** `ConfigKey` it references resolves, or the mapping is on the documented unsupported-control allowlist.
- Any UI-dispatch control it uses (`[Library]`, `[Playlist]`, `[Skin]`) has a working implementation in the UI mode in use.
- Its scripts run on the same legacy JS API (`engine.*`, `midi.*`, `controller.*`) with the same semantics as Mixxx 2.7.
- A user mapping stored under the old Mixxx settings directory is either imported or discoverable without manual file surgery.
- The behavior is covered by an automated test or a documented manual test on real hardware.

---

## 2. Current state and evidence

### 2.1 What is already compatible

Comparison against upstream Mixxx 2.7.0-alpha base `76b1672d4` (2026-11-10), plus the current upstream `main` (`dc4bf993`):

| Area | Result |
|---|---|
| `src/control/` | Byte-identical to upstream |
| `src/controllers/` | 5 files differ, all cosmetic: `Mixxx` → `Paramix` strings, formatting, icon filename |
| `src/controllers/midi/midicontroller.cpp` | Only warning-dialog strings and formatting; no logic change |
| `res/controllers/` | 3 of 328 files differ, all deliberate stem additions: Hercules Mk2 XML + JS, Traktor F1 JS |
| Control vocabulary | No upstream control key removed in the fork (the only removed string was the `Donate` menu item); 53 fork-only keys added, mostly stems and `head_mono_mixdown` |
| Mapping format | XML schema and legacy JS API unchanged |
| Version gate | Internally still `2.7.0-alpha`, so mapping version and schema gating behaves as upstream |
| MIDI/HID backends | `PORTMIDI` and `HID` unchanged; current build has both enabled |
| Mapping discovery | `CONTROLLER_MIDI_MAPPINGS` is still generated and installed to `share/paramix/controllers`; resource lookup checks both `share/paramix` and `share/mixxx` |

Empirical load results with the prebuilt test binary (no compilation performed):

- `144/144` MIDI mappings passed `LoadMapping`.
- `18/18` HID mappings passed `LoadMapping`.
- The only unresolved-control logs were test-harness artifacts (the fixture creates only two decks, one sampler and no microphones or skin controls) or pre-existing upstream rot.

### 2.2 Remaining incompatibilities

#### A. Settings and user-mapping migration (P0)

The settings directory changed:

- Linux/BSD: `~/.mixxx` → `~/.paramix` (`CMakeLists.txt`, `MIXXX_SETTINGS_PATH`)
- macOS/Windows: Mixxx app-data path → `Paramix` app-data path (`CmdlineArgs` constructor)
- Config file name remains `mixxx.cfg` (`src/config.h.in`)

`docs/agent-context.md` explicitly records that the first-run migration wizard is still outstanding. Consequences:

- User mappings in `<old settings>/controllers` are not discovered.
- Legacy pre-1.11 mappings in `<old settings>/midi` are not discovered.
- `[Controller],<device>` enabled flags, `[ControllerPreset],<device>` mapping selections, learned mappings in the mapping XML, and `[ControllerSettings_*]` settings remain in the old config.
- `LegacyControllerMapping::loadSettings()` builds its config key by replacing `pConfig->getSettingsPath()` with `%SETTING_PATH`. Mapping files loaded from an old path therefore get a different settings key than the one stored by the old installation.

#### B. QML library-navigation controls (P0 if QML is default)

`src/library/librarycontrol.cpp` creates all `[Library]` and `[Playlist]` navigation controls, but deliberately does **not** connect them when `CmdlineArgs::Instance().isQml()` is true. The comment pattern is repeated around `m_pMoveVertical`, `m_pScrollVertical`, `m_pMoveHorizontal`, `m_pMoveFocus`, `m_pEditItem`, `m_pGoToItem`, and the sidebar controls. The QML UI is expected to implement the behavior.

The current QML implementation (`res/qml/Library/Control.qml`, `TrackList.qml`, `SourceTree.qml`, `Browser.qml`) covers only:

- `[Library],GoToItem`
- `[Library],MoveVertical`, `MoveUp`, `MoveDown`
- `[Playlist],LoadSelectedIntoFirstStopped`
- `[Playlist],SelectNextTrack`, `SelectPrevTrack`, `SelectTrackKnob`
- `[ChannelN],LoadSelectedTrack` / `LoadSelectedTrackAndPlay` via `ControlLoadSelectedTrackHandler.qml`
- `[App],num_decks`, `num_samplers`, `num_preview_decks`

It does **not** bind these controls, which are used by 27 bundled mappings:

| Control | Bundled mapping files |
|---|---|
| `[Library],MoveFocus` | 5 |
| `[Library],MoveFocusForward` | 5 |
| `[Library],MoveFocusBackward` | 4 |
| `[Library],MoveHorizontal` | 2 |
| `[Library],MoveLeft` | 2 |
| `[Library],MoveRight` | 2 |
| `[Library],ScrollVertical` | 4 |
| `[Playlist],SelectNextPlaylist` | 12 |
| `[Playlist],SelectPrevPlaylist` | 11 |
| `[Playlist],ToggleSelectedSidebarItem` | 3 |

Representative affected mappings include Pioneer DDJ-400/FLX4, Denon MC3000/MC4000/MC7000, Reloop Beatpad/Jockey 3 ME, Numark Mixtrack Platinum FX, Hercules Inpulse 200/300/500, Vestax VCI-400 and Stanton DJC.4.

#### C. MIDI learning in QML (P1)

`src/controllers/controllerlearningeventfilter.cpp` filters only objects that cast to `WWidget`:

```cpp
WWidget* pWidget = qobject_cast<WWidget*>(pObject);
if (!pWidget || !m_bListening) {
    return false;
}
```

QML controls are `QQuickItem`-based and never enter that path. The control picker in the learning wizard still works, but the "click a control in the UI" workflow is unavailable. This is a workflow regression for QML mode, not a mapping-loading failure.

#### D. QML skin controls (P1)

`SkinControls` is created in `CoreServices` before controllers are set up, so all `[Skin],show_*` controls exist in both UI modes. However, many have no QML listener. Currently wired in QML: `show_4effectunits`, `show_library_coverart`, `show_maximized_library`, `show_microphones`, `show_preview_decks`, `show_vinylcontrol`, `show_ducking_controls`. Not wired: `show_effectrack`, `show_samplers`, `show_mixer`, `show_settings`, `show_coverart`, `show_spinnies`.

Stock mappings only use `[Skin],show_maximized_library` (12 files), which is wired. Third-party mappings may use the others.

#### E. Pre-existing stale mapping controls (P1)

These are inherited from upstream Mixxx 2.7, not caused by the fork:

- `[Flanger],lfoDepth/lfoDelay/lfoPeriod` and `[ChannelN],flanger`: the flanger effect no longer exists. At least `Wireless DJ App.midi.xml` still references them.
- `Reloop Digital Jockey 2 IE.midi.xml` references `[EqualizerRack1_[Channelundefined]_Effect1],button_parameter3`, a pre-existing interpolation bug.

Missing **outputs** only trigger the "control(s) not found" warning dialog; missing **inputs** are silently ignored at runtime after a log line. The rest of the mapping still works.

#### F. Missing CI guardrails (P0)

There is currently no automated check that:

- A mapping change still resolves every control.
- A QML change removes a binding that a bundled mapping depends on.
- A new stock mapping uses a UI-dispatch control that QML does not implement.

The existing `MappingTestFixture` is too small to distinguish "missing because the fixture is incomplete" from "missing because the control was removed".

### 2.3 Important baseline detail: default UI

The current code does not make QML the default. `src/main.cpp` loads QML only when `--new-ui` / `--qml` is passed, or when the configured skin is a QML skin and `--developer` is active. `SkinLoader::getDefaultSkinName()` still returns `LateNight` (legacy QWidget UI).

This means controller compatibility is currently best in the default configuration. The QML gaps in 2.2.B-D become release blockers only when the fork flips the default UI. Section 4 makes that a hard gate.

---

## 3. Workstreams

### W0 — Guardrails: audit tools and full-fidelity tests (P0)

**Goal:** Make regressions visible before fixing the remaining gaps.

#### Tasks

1. Add `tools/controller_mapping_audit.py`.
   - Parse `res/controllers/*.midi.xml`, `*.hid.xml`, and `*.bulk.xml`.
   - Extract input and output `ConfigKey`s. Skip inputs with `<script-binding/>`; output keys are always `ConfigKey`s.
   - Build the control oracle from a full runtime fixture (preferred) or from a generated dump, not from the stale `src/test/co_dumps/co_dump_inital.csv`.
   - Support known dynamic patterns from source: `hotcue_%1_*`, `beatloop_%1_*`, `beatlooproll_%1_*`, `beatjump_%1_*`, `group_%1_enable`, and effect-parameter families.
   - Modes: `--strict` (CI; nonzero exit on unknown controls), `--json`, `--allowlist <file>`.
   - The allowlist must require `key`, `reason`, `owner`, and `review-after` fields. Do not allow wildcard entries that hide whole groups.
2. Add `tools/controller_ui_parity.py`.
   - Extract UI-dispatch references from mappings: groups `[Library]`, `[Playlist]`, `[Skin]`.
   - Extract QML bindings from `res/qml/**/*.qml` for `Mixxx.ControlProxy`, `ControlButton`, `ControlKnob`, `ControlFader`, `SkinControlCreator`, and equivalent `group`/`key` pairs.
   - Fail when a mapping-used UI control has neither a QML binding nor an allowlist entry with a reason.
   - Fail on stale allowlist entries that are no longer used or are now wired.
   - Emit a Markdown matrix suitable for `docs/controller-mapping-compatibility.md`.
3. Extend `src/test/controller_mapping_validation_test.{h,cpp}`.
   - Create the full default object set in `SetUp()`: 4 decks, 16 samplers, 1 preview deck, 2 microphones, 2 auxiliaries, `SkinControls`, effects, recording manager and library.
   - Add `MappingControlResolutionTest`: enumerate all input/output controls in each mapping and assert that `ControlDoublePrivate::getControl(key, ControlFlag::AllowMissingOrInvalid)` returns non-null, except for the checked-in allowlist.
   - Keep the existing `LoadMapping` tests. Add a test that also checks non-MIDI (HID, bulk) mappings where their classes expose input/output maps.
   - Do not make missing controls fatal in production. This is a test-only strictness increase.
4. Add the two scripts to `tools/paramix_verify.sh` and `.github/workflows/ci.yml`. Run the UI parity check only in the QML-enabled build or as a static check; it does not need a compiled binary.
5. Add `docs/controller-mapping-compatibility.md` (human-readable output of the matrix, allowlist policy, and release checklist).

#### Files

- `tools/controller_mapping_audit.py` (new)
- `tools/controller_ui_parity.py` (new)
- `tools/controller_mapping_allowlist.txt` (new)
- `src/test/controller_mapping_validation_test.{h,cpp}`
- `CMakeLists.txt` (test source/setup changes only)
- `.github/workflows/ci.yml`, `tools/paramix_verify.sh`
- `docs/controller-mapping-compatibility.md` (new)

#### Acceptance

- The audit fails on a deliberately introduced unknown control and passes on the current tree with a reviewed allowlist.
- The parity check reports the 10 missing QML controls in section 2.2.B before W2 and zero unexplained gaps after W2.
- The full fixture no longer reports the false positives for `[Channel3/4]`, samplers, microphones or `[Skin]` controls.
- CI runs both checks on every PR.

---

### W1 — Settings and user-mapping migration (P0)

**Goal:** A user coming from Mixxx keeps their mappings, learned bindings, controller selections and per-mapping settings.

#### Design

Add a cross-platform `SettingsMigration` step that runs before `SettingsManager` opens the Paramix config. Coordinate with the existing macOS-only `Sandbox::migrateOldSettings()` (which moves old sandbox settings and runs before this step) so a directory is never moved or imported twice:

1. If `--settings-path` is set, do not auto-migrate. The user chose a path explicitly.
2. If the Paramix config already exists and contains the import marker, do nothing.
3. Discover legacy Mixxx settings directories:
   - Linux/BSD: `~/.mixxx`
   - macOS: `~/Library/Application Support/Mixxx` and the old sandbox container
     `~/Library/Containers/org.mixxx.mixxx/Data/Library/Application Support/Mixxx`
   - Windows: the legacy Mixxx `AppData` directory. Derive it from Qt standard locations with the legacy application name `Mixxx`, or hardcode the known locations and keep the path provider injectable for tests.
4. Show a one-time dialog (or a QML dialog in QML mode) with **Import**, **Skip**, and **Don't ask again**. Default to **Import**. If the process is non-interactive (`--safe-mode` or a future `--batch` mode), import without prompting only when `--import-mixxx-settings` is set.
5. Copy, never move:
   - `<old>/controllers/` recursively (XML, JS, QML modules, icon files)
   - `<old>/midi/` (legacy mappings)
   - `<old>/mixxx.cfg` when the Paramix config does not exist
   - optional controller-adjacent settings: `broadcast_profiles/` and `effects/` only if the user opts in
   - do **not** migrate the library database, analysis files, waveform cache or stem cache by default. Those are large and outside controller compatibility.
6. Conflict policy:
   - Existing Paramix files always win.
   - Copy only missing files; report skipped conflicts in the result dialog and log.
   - With `--import-mixxx-settings=replace`, back up the target to `<settings>/backup-mixxx-<timestamp>/` before overwriting.
   - If the target `mixxx.cfg` already exists, merge only controller-related groups from the old file:
     `[Controller]`, `[ControllerPreset]`, and `[ControllerSettings_*]`. Never overwrite an existing Paramix value.
7. Record completion in the Paramix config, for example `[Config],MixxxSettingsImport=1` plus source path and timestamp, and/or a marker file `.mixxx-settings-imported`.
8. Add a read-only fallback in `ControllerManager::getMappingPaths()` so that, if the user skips migration, `<old settings>/controllers` and `<old settings>/midi` are still searched after the new user directory. Never write to the legacy directory.
9. Add CLI flags:
   - `--import-mixxx-settings` — force a one-time import and skip the prompt
   - `--no-import-mixxx-settings` — suppress the prompt and do not import
   - `--import-mixxx-settings=replace` — allow overwrite with backup
10. Surface the result in Preferences → Controllers and in the QML controller preferences: "Imported N mappings from Mixxx" with a link to the mappings folder.

#### Files

- `src/preferences/settingsmigration.{h,cpp}` (new)
- `src/coreservices.cpp` (`initializeSettings()`, before `SettingsManager`)
- `src/preferences/settingsmanager.{h,cpp}` if the migration needs to run inside the settings manager instead
- `src/preferences/upgrade.{h,cpp}` (reuse version-upgrade conventions if appropriate)
- `src/util/cmdlineargs.{h,cpp}`
- `src/controllers/defs_controllers.h`, `src/controllers/controllermanager.cpp`
- `src/qml/qmlpreferencesproxy.{h,cpp}` (show import status and open-folder action)
- `src/test/settingsmigration_test.cpp` (new)
- `CMakeLists.txt`

#### Tests

- Unit tests with a temporary directory and an injected legacy-path provider:
  - old only, new only, both, partial target, no old directory
  - import twice is idempotent
  - existing target files are not overwritten
  - replace mode creates a backup
  - unreadable files/directories are reported and skipped without aborting startup
  - a config merge imports `[Controller]`, `[ControllerPreset]` and `[ControllerSettings_*]` keys but leaves other Paramix keys untouched
  - legacy mapping fallback finds a mapping when migration is skipped
- Manual test: install a stock Mixxx 2.7 profile with one user mapping, one learned override, and an enabled controller; launch Paramix; verify the mapping is selected, the learned override is active, and the controller opens without reconfiguration.

#### Acceptance

- A Mixxx user mapping in the old directory is either imported or discoverable without manual copying.
- Enabled-controller and mapping-selection state survive the upgrade.
- No file in the old Mixxx directory is deleted or modified.
- Running the migration twice is safe.
- The migration is covered by tests and documented for users.

---

### W2 — QML UI control parity for mappings (P0 if QML becomes default, P1 otherwise)

**Goal:** Every UI-dispatch control used by a bundled or common third-party mapping works in QML the way it works in the classic UI.

#### Design decision

Fix this in QML, not by reconnecting `LibraryControl`. `LibraryControl`'s slots call `m_pLibraryWidget`, `m_pSidebarWidget` and `m_pSearchbox`; those are legacy widgets and are null in QML mode. Reconnecting them would produce no-ops or null dereferences. The QML side must own the behavior.

#### Tasks

1. Introduce one library controller object at the `Library.qml` level (or extend `Library/Control.qml` and instantiate it once in `Library.qml`).
   - Own the `[Library],focused_widget` proxy.
   - Route focus between Searchbar, Sidebar and LibraryView.
   - Pass focus/signal handlers to `SourceTree.qml`, `TrackList.qml` and `Browser.qml`.
   - Remove the current duplicate assumption that `TrackList.qml` owns the only `FocusedWidgetControl`.
   - Track focus changes from the actual QML focus items and write `[Library],focused_widget`, so scripts and the QML UI stay in sync.
2. Implement the missing navigation controls:

   | Control | QML behavior |
   |---|---|
   | `[Library],MoveFocus` | Cycle Searchbar → Sidebar → LibraryView, honoring the sign of the encoder value. Match the legacy `slotMoveFocus` direction and wrap behavior. |
   | `[Library],MoveFocusForward` / `MoveFocusBackward` | Same as `MoveFocus` with `+1` / `-1`, only when value > 0. |
   | `[Library],MoveVertical` / `MoveUp` / `MoveDown` | Already implemented in `TrackList.qml`; extend so the focused pane decides whether to move the track selection or the sidebar selection. |
   | `[Library],ScrollVertical` / `ScrollUp` / `ScrollDown` | Page up/down in the focused list. Add `scrollVertical(delta)` to `TrackList.qml` and `SourceTree.qml`; clamp at the ends. |
   | `[Library],MoveLeft` / `MoveRight` / `MoveHorizontal` | Route to the focused pane: sidebar collapses/expands the selected node; track list moves horizontally or scrolls one column; searchbar moves the text cursor. |
   | `[Playlist],SelectNextTrack` / `SelectPrevTrack` / `SelectTrackKnob` | Already implemented; keep behavior and add regression tests. |
   | `[Playlist],SelectNextPlaylist` / `SelectPrevPlaylist` | Add `selectNext()` / `selectPrevious()` to `SourceTree.qml`, using the visible tree rows and `QmlSidebarModelProxy::activate()`. Skip disabled/placeholder rows, handle lazy children, and keep the QML selection model in sync. |
   | `[Playlist],ToggleSelectedSidebarItem` | Add `toggleSelected()` to `SourceTree.qml`; call the existing `toggleNode(row)` on the current row. |
   | `[Library],MoveTrackUp` / `MoveTrackDown` / `MoveTrack` | Not used by stock bundled mappings but used by keyboard/third-party workflows. Either implement playlist reordering or add a reasoned allowlist entry. |
   | `[Library],EditItem`, `GoToItem` | `GoToItem` exists; `EditItem` should focus the search field or open the tag editor for the selected row. Add a test or allowlist entry. |
   | `[Library],sort_column`, `sort_order`, `sort_column_toggle`, `sort_focused_column`, `font_size_*`, `set_bpmlock` | Audit; implement if cheap, otherwise allowlist with a reason. |

3. Wire the remaining `[Skin]` controls:

   | Control | QML action |
   |---|---|
   | `show_effectrack` | `MainWindow.showEffects` (already exists) |
   | `show_samplers` | `MainWindow.showSamplers` (already exists) |
   | `show_maximized_library` | Already wired |
   | `show_microphones` | Already wired |
   | `show_preview_decks` | Already wired |
   | `show_vinylcontrol` | Already wired |
   | `show_library_coverart` | Already wired |
   | `show_4effectunits` | Already wired |
   | `show_ducking_controls` | Already wired |
   | `show_coverart` | `Mixxx.Config.paramixShowCoverArt` (already used by `Deck/InfoBar.qml`) |
   | `show_spinnies` | Add a Paramix config property (for example `paramixShowSpinnies`) and gate `Deck/Spinny.qml` |
   | `show_mixer` | Add a `showMixer` property and hide the mixer panes; keep the crossfader and master controls reachable |
   | `show_settings` | Open the existing `ParamixCustomize` popup (`res/qml/MainWindow.qml`) |

4. Keep the legacy UI unchanged. This work must not alter `LibraryControl` connection logic or legacy widget behavior.
5. Add a compatibility mapping table to `docs/controller-mapping-compatibility.md`: control → QML implementation → test.
6. Add a manual smoke matrix for at least one controller per missing control family: Pioneer DDJ-FLX4/400 (library browse), Denon MC7000 (focus/playlist), Reloop Beatpad (playlist), Hercules Inpulse 300 (scroll), Vestax VCI-400 (horizontal navigation).

#### Files

- `res/qml/Library.qml`
- `res/qml/Library/Control.qml`
- `res/qml/Library/TrackList.qml`
- `res/qml/Library/SourceTree.qml`
- `res/qml/Library/Browser.qml`
- `res/qml/MainWindow.qml`
- `res/qml/ApplicationMenuActions.qml`
- `res/qml/Deck/InfoBar.qml`, `res/qml/Deck/Spinny.qml` (if `show_coverart` / `show_spinnies` require it)
- `res/qml/ParamixCustomize.qml`
- `src/qml/qmlsidebarmodelproxy.{h,cpp}` only if a C++ invokable is preferable to QML-side traversal
- `src/qml/qmlconfigproxy.{h,cpp}` for a new `paramixShowSpinnies` property if needed
- `src/test/qml_controller_bindings_test.cpp` (new) or the nearest existing QML test target

#### Tests

- `tools/controller_ui_parity.py` reports zero unexplained gaps.
- An offscreen QML test instantiates the library pane and verifies:
  - each mapping-used control changes the expected focus, selection, expanded row, or visibility
  - `focused_widget` is written in both directions (QML focus → control, control → QML focus)
  - sidebar next/previous does not select the search row or a placeholder
  - scroll/page functions clamp correctly
- Manual hardware smoke test per the matrix above.

#### Acceptance

- All 10 controls in section 2.2.B work in QML.
- The 27 affected stock mappings can browse and load tracks in QML mode.
- All `[Skin]` controls either act on the QML UI or are explicitly allowlisted with a documented reason.
- Legacy UI behavior is unchanged.

---

### W3 — MIDI learning on QML controls (P1)

**Goal:** The learning wizard can learn a control by clicking it in the QML UI, or the UI clearly provides an equivalent first-class flow.

#### Recommended design

1. Add a `QmlControlLearningRegistry` (owned by `ControllerManager` or a small singleton) that maps a QQuickItem to its `QmlControlProxy`/`ConfigKey`.
   - `QmlControlProxy` registers itself after `componentComplete()` and unregisters on destruction.
   - Expose the `ConfigKey` from `QmlControlProxy` for the registry.
   - Ignore proxies that are not interactive or whose key is invalid.
2. Extend `ControllerLearningEventFilter` to install an event filter on the QML window when listening:
   - On mouse press, hit-test the `QQuickItem` under the cursor.
   - Walk the item and its ancestors (and the relevant QObject children) until a registered proxy is found.
   - Emit `controlClicked(ConfigKey)` exactly as the `WWidget` path does.
   - If multiple proxies match a composite control, prefer the nearest ancestor registered as interactive; log ambiguity in developer mode.
3. Keep the control picker as the always-available fallback.
4. If implementation A proves unreliable for wrapped controls, use implementation B: an explicit `LearningTarget` attached property on `Skin.Button`, `Skin.Knob`, `Skin.Fader` and the composite controls, plus a `Mixxx.ControllerLearning.active` singleton that emits the key on press. Choose one design before coding; do not mix both.

#### Files

- `src/controllers/controllerlearningeventfilter.{h,cpp}`
- `src/controllers/controllermanager.{h,cpp}` (registry ownership/lifetime)
- `src/controllers/dlgcontrollerlearning.{h,cpp}`
- `src/qml/qmlcontrolproxy.{h,cpp}`
- `res/qml/ControlButton.qml`, `ControlKnob.qml`, `ControlFader.qml` (if design B)
- `src/test/controller_learning_qml_test.cpp` (new)

#### Tests

- Offscreen QML test: create a window with a `ControlKnob` bound to `[Channel1],filterLow`, start listening, synthesize a click, assert the learned key.
- Test nested controls (knob inside a deck pane) resolve the inner control, not the pane.
- Test invalid/read-only proxies are skipped.
- Manual: open the learning wizard in QML mode and learn play, cue, a filter knob, a library control and a mixer fader.

#### Acceptance

- Learn-by-click works for representative QML controls.
- The learned key matches the control the user clicked.
- The legacy `WWidget` path still works.
- If the feature is deferred, the learning wizard shows an explicit hint that QML click-learning is unavailable and points to the control picker. A deferred feature must be recorded as an accepted gap, not silently shipped.

---

### W4 — Bundled mapping cleanup and stem coverage (P1/P2)

**Goal:** Remove known stale references, document intentional divergence, and make stem mappings reusable.

#### Tasks

1. Fix provably broken stock mappings only:
   - `res/controllers/Wireless DJ App.midi.xml`: replace or remove `[Flanger],lfo*` and `[ChannelN],flanger` references. Since the flanger effect no longer exists, map the controls to a current effect unit only if the original intent is clear; otherwise remove the dead outputs and note it.
   - `res/controllers/Reloop Digital Jockey 2 IE.midi.xml`: fix the `[EqualizerRack1_[Channelundefined]_Effect1]` interpolation bug.
   - Audit every `flanger`, `pitch_set_default`, unsuffixed `beatloop_4`/`beatloop_8`, `preplay`, `slowback`, `slowfwd` and similar legacy key. For each, decide: implement a correct alias in C++ (only when semantics match), update the mapping, or add the key to the unsupported-control allowlist with a reason.
2. Do not fork all vendored mappings. Keep a short patch list in `docs/controller-mapping-compatibility.md` with upstream link, reason, and review date.
3. Add `res/controllers/Paramix-Stems.js` (or an equivalent shared helper) for:
   - deriving `[ChannelN_StemX]` and `[QuickEffectRack1_[ChannelN_StemX]]` groups
   - per-stem volume/mute/QuickFX linking
   - feature detection so a mapping degrades gracefully if stem controls are absent
   - consistent LED colors
4. Refactor the Hercules Mk2 and Traktor F1 stem additions to use the helper. Review the current code for undefined-control guards and mode/LED edge cases.
5. Expand stem mapping coverage only after W0-W2 are green. Prioritize controllers with spare faders/pads and no stem mode conflict. Keep each controller in its own PR.
6. Add a mapping-author checklist (Appendix C) to the docs and reference it from the PR template.

#### Files

- `res/controllers/Wireless DJ App.midi.xml`
- `res/controllers/Reloop Digital Jockey 2 IE.midi.xml`
- `res/controllers/Paramix-Stems.js` (new)
- `res/controllers/Hercules DJ Console Mk2.midi.xml`, `Hercules-DJ-Console-Mk2-scripts.js`
- `res/controllers/Traktor-Kontrol-F1-scripts.js`
- `docs/controller-mapping-compatibility.md`, `docs/agent-context.md`
- Any additional controller mappings selected for stem support

#### Tests

- W0 audit reports no unknown non-allowlisted controls.
- Existing `LoadMapping` tests still pass for all 144 MIDI and 18 HID mappings.
- Manual test each modified controller in both stem and non-stem modes.
- Verify a STEM-disabled build still loads the mappings and the stem-specific inputs are inert rather than fatal.

#### Acceptance

- No bundled mapping references an unknown control except documented, reviewed allowlist entries.
- The flanger and `Channelundefined` bugs are fixed or explicitly documented as upstream-fixed-later.
- Stem helper is used by the fork-modified mappings and is covered by a load test.
- No non-stem behavior regresses.

---

### W5 — Release gate, default UI decision and documentation (P1)

**Goal:** Do not call the fork "controller compatible" until the checks are green, and do not switch the default UI until QML parity is complete.

#### Tasks

1. Make the QML-default decision explicit in `docs/agent-context.md`:
   - **Recommended:** keep the classic UI as the default for 0.1 and until W0 + W2 are green.
   - Flipping the default requires: parity check zero gaps, QML library smoke tests green, and one full hardware pass on the supported-controller list.
2. Add a release checklist item:
   - `tools/controller_mapping_audit.py --strict`
   - `tools/controller_ui_parity.py`
   - MIDI/HID mapping load tests
   - settings migration test
   - QML binding test
   - hardware smoke matrix
3. Add user documentation:
   - "Migrating from Mixxx" in README / manual: what is imported, where mappings live (`~/.paramix/controllers` on Linux), how to import manually, how to keep the old settings with `--settings-path`.
   - "Controller mappings in the Paramix UI": which controls work in QML, and how to use the control picker if click-learning is unavailable.
4. Update `docs/agent-context.md`:
   - Current state: controller/mapping status, QML parity status, migration status.
   - Work log: link this plan and each PR.
   - Locked decisions: record the default-UI decision and the mapping allowlist policy.
5. Update `README.md` only after the claims are true. Do not claim full controller parity in the QML skin while the gap matrix is non-empty.

#### Acceptance

- Release checklist is in the repo and referenced by the PR template.
- Docs match the actual behavior in both UI modes.
- The default UI decision and its preconditions are recorded.

---

## 4. Sequencing, dependencies and ownership

### 4.1 Milestones

| Milestone | Workstreams | Outcome | Suggested duration |
|---|---|---|---|
| M1 — No silent regressions | W0 | Audit + full fixture + CI guardrails; baseline gap matrix published | 2-3 days |
| M2 — Upgrade-safe | W1 | Old Mixxx mappings/settings imported or discoverable; fallback path; tests | 3-5 days |
| M3 — QML mapping parity | W2 | All mapping-used library/playlist/skin controls wired in QML; parity check green | 1-2 weeks |
| M4 — Learning and cleanup | W3, W4 | QML click-learning (or explicit accepted gap) and stale-mapping cleanup | 1 week |
| M5 — Release gate | W5 | Docs, default-UI decision, hardware smoke pass | 2-3 days |

### 4.2 Dependencies

- W0 must land before W2 and W4 so regressions are measurable.
- W1 is independent of W0/W2 and can run in parallel.
- W2 depends on the QML architecture decision in W2's design section; it does not depend on W1.
- W3 depends on W2's final library/focus architecture but can start on the learning bridge in parallel.
- W4 depends on W0's audit and should be last for mapping edits.
- W5 depends on all other workstreams.

### 4.3 Suggested agent split

| Agent | Ownership | Avoid touching |
|---|---|---|
| A | W0 tools, CI, test fixture | `res/qml`, controller mappings |
| B | W1 settings migration, CLI, controller path fallback | QML files, mapping XML |
| C | W2 library/QML control bindings | settings migration, mapping JSON |
| D | W2 skin toggles + W3 learning bridge | library navigation internals |
| E | W4 mapping cleanup + stem helper | C++ engine/control code, QML |
| Human | Decisions, hardware QA, final review | — |

**Shared-file conflicts to coordinate:** `CMakeLists.txt`, `docs/agent-context.md`, `res/qml/MainWindow.qml`, `res/qml/Library/Control.qml`, and `src/test/controller_mapping_validation_test.cpp`. Assign one owner per PR and rebase before editing these.

---

## 5. Test plan

### 5.1 Automated

| Test | Type | What it proves |
|---|---|---|
| `tools/controller_mapping_audit.py --strict` | Static | Every mapping control exists or is a reviewed exception |
| `tools/controller_ui_parity.py` | Static | Every mapping-used UI control has a QML binding or a reviewed exception |
| `MidiMappings/MappingTestFixture.LoadMapping/*` | Runtime | Every MIDI mapping loads and initializes scripts |
| `HidMappings/MappingTestFixture.LoadMapping/*` | Runtime | Every HID mapping loads and initializes scripts |
| `MappingControlResolutionTest` | Runtime | No mapping references a missing control in the full default object set |
| `SettingsMigrationTest` | Runtime | Import/merge/idempotency/conflict behavior |
| `ControllerLearningQmlTest` | Runtime QML | Click-learning resolves the correct `ConfigKey` |
| `QmlControllerBindingsTest` | Runtime QML | Library/playlist/skin controls drive the QML UI |

### 5.2 Commands

Use the existing build directory; do not introduce a second build tree just for these tests.

```bash
# Mapping load tests (prebuilt binary)
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='MidiMappings/MappingTestFixture.LoadMapping/*'
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='HidMappings/MappingTestFixture.LoadMapping/*'

# New tests
QT_QPA_PLATFORM=offscreen ./build-main/mixxx-test \
  --gtest_filter='*MappingControlResolution*:*SettingsMigration*:*ControllerLearningQml*'

# Static checks
python3 tools/controller_mapping_audit.py --strict
python3 tools/controller_ui_parity.py
tools/paramix_verify.sh
```

### 5.3 Manual hardware matrix

At minimum, verify these controllers in both legacy and QML modes after W2/W4:

- Pioneer DDJ-FLX4 or DDJ-400: browse encoder, load, playlist navigation, focus.
- Denon MC7000: library focus, playlist next/previous, scroll.
- Reloop Beatpad: playlist selection and load.
- Hercules Inpulse 300: vertical browse and page scroll.
- Vestax VCI-400: horizontal navigation.
- Hercules Mk2 and Traktor F1: stem mode and non-stem mode.
- Any controller using `[Skin],show_maximized_library`: verify the library maximize button still works in QML.

Record the firmware/driver version, OS, and UI mode in the PR.

---

## 6. Risks, non-goals and open decisions

### 6.1 Risks

| Risk | Mitigation |
|---|---|
| Copying a whole old `mixxx.cfg` imports stale UI or sound settings | Fresh profile: copy whole config. Existing profile: merge only controller groups. Always back up before replace. |
| Editing vendored mappings drifts from upstream | Keep a short patch list; prefer allowlist + upstream report; never reformat or wholesale-edit the tree. |
| QML focus model differs from legacy `WWidget` focus | Model the legacy `FocusWidget` order; add offscreen focus tests; centralize focus ownership in one QML object. |
| QML learning hit-testing is fragile | Prefer registry + ancestor walk; fall back to explicit `LearningTarget`; keep control picker as guaranteed path. |
| Dynamic controls make the static audit incomplete | Use the full runtime fixture as the oracle; expand known patterns in one place; review allowlist each release. |
| Migrating the library DB is out of scope but users expect it | State explicitly that this plan migrates controller assets and settings only. Track library migration as a separate decision. |
| Default-UI flip hides a mapping regression | Do not flip until W2 parity and hardware smoke tests pass. |

### 6.2 Non-goals

- Rewriting the controller engine or JavaScript API.
- Re-adding the removed flanger effect.
- Automatic migration of the entire music library database, analysis files or stem cache.
- Stem mapping coverage for every controller in one release.
- Changing control names to fit the Paramix UI.

### 6.3 Open decisions

| # | Decision | Recommended answer | Owner |
|---|---|---|---|
| 1 | Is QML the default UI for 0.1? | No. Keep classic default until W2 is green. | Product owner |
| 2 | Automatic import or prompt? | Prompt once with Import/Skip/Don't ask again; default Import. | Product owner |
| 3 | Import whole `mixxx.cfg` on a fresh profile? | Yes, on a fresh Paramix profile; merge only controller groups otherwise. | W1 owner |
| 4 | Fix vendored mappings in place or allowlist them? | Fix only provably broken files; allowlist the rest with a reason and an upstream issue. | W4 owner |
| 5 | Is QML click-learning required for 0.1? | Implement if W2 is stable; otherwise ship control-picker-only with a visible hint and record an accepted gap. | Product owner |
| 6 | Which controllers get stem modes first? | Controllers with spare faders/pads and existing community demand; one PR each. | W4 owner |

---

## 7. Definition of done

The compatibility work is complete when:

1. `tools/controller_mapping_audit.py --strict` passes with a reviewed, non-stale allowlist.
2. `tools/controller_ui_parity.py` reports zero unexplained gaps.
3. `144/144` MIDI and `18/18` HID mappings load in the full fixture.
4. `MappingControlResolutionTest` passes without false positives for decks, samplers, microphones, auxiliaries or `[Skin]` controls.
5. A Mixxx user's controller mappings and enabled-controller state survive the upgrade, with automated migration tests.
6. `LibraryControl`'s QML-mode disconnects are fully compensated by QML bindings for every mapping-used control.
7. The `[Skin]` control matrix has no silent no-ops.
8. QML click-learning works, or the fallback is explicit, documented and accepted.
9. No stock mapping references `[Flanger]` or another removed control without an allowlist entry and documented reason.
10. The default-UI decision and release checklist are recorded in `docs/agent-context.md` and the PR template.

---

## Appendix A — Current QML control gap matrix

| Control | Classic UI | QML today | Required action |
|---|---|---|---|
| `[Library],GoToItem` | Works | Works | Regression test |
| `[Library],MoveVertical/Up/Down` | Works | Works | Extend focus routing |
| `[Library],ScrollVertical/Up/Down` | Works | Missing | W2 |
| `[Library],MoveHorizontal/Left/Right` | Works | Missing | W2 |
| `[Library],MoveFocus/Forward/Backward` | Works | Missing | W2 |
| `[Library],EditItem` | Works | Missing | W2 or allowlist |
| `[Library],MoveTrack*` | Works | Missing | W2 or allowlist |
| `[Playlist],SelectNext/PrevTrack` | Works | Works | Regression test |
| `[Playlist],SelectTrackKnob` | Works | Works | Regression test |
| `[Playlist],SelectNext/PrevPlaylist` | Works | Missing | W2 |
| `[Playlist],ToggleSelectedSidebarItem` | Works | Missing | W2 |
| `[Playlist],LoadSelectedIntoFirstStopped` | Works | Works | Regression test |
| `[ChannelN],LoadSelectedTrack` | Works | Works | Regression test |
| `[Skin],show_maximized_library` | Works | Works | Regression test |
| `[Skin],show_effectrack` | Works | Missing | W2 |
| `[Skin],show_samplers` | Works | Missing | W2 |
| `[Skin],show_mixer` | Works | Missing | W2 or allowlist |
| `[Skin],show_settings` | Works | Missing | W2 |
| `[Skin],show_coverart` | Works | Missing | W2 |
| `[Skin],show_spinnies` | Works | Missing | W2 |

## Appendix B — Known stale or unsupported mapping references

| Key | Example file | Status/action |
|---|---|---|
| `[Flanger],lfoDepth/lfoDelay/lfoPeriod` | `Wireless DJ App.midi.xml` | Removed effect; W4 fixes or removes, then allowlists |
| `[ChannelN],flanger` | `Wireless DJ App.midi.xml` and scripts | Removed effect; W4 audits |
| `[EqualizerRack1_[Channelundefined]_Effect1],button_parameter3` | `Reloop Digital Jockey 2 IE.midi.xml` | Mapping bug; W4 fixes |
| `[SamplerN],duration` | `Pioneer DDJ-SB.midi.xml` | Verify in full fixture; likely test-harness false positive |
| `[Channel3/4],LoadSelectedTrack`, `[Channel3/4],vu_meter` | DJ Tech mappings | Full fixture should resolve |
| `[Microphone],enabled/talkover` | Hercules RMX 2 | Full fixture should resolve |
| `[Master],maximize_library` | Reloop Digital Jockey 2 IE | Alias exists; full skin fixture should resolve |

## Appendix C — Mapping-author checklist

When adding or changing a mapping:

- [ ] Use only controls that exist in `controlpickermenu.cpp` or the current control picker.
- [ ] Do not use `[Flanger]` or `[ChannelN],flanger`; use the current effect rack or QuickEffect controls.
- [ ] For beat loops, prefer `beatloop_X_toggle` / `beatloop_X_enabled` and `beatlooproll_X_activate`.
- [ ] For hotcues, prefer `hotcue_X_activate` / `hotcue_X_set` / `hotcue_X_clear` / `hotcue_X_enabled`.
- [ ] For effect-unit routing, use `group_[ChannelN]_enable` on the effect unit.
- [ ] If the mapping uses a `[Library]`, `[Playlist]` or `[Skin]` control, add it to the QML parity matrix or the reasoned allowlist.
- [ ] Run `tools/controller_mapping_audit.py --strict` and `tools/controller_ui_parity.py`.
- [ ] Run the mapping load test for both MIDI and HID variants.
- [ ] Test in the classic UI and in QML mode.
- [ ] Update `docs/controller-mapping-compatibility.md` and `docs/agent-context.md`.

## Appendix D — Useful paths and references

| Purpose | Path |
|---|---|
| Bundled mappings | `res/controllers/` |
| Mapping loader | `src/controllers/legacycontrollermappingfilehandler.cpp` |
| MIDI mapping class | `src/controllers/midi/legacymidicontrollermapping.{h,cpp}` |
| Mapping file paths | `src/controllers/defs_controllers.h` |
| Controller manager | `src/controllers/controllermanager.cpp` |
| Library control implementation | `src/library/librarycontrol.cpp` |
| QML library control | `res/qml/Library/Control.qml` |
| QML track list | `res/qml/Library/TrackList.qml` |
| QML source tree | `res/qml/Library/SourceTree.qml` |
| QML main window | `res/qml/MainWindow.qml` |
| QML control proxy | `src/qml/qmlcontrolproxy.{h,cpp}` |
| Learning event filter | `src/controllers/controllerlearningeventfilter.{h,cpp}` |
| Settings manager | `src/preferences/settingsmanager.{h,cpp}` |
| Settings initialization | `src/coreservices.cpp` |
| CLI settings path | `src/util/cmdlineargs.{h,cpp}` |
| Mapping load tests | `src/test/controller_mapping_validation_test.{h,cpp}` |
| CI | `.github/workflows/ci.yml` |
| Local verification | `tools/paramix_verify.sh` |
