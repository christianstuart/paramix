# F09 - Export to Rekordbox format

- **Branches:** `bradley/rekordbox-export-anlz`, `-pdb`, `-job`, `-ui` (four stacked PRs)
- **Worktree:** `C:\Users\Bradl\paramix-fx-rekordbox`
- **Lane / wave:** Export lane, starts in wave 1 and runs across all waves
- **Depends on:** wave 0; the owner's uncommitted library work for the last PR only
- **Blocks:** nothing
- **Needs its own build:** yes (C++)

## Owned files

- New `src/library/export/rekordbox/*` (writers, job, request)
- New `src/test/rekordboxexport*test.cpp`
- `src/library/export/libraryexporter.{h,cpp}`, `dlglibraryexport.{h,cpp}` (add a target; last PR)
- `src/qml/qmllibraryproxy.{h,cpp}`, `res/qml/ApplicationMenuActions.qml` (one action; last PR)
- `CMakeLists.txt` entries (see master plan for the protocol)
- New `docs/rekordbox-export.md`

Do not touch `src/library/sidebarmodel.*`, `src/qml/qmlsidebarmodelproxy.*` or the feature headers
the owner has uncommitted edits in until wave 0 has resolved them.

## What is known

- "rbexport" is [chrisle/rbxport](https://github.com/chrisle/rbxport), a Rust workspace. The
  relevant crates are `rbl-pdb`, `rbl-anlz`, `rbl-export` and possibly `rbl-onelibrary`.
- This repository once ported its `rbl-analysis` crate to C++ (`c4ee66d`) and removed it
  (`ac5d0e0`). No export code was ever here.
- **License gate.** The earlier port note recorded rbxport as GPL-2.0-or-later, but GitHub reports
  no detected license for the repository today. Confirm the license file and crate manifests at
  the commit being ported, and record the commit hash, before copying or translating any code.
  Paramix is GPL-2.0-only; Apache-2.0 or GPL-3.0-only code cannot be used.
- Rekordbox support in the tree is import-only: Kaitai-generated readers in
  `lib/rekordbox-metadata/` (EPL-1.0 `.ksy`) and `src/library/rekordbox/rekordboxfeature.*`.
- `EnginePrimeExportJob` + `LibraryExporter` + `DlgLibraryExport` is the pattern to mirror: load
  tracks on the main thread through `TrackCollectionManager`, write on a worker thread, expose it
  through `QmlLibraryProxy` and `ApplicationMenuActions.qml`.

## Decisions (defaults; integrator confirms before PR 1)

- **Target:** a Rekordbox device export (USB/SD): `PIONEER/rekordbox/export.pdb` plus
  `PIONEER/USBANLZ/**/ANLZ0000.DAT/.EXT`, with audio copied to `Contents/`. `rekordbox.xml` is a
  possible later addition and is not in scope.
- **Method:** C++ port of the writers, as was done for the analysis crate. Linking the Rust crates
  would add a Rust toolchain to all four CI targets.
- **Scope v1:** selected crates/playlists or the whole library; track metadata, BPM, key, rating,
  color; beatgrid; hot cues, memory cues, loops; preview and detail waveforms; playlist tree.

## Steps (one PR each)

1. **ANLZ writer.** Path, beat grid, cues and waveform sections, written from `mixxx::Beats`,
   `Cue` and `Waveform`. Take the exact section list from `rbl-anlz` and
   `lib/rekordbox-metadata/rekordbox_anlz.ksy`. Test by writing a file and reading it back with
   the existing Kaitai reader, plus a byte comparison against a small fixture produced by the
   reference implementation.
2. **PDB writer.** Page layout, tracks, artists, albums, genres, keys, colors, playlist tree and
   entries. Same test approach, including a round trip through `RekordboxFeature`'s import path.
3. **Export job.** `RekordboxExportJob` (worker thread, progress, cancel, free-space preflight,
   atomic write to a temporary directory then rename), track copy, path mapping, MP3 timing offset
   handled the same way the importer does.
4. **UI.** Target choice in the export dialog and a File-menu action in the QML UI, gated on a
   CMake option that defaults on.

## Verification

- Unit and round-trip tests on Linux and Windows CI.
- Manual: export 20 tracks and two playlists; open the stick in Rekordbox and, if available, on a
  CDJ. Hardware checks are the owner's; state clearly in each PR what was not tested.
- Importing the export back into Paramix reproduces grids and cues within one frame.

## Done when

A stick exported from Paramix loads in Rekordbox with playlists, grids, cues and waveforms intact.
