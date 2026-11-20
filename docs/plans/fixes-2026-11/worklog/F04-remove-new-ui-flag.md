# F04 worklog - remove the `--new-ui` flag

## What changed

- `CmdlineArgs`: dropped `isQml()`, `isAwareOfRisk()`, `m_qml`, `m_awareOfRisk` and the
  `allow-dangerous-data-corruption-risk` option object. Hidden, ignored `--new-ui`, `--qml` and
  `--allow-dangerous-data-corruption-risk` options keep old shortcuts and the Android manifest
  launching; each logs a warning. The last one is part of the same one-release shim because a
  shortcut created for the old gate passes both `--new-ui` and the risk flag, and
  `QCommandLineParser` rejects unknown options by exiting.
- `QmlApplication`: deleted the Mixxx 3.0 data-corruption gate and its dialog. The constructor
  still sets `m_loadSucceeded = false` and returns instead of calling `exit()` when the QML file
  cannot be loaded; the sound-device failure path is unchanged.
- `restartApplication()` no longer takes a UI flag and no longer appends or filters arguments; it
  relaunches with the same arguments, which the parser now accepts and ignores. The Help menu
  action and the QML-callable `QmlApplicationProxy::restartApplication()` twin are gone; nothing in
  `res/qml` called them.
- Every `isQml()` call site resolved to the flag-absent branch, i.e. exactly what the app did
  before: library navigation, scanner progress and legacy skin parsing keep their bodies, while the
  QML-only branches in `DlgPrefInterface`/`DlgPrefMixer` are removed and `DlgPrefInterface` keeps
  hiding the Paramix group boxes.
- `packaging/android/AndroidManifest.xml` no longer passes `--new-ui`.
- Smoke-test parameter `NewUi` renamed to `Default`/`useDefaultSkin`; docs and `AGENTS.md` updated.

## Grep audit

`git grep -n "new-ui\|isQml()\|isAwareOfRisk\|allow-dangerous"` in the worktree:

| Location | Verdict |
|---|---|
| `src/util/cmdlineargs.cpp:294,301,467,475` | **Allowed shim.** Registers the hidden ignored options and warns about them. |
| `src/main.cpp:83` | **Allowed prose.** Comment documents that the obsolete flag is accepted and ignored. |
| `AGENTS.md:36,38` | **Allowed prose.** Describes the shim and current launch behavior. |
| `docs/controller-mapping-compatibility-plan.md:93,161` | **Allowed prose.** Says the flag and its branches are gone; flags are ignored. |
| `docs/paramix-ui-design-system.md:125` | **Allowed prose.** Says `--new-ui`/`--qml` are obsolete and ignored. |
| `docs/plans/fixes-2026-11/*` (33 lines) | **Allowed prose.** The fix plan and this worklog describe the removal. |
| `res/translations/*.ts` (50 files) and `*.qm` (7 files) | **Generated catalogs, not code.** They still carry the old upstream `--new-ui` help string and the deleted gate message. They are produced by Qt `lupdate`/`lrelease`; hand-editing is wrong, and `.qm` are binary. The next translation update will prune the orphaned sources. No build reads them by keyword. |

No `isQml()` call sites remain. `grep "isQml"` still matches
`WaveformWidgetFactory::isQmlMode()`, which is a different, still-used API and not related to
`CmdlineArgs`.

## Audit verdict per checklist item

1. **Grep** - clean above; only the cmdlineargs shim, prose, plan docs and generated translation
   catalogs match.
2. **CmdlineArgs** - `isQml()`, `isAwareOfRisk()`, `m_qml`, `m_awareOfRisk` and their option
   objects are gone. Hidden ignored `--new-ui`, `--qml` and
   `--allow-dangerous-data-corruption-risk` are registered before parsing, so a stale shortcut
   reaches the warning instead of the unknown-option exit path.
3. **`isQml()` branches** - all resolve to the flag-absent behavior: library and scanner
   connections unconditional, `DlgPrefInterface`/`DlgPrefMixer` hide the Paramix widgets,
   `LegacySkinParser` keeps building widgets, `upgrade.cpp` writes the normal version. No behavior
   was flipped.
4. **`qmlapplication.cpp`** - gate and dialog removed; the `m_loadSucceeded = false; return;`
   structure remains for QML load failures, and `runMixxx()` still turns that into
   `kFatalErrorOnStartupExitCode` rather than calling `exit()` in the constructor.
5. **`restartapplication.{h,cpp}`** - bool parameter removed, no flags appended, no literal flag
   filtering. `grep "restartApplication("` finds no `true`/`false` call sites; the only caller was
   the QML proxy method, removed in this commit.
6. **Help menu and QML twin** - "Use Paramix QML Interface" action removed; the
   `QmlApplicationProxy::restartApplication()` invokable is removed too. `res/qml` has no
   `restartApplication` call (only unrelated timer `restart()` calls).
7. **Android manifest** - `--new-ui` removed; arguments are now
   `--log-level debug --developer`.
8. **Smoke test** - parameter renamed to `useDefaultSkin`; cases are `LateNightQML` and `Default`.
   Both still construct `QmlApplication`; the `Default` case uses the default QML entry point.
9. **Docs** - `AGENTS.md`, `docs/paramix-ui-design-system.md`,
   `docs/controller-mapping-compatibility-plan.md` and `docs/performance.md` no longer describe
   the flag as functional. `AGENTS.md` now states the QML interface always loads and that old
   shortcuts (including `--qml` and the risk flag) start with a warning.
10. **Formatting** - `clang-format --dry-run --Werror` is clean on every `.cpp`/`.h` changed by
    this branch; `python tools/qml_qt_api_guard.py` reports 0 violations.

## Behavior decisions left for a human

- `LibraryControl` now always connects the `[Library]`/`[Playlist]` navigation controls. The QML UI
  also implements handling for some of them, so both paths are active. This is what the app did
  without the flag and is retained deliberately; whether the duplicate widget-side connections
  should be dropped in a QML-only build needs manual controller testing.
- The `[Paramix]` colour/font/motion/visibility group on the native **Interface** page and the
  fourth-EQ-band checkbox on **Mixer** stay hidden, matching the flag-absent behavior. The QML
  scene reads the same keys through `Mixxx.Config`. Re-exposing those controls in the Qt dialog is
  a separate change.
- `LegacySkinParser` used to return `nullptr` for overview/visual/spinny/VU nodes under the flag.
  It now builds them. That parser only serves the removed classic skins, so there is no QML runtime
  impact, but the dead path is left as-is rather than deleted here.

## Verification

- Not run in this worktree: no C++ toolchain, Qt or CMake, so no build and no
  `QmlStartupSmokeTest`/`QmlResponsiveBoundsTest`. Product CI on PR #76 covers those; the only red
  job is macOS arm64 at "Pre-fetch build environment" (infrastructure).
- Not re-run here: launching the app with a stale `--new-ui` (and `--allow-dangerous-...`). The
  parser path is covered by inspection: both options are registered as hidden no-ops, so
  `QCommandLineParser::parse()` succeeds and only the warning branch runs.
- `python tools/qml_qt_api_guard.py`: 279 files scanned, 0 violations.
- `clang-format 19.1.3 --dry-run --Werror` on the touched C++ files: no output.
