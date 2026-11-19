# F04 - Remove the `--new-ui` launch flag

- **Branch:** `bradley/fix-remove-new-ui-flag`
- **Worktree:** `C:\Users\Bradl\paramix-fx-shell`
- **Lane / wave:** Shell lane, wave 1 (merge first in the wave)
- **Depends on:** wave 0, including the decision on the owner's uncommitted library work
- **Blocks:** nothing outside its lane
- **Needs its own build:** yes (C++)

## Owned files

- `src/util/cmdlineargs.{h,cpp}`, `src/main.cpp`, `src/qml/qmlapplication.cpp`
- `src/util/restartapplication.cpp`, `src/qml/qmlapplicationproxy.cpp`
- `src/widget/wmainmenubar.cpp` (the "Use Paramix QML Interface" restart action)
- `isQml()` call sites: `src/library/librarycontrol.cpp`, `src/library/scanner/libraryscanner.cpp`,
  `src/preferences/dialog/dlgprefinterface.cpp`, `dlgprefmixer.cpp`,
  `src/skin/legacy/legacyskinparser.cpp`, `src/preferences/upgrade.cpp`
- `packaging/android/AndroidManifest.xml`
- `src/test/qml/qmlstartupsmoketest.cpp` (the `NewUi` parameter naming only)
- `AGENTS.md`, `docs/paramix-ui-design-system.md`, `docs/controller-mapping-compatibility-plan.md`

## What is known

- The QML interface is already the only one: `main.cpp` always loads it. The flag is a no-op for
  the UI choice.
- What the flag still does: `CmdlineArgs::isQml()` is true only when it is passed. That gates
  1. the Mixxx 3.0 data-corruption check in `QmlApplication` (blocks an existing profile unless
     `--allow-dangerous-data-corruption-risk` is also given), and
  2. about a dozen `isQml()` branches elsewhere, so today the app runs the QML UI with the
     flag-absent branch of each of them.
- `restartApplication(true)` re-adds both flags.
- `src/library/librarycontrol.cpp` has uncommitted owner changes (library controller navigation).
  **Do not start the `isQml()` cleanup in that file until wave 0 has landed or parked that work.**

## Steps

1. Delete the `new-ui` and hidden `qml` options, `m_qml`, `isQml()`,
   `allow-dangerous-data-corruption-risk`, `m_awareOfRisk` and `isAwareOfRisk()`.
2. Delete the corruption gate in `QmlApplication`. Paramix uses the same schema as the classic UI,
   so the gate has no remaining purpose. Keep the constructor's "return instead of `exit()`"
   structure for other startup errors.
3. Resolve every `isQml()` call site to the behavior the app has today, which is the flag-absent
   branch, since nobody launches with the flag. Do not flip behavior silently. Where the other
   branch is what a QML-only app should do, list it in the PR description as a follow-up decision
   instead of changing it here.
4. `restartApplication`: stop adding the flags; drop the boolean if nothing else uses it. Remove
   the Help menu restart action and its QML-callable twin if they only existed to switch UI.
5. Android manifest: remove `--new-ui` from the arguments.
6. Unknown-option behavior: make sure an old shortcut that still passes `--new-ui` starts the app
   with a warning rather than failing. If the parser rejects unknown options here, keep a hidden,
   ignored `new-ui` option for one release and say so in the PR.
7. Rename the smoke-test parameter away from `NewUi`; update the three docs and `AGENTS.md`.

## Verification

- `git grep -n "new-ui\|isQml()\|isAwareOfRisk\|allow-dangerous"` returns nothing outside the
  one-release compatibility shim, if kept.
- `QmlStartupSmokeTest` and the bounds test pass.
- Manual: start with no arguments on the owner's existing profile; start with a stale `--new-ui`.

## Done when

No code path depends on the flag, the app starts identically with no arguments, and the docs no
longer mention it.
