# F11 - FX banks: 1 and 2 always visible, 3 and 4 in a toggleable window

- **Branch:** `bradley/fix-fx-banks`
- **Worktree:** `C:\Users\Bradl\paramix-fx-effects-ui`
- **Lane / wave:** FX UI lane, wave 1
- **Depends on:** wave 0
- **Blocks:** F06 (same lane; both edit the `MainWindow.qml` toolbar and View menu)
- **Needs its own build:** no (QML-only; run the main build against this worktree's `res/`)

## Owned files

- `res/qml/TopBarEffects.qml`, `res/qml/Effects/*`
- New `res/qml/EffectUnitsWindow.qml`
- `res/qml/MainWindow.qml` - the effects strip instantiation, one toolbar key, one View menu item
- `res/qml/Theme/Theme.qml` - effects metrics block only
- `docs/paramix-ui-design-system.md` (effects section)

## What is known

- `TopBarEffects.qml` shows two units and pages between FX 1-2 and FX 3-4 with a key; above a
  width threshold (`showAllUnits`) it shows all four side by side. So what is on screen depends on
  window width and on a page state nobody else can see.
- Each unit (`Effects/TopBarEffectUnit.qml`) has a badge that opens a parameter popup, a power
  dot, A-D deck assign keys, three effect selectors with on keys, dry/wet and a super knob that
  hides below `Theme.fxSuperMinWidth`.
- Window pattern to copy: `PadFxWindow.qml` / `DeveloperToolsWindow.qml` (a `Window` toggled by a
  checkable toolbar key and a View menu `CheckMenuItem`).
- `[Skin],show_4effectunits` exists as a control but the QML strip ignores it;
  `Effects/EffectAssignmentButtons.qml` still references it.

## Steps

1. Strip: always units 1 and 2, no paging and no four-wide mode. Give the freed width to the two
   units (longer effect names, super knob visible at smaller widths).
2. `EffectUnitsWindow.qml`: units 3 and 4 stacked, using the same `TopBarEffectUnit` component so
   there is one implementation. Small, resizable, remembers size and position.
3. Toggle: a toolbar key labeled "FX 3-4" and a View menu item, both bound to
   `[Skin],show_4effectunits` so a controller can open it and the state persists. Closing the
   window clears the control.
4. Clean up while here, within the owned files only: consistent labels and spacing between slots,
   tooltips on the assign keys and badges, remove dead paging code and the stale reference in
   `EffectAssignmentButtons.qml`.
5. Update the bounds test expectations only if the strip height changes (it should not).

## Verification

- Grabs at 1280x720 and 2560x1369: strip shows exactly FX 1 and FX 2; window shows FX 3 and FX 4.
- Units 3 and 4 fully operable from the window; a controller mapped to unit 3 stays in sync.
- `QmlStartupSmokeTest`, bounds test and the QML Qt-API guard pass.

## Done when

FX 1-2 are fixed in the top bar at every window size and FX 3-4 live in a window with a visible,
persistent toggle.
