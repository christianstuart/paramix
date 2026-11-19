#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include <array>
#include <cstdint>

#include "audio/frame.h"
#include "audio/types.h"
#include "track/cue.h"

class Waveform;

namespace mixxx {

class Beats;

namespace rekordbox {

/// One column of an analyzed waveform. Every value is 0..=255.
///
/// `peak` is the column's overall sample peak and is only used where the
/// format has a separate peak field (`PWV4`); the five-bit heights of the
/// other sections come from the three bands.
struct BandColumn {
    quint8 low = 0;
    quint8 mid = 0;
    quint8 high = 0;
    quint8 peak = 0;

    bool operator==(const BandColumn& other) const {
        return low == other.low &&
                mid == other.mid &&
                high == other.high &&
                peak == other.peak;
    }
};

/// One `PQTZ` beat entry.
struct AnlzBeat {
    /// Beat position within the bar, 1 is the downbeat.
    quint16 beatNumber = 1;
    /// Beats per minute multiplied by 100.
    quint16 tempoX100 = 0;
    /// Milliseconds from the start of the track at 100% pitch.
    quint32 timeMs = 0;

    bool operator==(const AnlzBeat& other) const {
        return beatNumber == other.beatNumber &&
                tempoX100 == other.tempoX100 &&
                timeMs == other.timeMs;
    }
};

/// One device cue. A cue is either a memory cue (`slot == 0`) or a hot cue
/// (`slot` 1..16 for A..P), and may be a loop.
struct AnlzCue {
    quint32 slot = 0;
    quint32 timeMs = 0;
    /// Set when `isLoop` is true; the device loops back from here to `timeMs`.
    quint32 loopEndMs = 0;
    bool isLoop = false;
    /// The memory loop that a player treats as the track's active loop.
    bool activeLoop = false;
    /// Memory cue color row, 1..8, or 0 for none.
    quint8 memoryColorRow = 0;
    /// Hot cue color index into the device palette, or 0 for the slot default.
    quint8 colorCode = 0;
    QString comment;
    quint16 loopNumerator = 0;
    quint16 loopDenominator = 0;

    bool isMemory() const {
        return slot == 0;
    }
};

/// A section with its framing kept separate from its tag-specific header and
/// payload. `lenHeader()` and `lenTag()` are what is written to the file.
struct AnlzSection {
    std::array<char, 4> tag = {'\0', '\0', '\0', '\0'};
    QByteArray header;
    QByteArray payload;

    quint32 lenHeader() const {
        return 12 + static_cast<quint32>(header.size());
    }
    quint32 lenTag() const {
        return lenHeader() + static_cast<quint32>(payload.size());
    }

    /// The complete section: framing, tag-specific header and payload.
    QByteArray toByteArray() const;
};

/// The three files one analysis produces, written beside each other.
struct AnalysisFiles {
    QByteArray dat;
    QByteArray ext;
    QByteArray twoEx;
};

/// Writer for rekordbox ANLZ analysis files (`.DAT`, `.EXT`, `.2EX`).
///
/// The format and the section layout are taken from chrisle/rbxport's
/// `rbl-anlz` crate at commit d0ff75b378ec85be891d9f3067ada1e82bad4cae and
/// from `lib/rekordbox-metadata/rekordbox_anlz.ksy`.
class AnlzWriter {
  public:
    /// Columns in the `PWAV` preview.
    static constexpr int kPreviewColumns = 400;
    /// Columns in the `PWV2` tiny preview.
    static constexpr int kTinyColumns = 100;
    /// Columns in the `PWV4` and `PWV6` overviews.
    static constexpr int kOverviewColumns = 1200;

    /// Renders a whole file: the `PMAI` header, then every section.
    static QByteArray render(const QVector<AnlzSection>& sections);

    static AnlzSection pathSection(const QString& path);
    static AnlzSection beatGridSection(const QVector<AnlzBeat>& beats);
    static AnlzSection waveformPreviewSection(
            const std::array<char, 4>& tag, const QByteArray& data);
    static AnlzSection waveformScrollSection(
            const std::array<char, 4>& tag, quint32 stride, const QByteArray& data);
    static AnlzSection vbrTableZeroSection();

    /// Builds the cue list sections. A `.DAT` carries memory cues and hot
    /// cues A-C as `PCOB`; an `.EXT` carries hot cues D-P as `PCOB` plus the
    /// complete hot and memory lists as `PCO2`.
    static QVector<AnlzSection> cueSections(
            const QVector<AnlzCue>& cues, bool extended);

    /// Authors the three files for one track.
    static AnalysisFiles author(const QString& path,
            const QVector<AnlzBeat>& beats,
            const QVector<AnlzCue>& cues,
            const QVector<BandColumn>& columns);

    /// End-to-end convenience for the objects a track owns.
    static AnalysisFiles author(const QString& path,
            const Beats& beats,
            const QList<CuePointer>& cues,
            const Waveform& waveform,
            audio::FramePos endPosition,
            int columnsPerSecond = 150);

    // Waveform encoders. Each returns the payload bytes for its section.
    static QVector<BandColumn> resample(const QVector<BandColumn>& columns, int count);
    static QByteArray encodePwav(const QVector<BandColumn>& columns);
    static QByteArray encodePwv2(const QVector<BandColumn>& columns);
    static QByteArray encodePwv3(const QVector<BandColumn>& columns);
    static QByteArray encodePwv4(const QVector<BandColumn>& columns);
    static QByteArray encodePwv5(const QVector<BandColumn>& columns);
    static QByteArray encodePwv6(const QVector<BandColumn>& columns);
    static QByteArray encodePwv7(const QVector<BandColumn>& columns);
};

/// Converts a Mixxx beat grid to the beats `PQTZ` stores, starting at the
/// first beat and stopping at `endPosition` (the track duration).
QVector<AnlzBeat> beatsFromBeats(
        const Beats& beats, audio::FramePos endPosition);

/// Converts Mixxx cues to device cues. `HotCue` becomes a hot cue, a `Loop`
/// becomes a loop (hot when it has a hot cue index, memory otherwise) and
/// `MainCue` becomes a memory cue. `Intro`, `Outro` and analysis-only cues
/// are not exported.
QVector<AnlzCue> cuesFromCues(
        const QList<CuePointer>& cues, audio::SampleRate sampleRate);

/// Converts a stored waveform summary to band columns.
QVector<BandColumn> columnsFromWaveform(const Waveform& waveform);

/// Resamples columns to `columnsPerSecond * durationMs / 1000`, the rate of
/// the scrolling waveform sections.
QVector<BandColumn> columnsForDuration(const QVector<BandColumn>& columns,
        quint32 durationMs,
        int columnsPerSecond = 150);

/// The device palette index closest to `color`. Returns 0 for black, which
/// the format treats as "use the slot's default color".
quint8 deviceColorCode(RgbColor color);

/// The RGB a player is sent for a palette index; 0 gets the slot default.
std::array<quint8, 3> deviceColor(quint8 code, quint32 slot);

} // namespace rekordbox
} // namespace mixxx
