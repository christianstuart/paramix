# F10 - Controller-based MIDI mapping UI

- **Branches:** `bradley/midi-mapping-backend`, `bradley/midi-mapping-ui` (two stacked PRs)
- **Worktree:** `C:\Users\Bradl\paramix-fx-mapping`
- **Lane / wave:** Mapping lane, starts in wave 1; UI entry point lands in wave 3
- **Depends on:** wave 0; F06 merged before the `MainWindow.qml` entry is added
- **Blocks:** nothing
- **Needs its own build:** yes (C++)

## Owned files

- New `src/qml/qmlcontrollerlearningproxy.{h,cpp}` and its test
- `src/qml/qmlpreferencesproxy.{h,cpp}` (additive: the existing `Mixxx.ControllerManager` types)
- `src/controllers/controlpickermenu.{h,cpp}` (extract the control list into a model both can use)
- New `res/qml/ControllerMapping/*.qml`
- `res/qml/MainWindow.qml` - one entry point (last, after F06)
- `CMakeLists.txt` entries (see master plan for the protocol)
- New `docs/controller-mapping-ui.md`

Do not change `src/controllers/midi/*` learning behavior or the mapping file format.

## What is known

- The classic learning wizard (`DlgControllerLearning`) works but lives in the QtWidgets
  preferences dialog. Its click-to-learn filter only recognizes `WWidget`, so clicking a control
  in the QML UI does nothing.
- Backend pieces to reuse: `MidiController::startLearning` / `stopLearning` and its
  message-received signal, `LearningUtils::guessMidiInputMappings`, `ControlPickerMenu`'s list of
  mappable controls with titles and descriptions, `LegacyControllerMappingFileHandler` for saving
  to the user mappings directory.
- QML already has `Mixxx.ControllerManager` with device and mapping proxies
  (`qmlpreferencesproxy.*`), including `save()`. Nothing in `res/qml` uses them yet.
- `docs/controller-mapping-compatibility-plan.md` lists learn-by-click as a known P1 gap.

## Design

A dedicated window with one flow, driven from the hardware:

1. Pick a device (skipped when only one is connected). Show its current mapping name.
2. **Move a control on the controller.** The window shows what arrived (channel, type, number) and
   whether it is already mapped, and to what.
3. **Choose what it should do:** click any control in the main window (learn mode highlights
   mappable controls), or search the control list by name.
4. Options detected automatically (button, knob, relative encoder, 14-bit) with a manual override.
5. The mapped-controls list shows everything learned in this session, with undo and delete.
6. Save writes a user mapping (a copy if the active mapping is a bundled one).

Hardware-first matters: the classic wizard starts from the software control; starting from the
physical control lets someone map a whole controller without leaving it.

## Steps

**PR 1 - backend.**

1. Extract `ControlPickerMenu`'s control inventory into a list model (group, key, title,
   description, category) usable without a `QMenu`.
2. `Mixxx.ControllerLearning`: `start(device)`, `stop()`, `lastMessage`, `suggestedMappings`,
   `existingMappingFor(message)`, `assign(configKey, options)`, `remove(...)`, `save()`,
   `pendingMappings` model. All on the main thread, same as the wizard.
3. Tests with a fake MIDI controller: learn a button, a knob and a relative encoder; reassign;
   save and reload.

**PR 2 - UI.**

4. The window and the searchable control list.
5. Click-to-learn for QML: while learn mode is on, an overlay intercepts the press, resolves the
   item under the cursor to its `ControlProxy` group/key, and reports it. Add a small attached
   helper if items cannot be resolved generically. Fall back to the search list when nothing
   resolves.
6. Entry point in the toolbar/View menu and a link from the classic controller preferences page.

## Verification

- Backend tests on CI.
- Manual with a real controller: map ten controls without touching the mouse except to pick
  targets; restart; mappings persist; bundled mapping file is untouched.
- All bundled mappings still load.

## Done when

A new controller can be mapped end to end from the QML UI, and the result is a normal user mapping
file the classic preferences page can open.
