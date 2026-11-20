# Rekordbox device export

Paramix can import rekordbox analysis files but, until this work, could not
write them. This document describes the writer added for F09. It currently
covers **PR 1, the ANLZ analysis writer only**; the PDB database writer, the
export job and the UI follow in separate pull requests.

## What an export contains

A rekordbox USB/SD export keeps three things per track:

```text
PIONEER/
  rekordbox/
    export.pdb              playlists, track metadata, cue database
  USBANLZ/
    <path-derived directory>/
      ANLZ0000.DAT          path, beat grid, previews, memory/A-C cues
      ANLZ0000.EXT          scrolling waveform, D-P and extended cues
      ANLZ0000.2EX          3-band waveforms
Contents/
  <audio files>
```

Only the three ANLZ files are written by this PR. `export.pdb`, the audio copy
and the directory layout belong to the later export job.

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

## Provenance

This writer is a clean-room implementation. The licence question was settled by
excluding the reference implementation, not by relying on its terms:
[chrisle/rbxport](https://github.com/chrisle/rbxport) reports no detected
licence (GitHub `NOASSERTION`), and Paramix is GPL-2.0-only. A repository that
grants no rights cannot be an input, whatever its README or an older note says.

### Permitted inputs

Only these sources informed this code:

1. `lib/rekordbox-metadata/rekordbox_anlz.ksy` and the reader generated from it
   (EPL-1.0). It supplies the `PMAI` framing, the tag-to-body switch, the field
   order of every section, the enums, the per-tag entry widths and the notes on
   which file each waveform tag appears in.
2. `src/library/rekordbox/rekordboxfeature.cpp`, the in-tree importer, for the
   1 ms floor applied to beat and cue times.
3. Public rekordbox documentation published by AlphaTheta/Pioneer DJ.
4. Files produced by the project owner's own rekordbox installation, and values
   measured from them.

### Excluded implementation

No part of chrisle/rbxport is an input: not its Rust sources, tests, constants,
naming, section ordering or measured tables. No revision of that repository was
read during the clean-room audit that produced this statement.

### Spec and reader used

`lib/rekordbox-metadata/rekordbox_anlz.ksy` is the normative reference.
`src/test/rekordboxexportanlztest.cpp` parses the writer's output with the
Kaitai-generated reader built from that file, so the test's oracle is in-tree
and EPL-1.0. The test contains no fixture of external origin.

### Attestation

No rbxport code was translated, transcribed or mechanically adapted for this
writer. The section framing is written directly from `tagged_section`; the
`PPTH`, `PQTZ`, `PCOB`, `PCO2`, `PWAV`/`PWV2` and scrolling-tag headers follow
the `.ksy` field order and widths. Where the `.ksy` does not describe a value,
the writer records an *observed value* and marks it as such in the source: the
`PMAI` header trailer, the scrolling-tag trailer words, the `PWV4`/`PWV5`/
`PWV6`/`PWV7` bit layouts and band scale, the 65-entry hot-cue palette and its
slot defaults, the 150 columns/second rate, the 88-byte `PCP2` size and the
`memory_count` convention. They are recorded as measurements of files that
rekordbox itself wrote, never as values taken from another implementation.

That origin is the one claim this audit cannot verify from inside the
repository: no rekordbox output is stored with the test, so those values are
currently asserted rather than evidenced.

Earlier revisions of this document and of the writer attributed the same values
to rbxport's `rbl-anlz` crate and quoted one of its `PCP2` test vectors. Both
were removed in the clean-room pass: a table of values published in a repository
with no licence grant is not a permitted input either.

The gap this audit cannot close is therefore evidentiary, not textual.
`docs/plans/fixes-2026-11/worklog/F09-pr1.md` carries the per-section evidence
table and the list that must be checked against the owner's own output before
this branch merges.

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

## Not implemented yet

- `export.pdb` (playlists, track metadata, the cue database) — PR 2.
- The export job: worker thread, progress, free-space checks, audio copy,
  path mapping and the MP3 timing offset — PR 3.
- The export dialog and QML action — PR 4.
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

Nothing in this PR has been tested in rekordbox or on a CDJ. The bytes are
verified against the Kaitai reader and the format description in this document;
loading a stick on hardware remains the owner's check.
