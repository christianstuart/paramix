# F04 worklog - remove the `--new-ui` flag

## What changed

- `CmdlineArgs`: dropped `isQml()`, `isAwareOfRisk()`, `m_qml`, `m_awareOfRisk` and the
  `allow-dangerous-data-corruption-risk` option. A hidden, ignored `--new-ui` (and `--qml`) option
  keeps old shortcuts and the Android manifest launching; both log a warning.
- `QmlApplication`: deleted the Mixxx 3.0 data-corruption gate. The sound-device failure path is
  unchanged.
- `restartApplication()` no longer takes a UI flag and drops the obsolete arguments.
  `QmlApplicationProxy::restartApplication()` and the Help menu action are gone; nothing in QML
  called them.
- Every `isQml()` call site resolved to the flag-absent branch, i.e. exactly what the app did
  before: library navigation, scanner progress and legacy skin parsing keep their bodies, while the
  QML-only branches in `DlgPrefInterface`/`DlgPrefMixer` are removed and `DlgPrefInterface` keeps
  hiding the Paramix group boxes.
- `packaging/android/AndroidManifest.xml` no longer passes `--new-ui`.
- Smoke-test parameter `NewUi` renamed to `Default`; docs and `AGENTS.md` updated.

## Follow-up decision for the PR

The `[Paramix]` colour scheme, font, motion and visibility controls in the Qt `DlgPrefInterface`
were only reachable with `--new-ui`, which nobody passes, so that UI was dead. The QML side reads
and writes the same keys through `Mixxx.Config` (`src/qml/qmlconfigproxy.cpp`), so no behaviour was
lost. If the Qt dialog should expose those settings again, re-add them unconditionally as a separate
change.

## Verification

- Build: `cmake --build build` succeeds in `C:\Users\Bradl\paramix-fx-shell`.
- `git grep -n "isQml()\|isAwareOfRisk\|allow-dangerous"` returns only the ignored-flag shim and
  the docs.
- `QmlStartupSmokeTest` and `QmlResponsiveBoundsTest` cannot run on this Windows machine: both
  crash with an access violation in the test body even on unmodified `main` (no display/GL, missing
  Qt fonts). CI is the authority for the QML tests.
- Manual stale-flag check done: `paramix.exe --new-ui` starts and keeps running; the flag is
  accepted and ignored.
