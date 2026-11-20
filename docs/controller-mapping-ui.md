# Controller-based MIDI mapping

Paramix maps a MIDI controller from the hardware, not from the software control.
Open it with the **Map** key in the main toolbar. The window edits a copy of the
device's mapping and writes a normal user mapping file, so the classic
preferences page can still open the result.

This is the UI half of F10 (issue #68). The backend is
`Mixxx.ControllerLearning` (`src/qml/qmlcontrollerlearningproxy.{h,cpp}`); the
window itself makes no controller calls of its own.

## The flow

The window has one path, top to bottom:

1. **Device.** With one MIDI controller connected it is selected automatically
   and the picker is hidden. With several, choose one from the list. The header
   shows the active mapping name and whether it is bundled or a user mapping.
   Only MIDI devices are listed; HID and bulk devices are not supported by the
   learning proxy.
2. **Move a control.** The window starts a learning session immediately. Move a
   knob, fader, button or encoder on the hardware; the card shows the raw
   message (`status control value` in hex), the 1-based MIDI channel, the op
   code, the control number and the value. If that physical control already has
   a mapping, the card names the Paramix control it is bound to and warns that
   assigning a new target replaces the old mapping.
3. **Choose a target**, either:
   - **Click a control** in the main window (learn-by-click), or
   - search the list by title, description, category, group or key.
4. **Options.** The detected type is shown (button, absolute knob, relative
   encoder or 14-bit). The **Override** list can force button, absolute knob or
   relative encoder. *Auto-detect* keeps what the backend guessed. The override
   is applied as a raw MIDI option mask; `Invert` and `SoftTakeover` are kept
   from the guess.
5. **This session.** Every learned control appears in the list with its
   config key, physical message and option names. **Undo last** removes the most
   recent row; **Delete** removes one control. Both also remove the control's
   mappings from the working mapping.
6. **Save.** Writes the working mapping to the user mapping directory, closes
   the session, reopens the device with the new file and returns the path. For a
   bundled mapping this creates a copy; the bundled file is never modified.
   **Continue** starts a fresh learning session on the same device.

Closing the window stops learning. Unsaved changes are discarded, as with the
classic learning wizard; the Save panel says so.

## Learn-by-click: what actually resolves

There is no generic way for QML to read the `Mixxx.ControlProxy` behind an item:
`ControlProxy` is a separate `QObject`, not part of the item's properties. The
overlay (`ControllerMappingLearnOverlay.qml`) therefore resolves the item under
the press and walks its parent chain looking for a pair of `group` / `key`
string properties, then checks that the pair is in the learnable inventory.

This works for the mixed-style control primitives, which carry that pair:

| Component | Fields |
|---|---|
| `res/qml/ControlButton.qml` | `required property string group` / `key` |
| `res/qml/ControlFader.qml` | `required property string group` / `key` |
| `res/qml/ControlKnob.qml` | `property alias group` / `key` |
| `res/qml/ControlMiniKnob.qml` | `property alias group` / `key` |

So transport (play/pause, cue), faders, EQ knobs, quick-effect knobs, the
crossfader and the output knobs resolve. An item also has to look interactive
(`pressed` or `value`), so a read-only meter or a layout container that happens
to forward a group is skipped.

It does **not** resolve:

- controls whose key lives only in a behaviour (`HotcueButton`'s
  `HotcueButtonBehavior` picks `hotcue_N_activate`/`hotcue_N_set` internally,
  and `SyncButton` only exports `group`);
- controls built from a bare `Mixxx.ControlProxy` child without a `group`/`key`
  on the item, such as many library, preview-deck and mixer readouts;
- controls that are not in `ControlPickerModel`'s inventory (for example
  Pad FX parameters).

When nothing resolves, the overlay stays in learn mode and says so; use the
search list instead. That fallback is the supported path for those controls.

**Smallest backend hook that would fix it.** An explicit attached target on the
item, for example a C++ `QML_ATTACHED` type `MappingTarget { group; key }` set
next to the proxy that the control already declares, used by the overlay instead
of guessing from the item's properties. Alternatively a
`Mixxx.ControllerLearning.controlAt(item)` helper that returns the nearest
`QmlControlProxy` registered by the control primitive. Both add a C++ member to
the learning/QML proxy, which this PR was not allowed to do; the window is
designed so the overlay can be pointed at either without touching the rest of
the flow.

If the overlay resolves an item that is not a real control, the assignment still
shows up with a `lastError` and can be deleted from the session list.

## Detected options and the override

`lastMessage.type` and `suggestedMappings[0].options` come from
`LearningUtils::guessMidiInputMappings`, the same detection the classic wizard
uses. Assigning sends the suggestion(s) plus the override mask to
`ControllerLearning.assign(group, key, options)`.

A 14-bit control is detected as a pair of MSB/LSB mappings and is shown as
*14-bit*. It cannot be forced from the override list: the backend applies one
option mask to every suggested message, so forcing the 14-bit flags by hand
would mark both messages as MSB. Auto-detect is the only correct path there.

## Session list, undo and save

- The session list is the backend's `pendingMappings` model, one row per Paramix
  control. A single press can expand to two mappings (Note On + Note Off) or two
  14-bit messages; they are kept together and deleted together.
- There is no redo stack. "Undo last" is `removeAt(last row)`, which removes the
  mappings from the working mapping as well as the row.
- Reassigning a physical control replaces its existing mapping in the working
  mapping, including one that came from the bundled file. That change is saved
  like any other.
- `save()` writes `<user mapping dir>/<name>.midi.xml`, adopts it through
  `ControllerManager::slotApplyMapping` and stops learning. The name field is
  prefilled with the mapping's name (or the device name for a new mapping).
  Changing the name after saving and saving again overwrites the original file,
  because the working mapping remembers its path; delete the old file in the
  controller preferences page if you want a different one.
- A controller with no mapping at all gets an empty mapping created and applied
  when the session starts, so its first mapping is created here. A controller
  with an existing mapping must already be open/enabled; this window does not
  change enabled state.

## Accessibility and sizing

Every interactive control has a tooltip and `Accessible.name`; list rows use
`Accessible.ListItem` with the control title and config key, and the message
card is a `Grouping` with a spoken summary of channel, op code and whether it is
already mapped. Sizes and spacing come from the `Theme` tokens. The window
minimum is 900x600.

## Files

| File | Purpose |
|---|---|
| `res/qml/ControllerMapping/ControllerMappingWindow.qml` | Window, device picker, detection and save flow |
| `res/qml/ControllerMapping/ControllerMappingMessageCard.qml` | Last message and existing-mapping card |
| `res/qml/ControllerMapping/ControllerMappingControlList.qml` | Searchable target list |
| `res/qml/ControllerMapping/ControllerMappingControlIndex.qml` | One-shot snapshot of `availableControls` |
| `res/qml/ControllerMapping/ControllerMappingPendingList.qml` | Session list, undo, delete |
| `res/qml/ControllerMapping/ControllerMappingLearnOverlay.qml` | Learn-by-click overlay in the main window |

`res/qml/ControllerMapping` needs no registration file: like `Deck`, `Mixer`,
`Effects` and `Library` it is imported as a directory
(`import "ControllerMapping" as ControllerMappingComponents`), loaded from the
source tree in a developer run, and shipped by the existing
`install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/res/qml" ...)` rule.

## Backend API used

`Mixxx.ControllerLearning` (`src/qml/qmlcontrollerlearningproxy.h`):
`learning`, `lastMessage`, `suggestedMappings`, `pendingMappings`,
`availableControls`, `mappingName`, `lastError`, `existingMappingFor()`,
`assign()`, `removeAt()`, `start(device)`, `stop()`, `save(name)`, and the
`mappingChanged` signal.

`Mixxx.ControllerManager` (`src/qml/qmlpreferencesproxy.h`): `knownDevices`,
`unknownDevices`, `deviceListChanged`. Device fields used: `name`, `vendor`,
`product`, `serialNumber`, `type`, `mapping`; mapping fields: `name`,
`isUserMapping(Mixxx.Config)`.
