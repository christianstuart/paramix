# Paramix product identity

This note records the user-visible identity of the Paramix fork: what was
renamed, what intentionally keeps the upstream name, and what still needs work.
It complements `docs/paramix-ui-design-system.md` and the legal attribution in
`NOTICE`.

## 1. Identity

| Item | Value |
|---|---|
| Product name | **Paramix** |
| Application name (`VersionStore::applicationName()`) | `Paramix` |
| Organization domain | `paramix.org` (reverse-DNS group `org.paramix`) |
| Package / bundle / Flatpak ID | **`org.paramix.Paramix`** |
| Linux desktop entry | `res/linux/org.paramix.Paramix.desktop` (`Name=Paramix`, `Exec=paramix`, `Icon=paramix`) |
| AppStream metadata | `res/linux/org.paramix.Paramix.metainfo.xml` |
| Installed executable | `paramix` / `paramix.exe` (the CMake target stays `mixxx`; only `OUTPUT_NAME` changes) |
| macOS bundle | `Paramix.app`, identifier `org.paramix.Paramix` |
| Linux data dir | `share/paramix` (`../share/paramix` is preferred at runtime, with `../share/mixxx` as an upgrade fallback) |
| Settings dir | `~/.paramix` on Linux/BSD; `AppLocalDataLocation` (`Application Support/Paramix` on macOS) elsewhere; log file `paramix.log` |
| Windows product name / installer | `Paramix`, `Paramix` install directory, new WiX upgrade GUID |
| App icon | `res/images/icons/scalable/apps/paramix.svg` plus generated PNG/ICO/ICNS/BMP |

The window title, error-dialog titles, notifications, MusicBrainz user agent,
JACK client name and console banner all derive from
`VersionStore::applicationName()`, so they follow the table above. A
non-bundled macOS developer build additionally sets `NSProcessInfo.processName`
to `Paramix` (`src/util/macprocessname.mm`) so the system menu bar shows the
capitalized name instead of the executable name `paramix`.

## 2. Brand assets

- App icon: four stem lanes of varying height in the theme palette on a dark
  rounded tile (`res/images/icons/scalable/apps/paramix.svg`).
- Interface font: **Inter** is bundled in `res/fonts/InterVariable.ttf` (OFL)
  and loaded by `FontUtils`; the QML skin defaults to it and the customizer can
  switch to a system family.
- Wordmark: `res/images/paramix_logo.svg` (About dialog) and
  `res/images/paramix-icon-logo-symbolic.svg` (splash / symbolic lockup).
- Raster output: `res/images/icons/{32,64,128,256,512}/apps/paramix.png`,
  `res/images/icons/ic_paramix.ico`, `res/osx/application.icns`,
  `res/osx/VolumeIcon.icns`, `res/images/paramix_install_logo.bmp`.
- Installer artwork: `packaging/wix/images/{banner.bmp,dialog.bmp,
  bundle_background_black.png,bundle-sidebar.png}`.
- Legacy skins: the `mixxx_logo*.svg` files keep their names because the skin
  XML references them, but their contents are the Paramix lockup. The Shade
  symbolic logo and the preference symbolic icons are replaced in place.

To regenerate the raster output from the SVG sources, render the SVG with any
SVG renderer, build an `.iconset`, run `iconutil -c icns`, and write the ICO as
PNG-compressed entries. `packaging/macos/regenerate_icns.sh` and
`packaging/ios/regenerate_appiconset.sh` use the Paramix SVG inputs.

## 3. What intentionally still says "Mixxx"

These are **upstream references**, not product branding:

- The **About** dialog keeps "Based on Mixxx, the free DJ software", the full
  historical contributor lists and the GPL notice. The website link points to
  the Paramix repository.
- **License headers**, `NOTICE`, the WiX/GPL text and the bundled effect
  credits ("The Mixxx Team") remain.
- **Version-history copy** such as "before Mixxx 2.4" or "Mixxx versions before
  1.11" remains, because it describes upstream releases. The AppStream
  `<releases>` section is inherited upstream history for the same reason.
- **Legacy file/identifier names**: `mixxxdb.sqlite`, `.mixxx` import paths,
  `MixxxControl`, `MixxxLibraryFeature`, the `Mixxx` QML module, the `mixxx`
  CMake target, `#Mixxx` skin object names and MIDI preset XML elements.
  Engine DJ exports keep the stable `Mixxx Crates` / `Mixxx Playlists` root
  names so an existing exported database is not duplicated.
- **Documentation links** that genuinely point at upstream resources
  (`manual.mixxx.org`, the Mixxx wiki and controller forums). Labels for those
  links are neutral ("Wiki", "Controller Forums", "DJ Hardware Guide") so the
  app does not claim to own upstream documentation.

## 4. Migration note

The settings directory moved with the product name (`~/.mixxx` → `~/.paramix` on
Linux, `Application Support/Mixxx` → `Application Support/Paramix` on macOS and
`%LOCALAPPDATA%\Mixxx` → `%LOCALAPPDATA%\Paramix` on Windows). The macOS sandbox
migration still imports `Application Support/Mixxx` when present.

There is **no first-run migration wizard yet** for the non-sandboxed case. That
is the planned `~/.paramix` wizard (decision 10): copy the library database,
settings and controller mappings, verify the copy, then switch and leave the old
directory untouched. Until it lands, a user who upgrades in place should move
the old directory manually or pass `--settings-path`. Saved Icecast/
Shoutcast passwords are stored under a `Mixxx - ` keychain prefix and must be
re-entered once.

## 5. Customization and settings

The Paramix QML interface has a dedicated **Customize Paramix** panel for
scheme, accent color, font, type scale, motion, panel visibility, waveform
playhead/style and stem lanes. The values live in a `[Paramix]` config group and
are injected into the `Theme` singleton. The toolbar gear opens the native Mixxx
Preferences dialog, which remains the primary settings window for audio,
library, controllers and analysis. The QML settings popup added during the UI
overhaul is retired. The customizer can restart into the classic widget
interface, and the native Help menu can restart back into the QML interface.
See `docs/paramix-ui-design-system.md` §6.

## 6. Follow-ups

- Debian/Ubuntu packaging (`packaging/debian`, `packaging/CPackDebInstall.cmake`)
  still uses the upstream package name, man page and udev rule file names.
- The WiX cross-scope upgrade patch still looks for old Mixxx installations; it
  is inert with the new upgrade GUID but should be reviewed.
- Translations of the new strings.
- Flip the Paramix QML skin to the default (packaging decision, see the UI
  design-system note).
