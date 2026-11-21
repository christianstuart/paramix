# Rekordbox device export

Paramix can import rekordbox analysis files and databases but, until this
work, could not write them. This document describes the writers, the export
job and the UI added for F09. It covers **PR 1, the ANLZ analysis writer**,
**PR 2, the `export.pdb` database writer**, **PR 3, the export job**, and
**PR 4, the target choice and menu action**.

## What an export contains

A rekordbox USB/SD export keeps three things per track:

```text
PIONEER/
  rekordbox/
    export.pdb              playlists and track metadata
  USBANLZ/
    <path-derived directory>/
      ANLZ0000.DAT          path, beat grid, previews, memory/A-C cues
      ANLZ0000.EXT          scrolling waveform, D-P and extended cues
      ANLZ0000.2EX          3-band waveforms
Contents/
  <audio files>
```

The writers produce the three ANLZ files and `export.pdb`. The export job copies
the audio and lays out the tree below.

## Format

An ANLZ file is a `PMAI` header followed by type-tagged sections. Each section
is `fourcc`, `len_header`, `len_tag`, then the tag-specific header bytes up to
`len_header`, then the payload up to `len_tag`. The middle part is the easy
place to be wrong: `len_header` differs per tag (16 for `PPTH`, 20 for `PWAV`,
24 for `PQTZ`, 14 for `PWVC`).

The file header is `PMAI`, `len_header = 28`, `len_file`, then 16 bytes whose
meaning is undocumented and that are reproduced rather than derived. The writer
uses the same bytes rekordbox writes.

The sections the writer authors, in file order:

| File | Sections |
|---|---|
| `.DAT` | `PPTH`, `PVBR`, `PQTZ`, `PWAV`, `PWV2`, `PCOB` (hot), `PCOB` (memory) |
| `.EXT` | `PPTH`, `PWV3`, `PCOB` (hot), `PCOB` (memory), `PCO2` (hot), `PCO2` (memory), `PWV5`, `PWV4` |
| `.2EX` | `PPTH`, `PWV7`, `PWV6` |

### Path

`PPTH` stores the audio path the device resolves, UTF-16BE with a NUL
terminator. A desktop share tree names the file as `?/name.mp3`; a USB export
names the full `/Contents/...` path. The writer takes the path as given, so the
export job decides which form to use.

### Beat grid

`PQTZ` holds one 8-byte entry per beat: the beat number in the bar (1 is the
downbeat), the tempo in BPM × 100, and the time in milliseconds at 100% pitch.
The header carries a signed offset, written as zero, and the beat count.

The writer walks the Mixxx `Beats` grid from the first beat to the track
duration and derives each tempo from the interval to the next beat. Beat
numbers use `Beats::getDownbeatInfo()` when a downbeat-aware analysis recorded
one; otherwise the grid is numbered 1-4 from the first beat.

### Cues

The two cue tags are a list of cues and loops:

- `PCOB` is the legacy list: 56-byte `PCPT` entries, no comments or colors.
  A `.DAT` carries memory cues, loops and hot cues A-C. An `.EXT` carries
  hot cues D-P in the same form.
- `PCO2` is the extended list: 88-byte `PCP2` entries plus a UTF-16BE comment,
  a memory cue color row and the hot cue RGB. An `.EXT` carries the complete
  hot and memory lists.

Each list has a type word (0 memory, 1 hot) and a cue count. A populated memory
list's last header word is its last index; a hot or empty list writes
`0xffffffff`. Entries carry the hot cue slot (0 memory, 1-16 A-P), the
position, the loop end or `0xffffffff`, and the loop/active-loop state.

Mixxx `Cue` objects map as follows:

| Mixxx type | Device form |
|---|---|
| `HotCue` | hot cue, slot `hotCue + 1` |
| `Loop` with a hot cue index | hot loop |
| `Loop` without a hot cue index | memory loop |
| `MainCue` | memory cue |
| `Intro`, `Outro`, analysis cues | not exported |

Hot cue colors are mapped to the nearest entry of the 65-color device palette.
Memory cue colors are not exported yet (see below).

### Waveforms

The preview and detail sections are generated from `Waveform` band columns:

| Tag | Columns | Encoding |
|---|---|---|
| `PWAV` | 400 | five-bit height plus three-bit whiteness, height at least 2 |
| `PWV2` | 100 | five-bit height only, clamped to 1-15 |
| `PWV3` | 150/second | five-bit height plus whiteness; detail height |
| `PWV4` | 1200 | peak, two unused bytes, then red/green/blue bands |
| `PWV5` | 150/second | `rrrgggbbbhhhhh00`, colors normalized to the strongest band |
| `PWV6` | 1200 | low, mid, high bands at the `PWV6` scale |
| `PWV7` | 150/second | low, mid, high bands at full seven-bit detail |

Preview tags take the loudest column of each bucket. Detail tags use
rekordbox's squared sample-peak curve for the height. The three band scales and
the color weights are observed values measured from rekordbox's own files; they
are recorded rather than specified, as noted in the provenance statement below.
A silent preview column is not zero: players read a zero-height preview as "not
analyzed" and rebuild it.

## The PDB database

`PIONEER/rekordbox/export.pdb` is a DeviceSQL database: a file of fixed 4096-byte
pages. Page 0 is a header that names the tables. Each table is a linked list of
pages; a table's index page links to its first data page, and rows are stored in
a heap that grows forward from the page header while their offsets are indexed
backward from the end of the page, sixteen to a group, each group preceded by a
presence bitmap. Deleted rows keep their index entry and clear their bit, so a
reader that ignores the bitmap parses garbage.

The writer emits the twenty tables rekordbox writes, in its order:

| Type | Table | Contents |
|---|---|---|
| 0 | tracks | one row per track |
| 1 | genres | id and name |
| 2 | artists | id and name |
| 3 | albums | id, artist id and name |
| 4 | labels | empty (not written yet) |
| 5 | keys | id twice and name |
| 6 | colors | the eight device colors |
| 7 | playlist_tree | playlists and folders |
| 8 | playlist_entries | playlist membership and order |
| 9-15 | history, artwork and unknown | empty |
| 16 | columns | the 27 browse column names |
| 17 | categories | the default browse categories |
| 18 | sorts | the default sort options |
| 19 | property | device name, track count, date and background color |

Track rows carry the fixed fields rekordbox uses (sample rate, file size,
bitrate, duration, tempo, year, rating, color, the artist/album/genre/key/color
ids and the media-relative file and analysis paths) plus 21 string slots. The
strings use DeviceSQL's three encodings: short ASCII with its mangled length
byte, long ASCII above 125 bytes, and long UTF-16LE for anything ASCII cannot
carry. Long strings start on a four-byte boundary of the row.

Playlist folders and playlists share one table; `is_folder` distinguishes them,
`parent_id` and `sort_order` build the tree, and `playlist_entries` gives the
position of each track in each playlist.

The `property` row names the device, counts the tracks, records the export date
(`YYYY-MM-DD`) and the Device Library background color. The browse column,
category and sort rows are the default rows a player shows; they are recorded
as observed bytes rather than generated from names.

The writer takes the numeric ids from the caller. Allocating them and mapping
Paramix tracks, artists, albums and playlists onto them is the export job's
work, so the writer stays a pure function of its input.

## The export job

`RekordboxExportJob` runs on a worker thread and writes the device tree. It
loads tracks and their stored waveform summaries on the collection thread,
then copies audio, writes ANLZ files and builds `export.pdb` on the worker
thread.

The request selects the device root and either the whole library or a set of
crates and playlists. Selected crates only choose tracks; playlists also become
`playlist_tree` rows. With neither crates nor playlists selected, the whole
library and every non-hidden playlist are exported.

Paths and ids are assigned deterministically:

```text
Contents/<Artist>/<Album>/<track id> - <file name>
PIONEER/USBANLZ/P<folder>/<track id>/ANLZ0000.DAT
```

The audio path, `PPTH` and the PDB `filePath` are media-relative
(`/Contents/...`). Artist, album, genre and key rows are deduplicated and
numbered from 1 in track order. Playlist rows are numbered from 1 in playlist
order.

The job writes into a staging directory under the device root and renames
`PIONEER` and `Contents` into place only after every file is written. It
refuses to start when either directory already exists, so an existing
rekordbox export must be erased or a different empty device chosen. This keeps
the commit from replacing a library the user did not select.

MP3 timing offsets are applied in the same decoder-dependent way as the
importer, by adding the offset to every ANLZ beat and cue time. A non-MP3 file
gets offset 0. File types rekordbox cannot play (for example Ogg and Opus) are
skipped with a warning; if every selected track is skipped, the job fails
instead of writing an empty export.

The job does not export track colors yet: `PdbTrack::colorId` is 0, because the
eight device color rows are names rather than RGB values and the mapping from
an arbitrary Mixxx color has not been established.

## User interface

The existing library-export dialog gains a **Target** choice: Engine DJ or
Rekordbox. The rekordbox target is compiled when `REKORDBOX_EXPORT` is on
(default) and the export dialog is built, which currently requires
`ENGINEPRIME`; the writers and the job are always built.

Choosing Rekordbox treats the selected directory as the device root. The
dialog warns when it already contains `PIONEER/` or `Contents/`, because the
job refuses to overwrite an existing export. The version selector and the
existing-database message belong to Engine DJ and are hidden for rekordbox.

The selection model is shared: whole library, or a set of crates and
playlists. The File menu gains **Export Library to Rekordbox...,** enabled by
`Mixxx.Library.rekordboxExportAvailable`; it opens the dialog with the
rekordbox target preselected. The existing Library menu action still opens the
dialog at the Engine DJ target. The QML property is false in builds without
`REKORDBOX_EXPORT`.

## Provenance


The owner has waived the license gate. The writers are C++ translations of two
crates from [chrisle/rbxport](https://github.com/chrisle/rbxport) at commit
`d0ff75b378ec85be891d9f3067ada1e82bad4cae` (`chore(release): cut 1.10.0`,
2026-11-07).

The license was checked in the repository itself at that commit. `LICENSE`
reads, verbatim:

> rbxport is licensed under the GNU General Public License, version 2 or (at
> your option) any later version (GPL-2.0-or-later).
>
> Copyright (c) 2026 Chris Le

`LICENSING.md` repeats: "rbxport is licensed as a whole under
GPL-2.0-or-later." The complete GPL text that `LICENSE` points to is
`crates/rbl-deck/vendor/rubberband/COPYING`.

**GPL-2.0-or-later is compatible with this repository's GPL-2.0-only terms:** a
recipient may choose version 2 of the GPL. No GPL-3.0-only or Apache-2.0 code is
involved.

### ANLZ writer (`rekordboxanlzwriter.*`)

- Derived from rbxport's `rbl-anlz`: the section framing, the cue and loop
  encodings, the waveform packings and scales, the 65-entry device palette and
  the section order in each file.
- `lib/rekordbox-metadata/rekordbox_anlz.ksy` (EPL-1.0) is the in-tree
  cross-check; the test writes files and reads them back with the
  Kaitai-generated reader built from it.
- An earlier revision asserted a `PCP2` byte string and a table of `PWV5` and
  `PWV3` values taken from `rbl-anlz`'s tests. Those fixtures were removed in a
  later pass, so the test is now a structural round trip. The implementation
  remains a translation of `rbl-anlz`, not clean-room work.
- The same pass changed `PVBR` to one unused word plus 400 entries, the body the
  `.ksy` describes.

### PDB writer (`rekordboxpdbwriter.*`)

- Derived from rbxport's `rbl-pdb`: the page layout, table directory, page
  header values, row layouts, string encodings and the constant browse rows
  (`columns`, `categories`, `sorts`).
- `lib/rekordbox-metadata/rekordbox_pdb.ksy` (EPL-1.0) is the in-tree
  cross-check; the test reads every row back with the Kaitai-generated reader.
- Numeric IDs are supplied by the caller; mapping Paramix objects onto them is
  the export job's work.

The earlier text in this section described the ANLZ writer as clean-room and
said that no rbxport input was used. That was inaccurate and is replaced by the
statements above.

## Testing

`src/test/rekordboxexportanlztest.cpp` writes files with the writer and reads
them back with the existing Kaitai reader. It covers:

- the section order and header sizes of all three files;
- a beat grid round trip, including downbeat phase;
- cue list contents in `.DAT` and `.EXT`, parsed back through both the `PCPT`
  and `PCP2` readers, including a cue comment and its palette RGB;
- waveform payload sizes, the preview floors, and the documented packing of
  `PWV4`, `PWV5`, `PWV6` and `PWV7`.

The reader is the only oracle: every assertion is either a field comparison
after a parse, or a property stated in this document. The test holds no
recorded file and no vector copied from another implementation.

Run it with:

```bash
./build/mixxx-test --gtest_filter='RekordboxExportAnlz.*'
```

`src/test/rekordboxexportpdbtest.cpp` writes `export.pdb` and reads it back
with the Kaitai-generated `rekordbox_pdb_t` reader, which is the reader
`RekordboxFeature` uses for imports. It covers:

- the twenty-table directory and the presence of every table;
- track rows: ids, tempo, duration, rating, color, file and analysis paths,
  comments, dates, the 21 string slots, and both long string encodings;
- artists, albums, genres, keys and the eight color rows;
- the playlist tree and playlist entries;
- page rollover on a 120-track table;
- the `property` row layout and the string/audio-type encoders.

```bash
./build/mixxx-test --gtest_filter='RekordboxExportPdb.*'
```

`src/test/rekordboxexportjobtest.cpp` covers the job's pure parts: path
sanitizing, the `Contents` and `USBANLZ` paths, and the artist/album/genre/key
id allocation and playlist entries in `buildPdbLibrary`.

```bash
./build/mixxx-test --gtest_filter='RekordboxExportJob.*'
```

`src/test/rekordboxexportuitest.cpp` covers the dialog's target selection and
the request it builds from the selected crates and playlists.

```bash
./build/mixxx-test --gtest_filter='RekordboxExportUiTest.*'
```

The full `RekordboxFeature` import is not run in the test: it needs a mounted
device tree and a database connection pool. The Kaitai reader and the row
layouts are shared with that path. The job's copy, staging and rename path is
covered by a separate test-only change; it needs a temporary collection and
synthetic audio files.

## Not implemented yet

- Overwriting or merging into an existing device export. The job refuses a
  destination that already has `PIONEER/` or `Contents/`.
- Track colors in `export.pdb`. The color is written as 0 (none); see "The
  export job" above.
- `labels`, `artwork` and the history tables. The tables are present and empty.
  Tracks can carry a label or artwork id, but nothing writes the matching row.
- `PSSI` song structure and `PVDI` vocals. The writer does not invent them;
  the format is not fully specified here.
- `PQT2`, the extended grid. It is not written rather than guessed.
- A real `PVBR` seek table. The writer emits the all-zero 400-entry table.
- `PWVC`, the gain the 3-band waveforms were drawn at. Its values are unknown;
  the sections are written without it.
- Memory cue colors (the eight device rows). Mixxx stores an RGB color, and the
  mapping to the device's color rows is not established here; memory cues are
  written with no color.
- Active-loop detection. A Mixxx saved loop is written as a loop with status 0;
  the export does not mark one as the track's active loop.
- Loop beat quantization. The loop numerator and denominator are written as
  zero, which means "not quantized".
- `Intro`, `Outro` and analysis-only cues.

## Hardware status

Nothing in F09 has been tested in rekordbox or on a CDJ. The bytes are verified
with the Kaitai readers and the format descriptions in this document; loading a
stick on hardware, checking the grids and cues on a player, and importing the
export back into Paramix remain the owner's checks.
