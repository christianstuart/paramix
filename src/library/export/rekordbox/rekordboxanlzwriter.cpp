#include "library/export/rekordbox/rekordboxanlzwriter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "track/beats.h"
#include "util/color/rgbcolor.h"
#include "waveform/waveform.h"

namespace {

using mixxx::rekordbox::AnlzCue;
using mixxx::rekordbox::AnlzSection;
using mixxx::rekordbox::BandColumn;

/// Observed value: the 16 bytes rekordbox writes after the 12-byte `PMAI`
/// frame. `rekordbox_anlz.ksy` only skips them (`size: len_header - _io.pos`),
/// so their meaning is unknown and they are recorded, not derived.
constexpr std::array<quint8, 16> kDefaultHeaderExtra = {
        0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};

constexpr std::array<char, 4> kTagPpth = {'P', 'P', 'T', 'H'};
constexpr std::array<char, 4> kTagPvbr = {'P', 'V', 'B', 'R'};
constexpr std::array<char, 4> kTagPqtz = {'P', 'Q', 'T', 'Z'};
constexpr std::array<char, 4> kTagPwav = {'P', 'W', 'A', 'V'};
constexpr std::array<char, 4> kTagPwv2 = {'P', 'W', 'V', '2'};
constexpr std::array<char, 4> kTagPwv3 = {'P', 'W', 'V', '3'};
constexpr std::array<char, 4> kTagPwv4 = {'P', 'W', 'V', '4'};
constexpr std::array<char, 4> kTagPwv5 = {'P', 'W', 'V', '5'};
constexpr std::array<char, 4> kTagPwv6 = {'P', 'W', 'V', '6'};
constexpr std::array<char, 4> kTagPwv7 = {'P', 'W', 'V', '7'};
constexpr std::array<char, 4> kTagPcob = {'P', 'C', 'O', 'B'};
constexpr std::array<char, 4> kTagPco2 = {'P', 'C', 'O', '2'};

void appendBe16(QByteArray* out, quint16 value) {
    out->append(static_cast<char>((value >> 8) & 0xff));
    out->append(static_cast<char>(value & 0xff));
}

void appendBe32(QByteArray* out, quint32 value) {
    out->append(static_cast<char>((value >> 24) & 0xff));
    out->append(static_cast<char>((value >> 16) & 0xff));
    out->append(static_cast<char>((value >> 8) & 0xff));
    out->append(static_cast<char>(value & 0xff));
}

void putBe16(QByteArray* out, int at, quint16 value) {
    (*out)[at] = static_cast<char>((value >> 8) & 0xff);
    (*out)[at + 1] = static_cast<char>(value & 0xff);
}

void putBe32(QByteArray* out, int at, quint32 value) {
    (*out)[at] = static_cast<char>((value >> 24) & 0xff);
    (*out)[at + 1] = static_cast<char>((value >> 16) & 0xff);
    (*out)[at + 2] = static_cast<char>((value >> 8) & 0xff);
    (*out)[at + 3] = static_cast<char>(value & 0xff);
}

AnlzSection makeSection(
        const std::array<char, 4>& tag, QByteArray header, QByteArray payload) {
    AnlzSection section;
    section.tag = tag;
    section.header = std::move(header);
    section.payload = std::move(payload);
    return section;
}

quint32 millisecondsOf(
        mixxx::audio::FramePos position, mixxx::audio::SampleRate sampleRate) {
    if (!position.isValid() || !sampleRate.isValid()) {
        return 0;
    }
    const double milliseconds = position.value() * 1000.0 / sampleRate.toDouble();
    if (!(milliseconds > 0.0)) {
        // The importer clamps cue and beat times below 1 ms.
        return 1;
    }
    return static_cast<quint32>(std::min(
            std::round(milliseconds),
            static_cast<double>(std::numeric_limits<quint32>::max())));
}

/// Observed value: the scale rekordbox's own `PWV6` uses for a 0..=255 band.
/// The `.ksy` fixes the three-byte entry width but not the band range.
constexpr quint32 kBandsFullScale = 44;

quint32 band(quint8 value) {
    return static_cast<quint32>(value) * kBandsFullScale / 255;
}

quint32 bandHeight(const BandColumn& column, quint32 ceiling) {
    const quint32 loudest = std::max({band(column.low), band(column.mid), band(column.high)});
    return std::min(loudest * ceiling / kBandsFullScale, ceiling);
}

/// A scrolling detail column's height: rekordbox's squared sample-peak curve.
quint32 detailHeight(const BandColumn& column) {
    constexpr quint32 kFullScale = 255 * 255;
    constexpr quint32 kDetailCeiling = 31;
    const quint32 peak = static_cast<quint32>(column.peak);
    return std::min((kDetailCeiling * peak * peak + kFullScale / 2) / kFullScale,
            kDetailCeiling);
}

quint32 monoByte(const BandColumn& column, quint32 height) {
    const quint32 total = static_cast<quint32>(column.low) +
            static_cast<quint32>(column.mid) +
            static_cast<quint32>(column.high);
    const quint32 whiteness = total == 0
            ? 0
            : (static_cast<quint32>(column.high) * 7 + total / 2) / total;
    return (std::min(whiteness, 7u) << 5) | std::min(height, 31u);
}

/// Observed value: the 65-entry palette a hot cue's color index addresses.
/// These are the RGB triples rekordbox itself writes into `PCO2` entries, not
/// a value described by the `.ksy`; see the open attestation gap for the
/// palette in `docs/rekordbox-export.md`.
constexpr std::array<std::array<quint8, 3>, 65> kDevicePalette = {{{{0x00, 0x00, 0x00}}, {{0x00, 0x00, 0xff}}, {{0x00, 0x1c, 0xff}}, {{0x00, 0x38, 0xff}}, {{0x00, 0x54, 0xff}}, {{0x00, 0x70, 0xff}}, {{0x00, 0x8c, 0xff}}, {{0x00, 0xa8, 0xff}}, {{0x00, 0xc4, 0xff}}, {{0x00, 0xe0, 0xff}}, {{0x00, 0xff, 0xff}}, {{0x00, 0xff, 0xe8}}, {{0x00, 0xff, 0xd1}}, {{0x00, 0xff, 0xba}}, {{0x00, 0xff, 0xa3}}, {{0x00, 0xff, 0x8c}}, {{0x00, 0xff, 0x75}}, {{0x00, 0xff, 0x5e}}, {{0x00, 0xff, 0x47}}, {{0x00, 0xff, 0x30}}, {{0x00, 0xff, 0x1a}}, {{0x00, 0xff, 0x00}}, {{0x1a, 0xff, 0x00}}, {{0x33, 0xff, 0x00}}, {{0x4d, 0xff, 0x00}}, {{0x66, 0xff, 0x00}}, {{0x80, 0xff, 0x00}}, {{0x99, 0xff, 0x00}}, {{0xb3, 0xff, 0x00}}, {{0xcc, 0xff, 0x00}}, {{0xe6, 0xff, 0x00}}, {{0xff, 0xff, 0x00}}, {{0xff, 0xe8, 0x00}}, {{0xff, 0xd1, 0x00}}, {{0xff, 0xba, 0x00}}, {{0xff, 0xa3, 0x00}}, {{0xff, 0x8c, 0x00}}, {{0xff, 0x75, 0x00}}, {{0xff, 0x5e, 0x00}}, {{0xff, 0x47, 0x00}}, {{0xff, 0x30, 0x00}}, {{0xff, 0x1a, 0x00}}, {{0xff, 0x00, 0x00}}, {{0xff, 0x00, 0x17}}, {{0xff, 0x00, 0x2e}}, {{0xff, 0x00, 0x45}}, {{0xff, 0x00, 0x5c}}, {{0xff, 0x00, 0x73}}, {{0xff, 0x00, 0x8a}}, {{0xff, 0x00, 0xa1}}, {{0xff, 0x00, 0xb8}}, {{0xff, 0x00, 0xcf}}, {{0xff, 0x00, 0xe6}}, {{0xff, 0x00, 0xff}}, {{0xe6, 0x00, 0xff}}, {{0xcc, 0x00, 0xff}}, {{0xb3, 0x00, 0xff}}, {{0x99, 0x00, 0xff}}, {{0x80, 0x00, 0xff}}, {{0x66, 0x00, 0xff}}, {{0x4d, 0x00, 0xff}}, {{0x33, 0x00, 0xff}}, {{0x1a, 0x00, 0xff}}, {{0x00, 0x00, 0x00}}, {{0xff, 0xff, 0xff}}}};

QByteArray utf16BeWithNull(const QString& text) {
    QByteArray out;
    out.reserve((text.size() + 1) * 2);
    for (int i = 0; i < text.size(); ++i) {
        appendBe16(&out, static_cast<quint16>(text.at(i).unicode()));
    }
    appendBe16(&out, 0);
    return out;
}

QByteArray cueEntry(const AnlzCue& cue, bool modern, int index, int count) {
    const QByteArray comment = cue.comment.isEmpty()
            ? QByteArray()
            : utf16BeWithNull(cue.comment);
    const int commentSize = static_cast<int>(comment.size());

    // `cue_entry` (`PCPT`) is 56 bytes per `rekordbox_anlz.ksy`. A
    // `cue_extended_entry` (`PCP2`) is 88 bytes plus the comment when it has
    // no trailing padding; 88 is the size observed in rekordbox's own output.
    const int length = modern ? 88 + commentSize : 56;
    QByteArray entry(length, '\0');
    entry.replace(0, 4, modern ? "PCP2" : "PCPT");
    putBe32(&entry, 4, modern ? 16 : 28);
    putBe32(&entry, 8, static_cast<quint32>(length));
    putBe32(&entry, 12, cue.slot);

    const int at = modern ? 16 : 28;
    entry[at] = static_cast<char>(cue.isLoop ? 2 : 1);
    entry[at + 1] = 0;
    // The three-byte gap after the type is 1000 in both entry forms, as the
    // `.ksy` records ("seems to always be 1000").
    entry[at + 2] = 3;
    entry[at + 3] = static_cast<char>(232);
    putBe32(&entry, at + 4, cue.timeMs);
    putBe32(&entry, at + 8, cue.isLoop ? cue.loopEndMs : 0xffffffffu);

    if (modern) {
        entry[28] = static_cast<char>(cue.memoryColorRow);
        // Observed value: the first byte of the seven-byte gap after
        // `color_id` is 1 in rekordbox's own `PCP2` entries.
        entry[29] = 1;
        putBe16(&entry, 36, cue.loopNumerator);
        putBe16(&entry, 38, cue.loopDenominator);
        putBe32(&entry, 40, static_cast<quint32>(commentSize));
        if (commentSize > 0) {
            std::copy(comment.cbegin(), comment.cend(), entry.begin() + 44);
        }
        entry[44 + commentSize] = static_cast<char>(cue.colorCode);
        const auto rgb = mixxx::rekordbox::deviceColor(cue.colorCode, cue.slot);
        entry[45 + commentSize] = static_cast<char>(rgb[0]);
        entry[46 + commentSize] = static_cast<char>(rgb[1]);
        entry[47 + commentSize] = static_cast<char>(rgb[2]);
    } else {
        putBe32(&entry, 16, cue.activeLoop ? 4 : 0);
        putBe32(&entry, 20, 0x10000);
        putBe16(&entry, 24, cue.isMemory() || index == 0 ? 0xffff : static_cast<quint16>(index - 1));
        putBe16(&entry, 26, cue.isMemory() || index + 1 == count ? 0xffff : static_cast<quint16>(index + 1));
    }
    return entry;
}

AnlzSection cueListSection(
        const QVector<const AnlzCue*>& list, quint32 listType, bool modern) {
    const int count = std::min<int>(static_cast<int>(list.size()),
            std::numeric_limits<quint16>::max());
    QByteArray header(modern ? 8 : 12, '\0');
    putBe32(&header, 0, listType);
    putBe16(&header, modern ? 4 : 6, static_cast<quint16>(count));
    if (!modern) {
        // `cue_tag.memory_count`, which the `.ksy` leaves unexplained: the
        // last index of a populated memory list, 0xffffffff for a hot list or
        // an empty list, as observed in rekordbox's own output.
        putBe32(&header,
                8,
                listType != 0 || count == 0
                        ? 0xffffffffu
                        : static_cast<quint32>(count - 1));
    }

    QByteArray payload;
    for (int i = 0; i < count; ++i) {
        payload.append(cueEntry(*list.at(i), modern, i, count));
    }
    return makeSection(modern ? kTagPco2 : kTagPcob, header, payload);
}

} // namespace

namespace mixxx {
namespace rekordbox {

QByteArray AnlzSection::toByteArray() const {
    QByteArray out;
    out.reserve(static_cast<int>(lenTag()));
    out.append(tag.data(), 4);
    appendBe32(&out, lenHeader());
    appendBe32(&out, lenTag());
    out.append(header);
    out.append(payload);
    return out;
}

QByteArray AnlzWriter::render(const QVector<AnlzSection>& sections) {
    quint32 bodyLength = 0;
    for (const AnlzSection& section : sections) {
        bodyLength += section.lenTag();
    }
    const quint32 lenHeader = 12 + static_cast<quint32>(kDefaultHeaderExtra.size());

    QByteArray out;
    out.reserve(static_cast<int>(lenHeader + bodyLength));
    out.append("PMAI", 4);
    appendBe32(&out, lenHeader);
    appendBe32(&out, lenHeader + bodyLength);
    for (quint8 byte : kDefaultHeaderExtra) {
        out.append(static_cast<char>(byte));
    }

    for (const AnlzSection& section : sections) {
        out.append(section.tag.data(), 4);
        appendBe32(&out, section.lenHeader());
        appendBe32(&out, section.lenTag());
        out.append(section.header);
        out.append(section.payload);
    }
    return out;
}

AnlzSection AnlzWriter::pathSection(const QString& path) {
    // PPTH, `path_tag`: u4 byte length, then UTF-16BE with a trailing NUL.
    const QByteArray text = utf16BeWithNull(path);
    QByteArray header;
    appendBe32(&header, static_cast<quint32>(text.size()));
    return makeSection(kTagPpth, header, text);
}

AnlzSection AnlzWriter::beatGridSection(const QVector<AnlzBeat>& beats) {
    // PQTZ, `beat_grid_tag`: two u4 words, then num_beats. The `.ksy` records
    // the second word as always 0x80000.
    QByteArray header;
    appendBe32(&header, 0);
    appendBe32(&header, 0x00080000);
    appendBe32(&header, static_cast<quint32>(beats.size()));

    QByteArray payload;
    payload.reserve(beats.size() * 8);
    for (const AnlzBeat& beat : beats) {
        appendBe16(&payload, beat.beatNumber);
        appendBe16(&payload, beat.tempoX100);
        appendBe32(&payload, beat.timeMs);
    }
    return makeSection(kTagPqtz, header, payload);
}

AnlzSection AnlzWriter::waveformPreviewSection(
        const std::array<char, 4>& tag, const QByteArray& data) {
    // PWAV/PWV2, `wave_preview_tag`: len_data, then the 0x10000 word the `.ksy`
    // records as a constant.
    QByteArray header;
    appendBe32(&header, static_cast<quint32>(data.size()));
    appendBe32(&header, 0x00010000);
    return makeSection(tag, header, data);
}

AnlzSection AnlzWriter::waveformScrollSection(
        const std::array<char, 4>& tag, quint32 stride, const QByteArray& data) {
    const quint32 entries = stride == 0
            ? 0
            : static_cast<quint32>(data.size()) / stride;
    QByteArray header;
    appendBe32(&header, stride);
    appendBe32(&header, entries);
    // `PWV6` is `wave_3band_preview_tag` and has only two header words. The
    // scrolling tags are `wave_scroll_tag`, `wave_color_preview_tag`,
    // `wave_color_scroll_tag` and `wave_3band_scroll_tag`, each of which the
    // `.ksy` gives one further constant word. Its meaning is unknown, so the
    // values observed in rekordbox's own files are recorded here.
    if (tag != kTagPwv6) {
        quint32 trailer = 0x00960000;
        if (tag == kTagPwv5) {
            trailer = 0x00960305;
        } else if (tag == kTagPwv4) {
            trailer = 0;
        }
        appendBe32(&header, trailer);
    }
    return makeSection(tag, header, data);
}

AnlzSection AnlzWriter::vbrTableZeroSection() {
    // PVBR, `vbr_tag`: one unused u4, then a 400-entry u4 index. An all-zero
    // table is the form used for a track whose bitrate is not indexed.
    QByteArray header;
    appendBe32(&header, 0);
    QByteArray payload(400 * 4, '\0');
    return makeSection(kTagPvbr, header, payload);
}

QVector<AnlzSection> AnlzWriter::cueSections(
        const QVector<AnlzCue>& cues, bool extended) {
    QVector<AnlzSection> out;
    for (bool modern : {false, true}) {
        if (modern && !extended) {
            continue;
        }
        for (bool hot : {true, false}) {
            QVector<const AnlzCue*> list;
            for (const AnlzCue& cue : cues) {
                if (cue.isMemory() == hot) {
                    continue;
                }
                if (!modern) {
                    // A `.DAT` carries A-C; an `.EXT`'s legacy list carries
                    // D-P, because A-C are already in the `.DAT`.
                    if (extended && cue.slot <= 3) {
                        continue;
                    }
                    if (!extended && cue.slot > 3) {
                        continue;
                    }
                }
                list.append(&cue);
            }
            out.append(cueListSection(list, hot ? 1 : 0, modern));
        }
    }
    return out;
}

QVector<BandColumn> AnlzWriter::resample(
        const QVector<BandColumn>& columns, int count) {
    if (count <= 0) {
        return {};
    }
    if (columns.isEmpty()) {
        return QVector<BandColumn>(count);
    }
    QVector<BandColumn> out;
    out.reserve(count);
    const int size = columns.size();
    for (int bucket = 0; bucket < count; ++bucket) {
        const int start = static_cast<int>(
                static_cast<qint64>(bucket) * size / count);
        int end = static_cast<int>(
                static_cast<qint64>(bucket + 1) * size / count);
        end = std::min(std::max(end, start + 1), size);
        BandColumn loudest;
        for (int i = start; i < end; ++i) {
            const BandColumn& column = columns.at(i);
            loudest.low = std::max(loudest.low, column.low);
            loudest.mid = std::max(loudest.mid, column.mid);
            loudest.high = std::max(loudest.high, column.high);
            loudest.peak = std::max(loudest.peak, column.peak);
        }
        out.append(loudest);
    }
    return out;
}

QByteArray AnlzWriter::encodePwav(const QVector<BandColumn>& columns) {
    QByteArray out;
    out.reserve(kPreviewColumns);
    for (const BandColumn& column : resample(columns, kPreviewColumns)) {
        quint8 byte = static_cast<quint8>(monoByte(column, bandHeight(column, 25)));
        // Players reject a preview with a zero height as unfinished.
        byte = static_cast<quint8>((byte & 0xe0) |
                std::max<quint8>(static_cast<quint8>(byte & 0x1f),
                        static_cast<quint8>(2)));
        out.append(static_cast<char>(byte));
    }
    return out;
}

QByteArray AnlzWriter::encodePwv2(const QVector<BandColumn>& columns) {
    QByteArray out;
    out.reserve(kTinyColumns);
    for (const BandColumn& column : resample(columns, kTinyColumns)) {
        const quint32 height = bandHeight(column, 15);
        out.append(static_cast<char>(std::clamp<quint32>(height, 1, 15)));
    }
    return out;
}

QByteArray AnlzWriter::encodePwv3(const QVector<BandColumn>& columns) {
    QByteArray out;
    out.reserve(columns.size());
    for (const BandColumn& column : columns) {
        out.append(static_cast<char>(monoByte(column, detailHeight(column))));
    }
    return out;
}

QByteArray AnlzWriter::encodePwv4(const QVector<BandColumn>& columns) {
    // `wave_color_preview_tag` fixes six bytes per entry: peak, two unused
    // bytes, then the three bands.
    const auto weigh = [](quint8 value, quint32 numerator, quint32 denominator) {
        return static_cast<quint8>(
                static_cast<quint32>(value >> 1) * numerator / denominator);
    };
    QByteArray out;
    out.reserve(kOverviewColumns * 6);
    for (const BandColumn& column : resample(columns, kOverviewColumns)) {
        out.append(static_cast<char>(column.peak >> 1));
        out.append('\0');
        out.append('\0');
        out.append(static_cast<char>(column.low >> 1));
        // The mid and high channels are weighted down; the ratios are what
        // the players draw.
        out.append(static_cast<char>(weigh(column.mid, 4, 5)));
        out.append(static_cast<char>(weigh(column.high, 13, 20)));
    }
    return out;
}

QByteArray AnlzWriter::encodePwv5(const QVector<BandColumn>& columns) {
    // `wave_color_scroll_tag` fixes two bytes per entry; the observed packing
    // is rrrgggbbbhhhhh00.
    QByteArray out;
    out.reserve(columns.size() * 2);
    for (const BandColumn& column : columns) {
        const quint32 low = 2 * static_cast<quint32>(column.low);
        const quint32 mid = 2 * static_cast<quint32>(column.mid);
        const quint32 high = 3 * static_cast<quint32>(column.high);
        const quint32 strongest = std::max({low, mid, high});
        quint32 red = 0;
        quint32 green = 0;
        quint32 blue = 0;
        if (strongest != 0) {
            const auto share = [strongest](quint32 value, quint32 full) {
                return (2 * full * value + strongest) / (2 * strongest);
            };
            red = share(low, 7);
            green = share(mid, 5);
            blue = share(high, 7);
        }
        const quint32 word = (red << 13) |
                (green << 10) |
                (blue << 7) |
                (detailHeight(column) << 2);
        appendBe16(&out, static_cast<quint16>(word));
    }
    return out;
}

QByteArray AnlzWriter::encodePwv6(const QVector<BandColumn>& columns) {
    QByteArray out;
    out.reserve(kOverviewColumns * 3);
    for (const BandColumn& column : resample(columns, kOverviewColumns)) {
        out.append(static_cast<char>(band(column.low)));
        out.append(static_cast<char>(band(column.mid)));
        out.append(static_cast<char>(band(column.high)));
    }
    return out;
}

QByteArray AnlzWriter::encodePwv7(const QVector<BandColumn>& columns) {
    QByteArray out;
    out.reserve(columns.size() * 3);
    for (const BandColumn& column : columns) {
        out.append(static_cast<char>(column.low >> 1));
        out.append(static_cast<char>(column.mid >> 1));
        out.append(static_cast<char>(column.high >> 1));
    }
    return out;
}

AnalysisFiles AnlzWriter::author(const QString& path,
        const QVector<AnlzBeat>& beats,
        const QVector<AnlzCue>& cues,
        const QVector<BandColumn>& columns) {
    AnalysisFiles files;

    QVector<AnlzSection> dat;
    dat.append(pathSection(path));
    dat.append(vbrTableZeroSection());
    dat.append(beatGridSection(beats));
    dat.append(waveformPreviewSection(kTagPwav, encodePwav(columns)));
    dat.append(waveformPreviewSection(kTagPwv2, encodePwv2(columns)));
    for (const AnlzSection& section : cueSections(cues, false)) {
        dat.append(section);
    }
    files.dat = render(dat);

    QVector<AnlzSection> ext;
    ext.append(pathSection(path));
    ext.append(waveformScrollSection(kTagPwv3, 1, encodePwv3(columns)));
    for (const AnlzSection& section : cueSections(cues, true)) {
        ext.append(section);
    }
    ext.append(waveformScrollSection(kTagPwv5, 2, encodePwv5(columns)));
    ext.append(waveformScrollSection(kTagPwv4, 6, encodePwv4(columns)));
    files.ext = render(ext);

    QVector<AnlzSection> twoEx;
    twoEx.append(pathSection(path));
    twoEx.append(waveformScrollSection(kTagPwv7, 3, encodePwv7(columns)));
    twoEx.append(waveformScrollSection(kTagPwv6, 3, encodePwv6(columns)));
    files.twoEx = render(twoEx);

    return files;
}

AnalysisFiles AnlzWriter::author(const QString& path,
        const Beats& beats,
        const QList<CuePointer>& cues,
        const Waveform& waveform,
        audio::FramePos endPosition,
        int columnsPerSecond) {
    const auto sampleRate = beats.getSampleRate();
    const QVector<BandColumn> summary = columnsFromWaveform(waveform);
    const quint32 durationMs = endPosition.isValid() && sampleRate.isValid()
            ? static_cast<quint32>(std::min(
                      std::round(endPosition.value() * 1000.0 / sampleRate.toDouble()),
                      static_cast<double>(std::numeric_limits<quint32>::max())))
            : 0;
    return author(path,
            beatsFromBeats(beats, endPosition),
            cuesFromCues(cues, sampleRate),
            columnsForDuration(summary, durationMs, columnsPerSecond));
}

QVector<AnlzBeat> beatsFromBeats(
        const Beats& beats, audio::FramePos endPosition) {
    QVector<AnlzBeat> out;
    const auto sampleRate = beats.getSampleRate();
    if (!sampleRate.isValid() || !endPosition.isValid()) {
        return out;
    }

    // `cfirstmarker()` and `clastmarker()` collapse to the same position for a
    // constant-tempo grid, so walk the grid beat by beat instead.
    constexpr int kMaxBeats = 1 << 20;
    QVector<audio::FramePos> positions;
    audio::FramePos position = beats.findNextBeat(audio::kStartFramePos);
    while (position.isValid() &&
            position <= endPosition &&
            positions.size() < kMaxBeats) {
        positions.append(position);
        position = beats.findNextBeat(audio::FramePos(position.value() + 1.0));
    }
    if (positions.isEmpty()) {
        return out;
    }

    const DownbeatInfo& downbeat = beats.getDownbeatInfo();
    const int beatsPerBar = downbeat.valid ? downbeat.beatsPerBar : 4;
    const int phase = downbeat.valid ? downbeat.phase : 0;

    out.reserve(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        double intervalFrames = 0.0;
        if (i + 1 < positions.size()) {
            intervalFrames = positions.at(i + 1).value() - positions.at(i).value();
        } else if (i > 0) {
            intervalFrames = positions.at(i).value() - positions.at(i - 1).value();
        }

        AnlzBeat beat;
        beat.timeMs = millisecondsOf(positions.at(i), sampleRate);
        if (intervalFrames > 0.0) {
            const double bpmX100 = 6000.0 * sampleRate.toDouble() / intervalFrames;
            beat.tempoX100 = static_cast<quint16>(std::clamp(
                    std::round(bpmX100),
                    0.0,
                    static_cast<double>(std::numeric_limits<quint16>::max())));
        }
        const int number = ((i - phase) % beatsPerBar + beatsPerBar) % beatsPerBar + 1;
        beat.beatNumber = static_cast<quint16>(number);
        out.append(beat);
    }
    return out;
}

QVector<AnlzCue> cuesFromCues(
        const QList<CuePointer>& cues, audio::SampleRate sampleRate) {
    QVector<AnlzCue> out;
    if (!sampleRate.isValid()) {
        return out;
    }
    for (const CuePointer& cue : cues) {
        if (!cue) {
            continue;
        }
        const auto start = cue->getPosition();
        if (!start.isValid()) {
            continue;
        }

        AnlzCue anlzCue;
        anlzCue.timeMs = millisecondsOf(start, sampleRate);

        switch (cue->getType()) {
        case CueType::HotCue: {
            const int hotCue = cue->getHotCue();
            if (hotCue < kFirstHotCueIndex) {
                continue;
            }
            anlzCue.slot = static_cast<quint32>(hotCue) + 1;
            break;
        }
        case CueType::Loop: {
            const int hotCue = cue->getHotCue();
            anlzCue.slot = hotCue >= kFirstHotCueIndex
                    ? static_cast<quint32>(hotCue) + 1
                    : 0;
            break;
        }
        case CueType::MainCue:
            anlzCue.slot = 0;
            break;
        default:
            // Intro, Outro and analysis-only cues are not device cues.
            continue;
        }

        const auto end = cue->getEndPosition();
        if (end.isValid()) {
            anlzCue.isLoop = true;
            anlzCue.loopEndMs = millisecondsOf(end, sampleRate);
        }

        anlzCue.comment = cue->getLabel();
        if (!anlzCue.isMemory()) {
            const RgbColor color = cue->getColor();
            if (color != RgbColor(0)) {
                anlzCue.colorCode = deviceColorCode(color);
            }
        }
        out.append(anlzCue);
    }
    return out;
}

QVector<BandColumn> columnsFromWaveform(const Waveform& waveform) {
    QVector<BandColumn> out;
    const int count = waveform.getDataSize();
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const WaveformData& data = waveform.get(i);
        BandColumn column;
        column.low = data.filtered.low;
        column.mid = data.filtered.mid;
        column.high = data.filtered.high;
        column.peak = data.filtered.all;
        out.append(column);
    }
    return out;
}

QVector<BandColumn> columnsForDuration(const QVector<BandColumn>& columns,
        quint32 durationMs,
        int columnsPerSecond) {
    if (columns.isEmpty() || durationMs == 0 || columnsPerSecond <= 0) {
        return {};
    }
    const qint64 count = static_cast<qint64>(durationMs) * columnsPerSecond / 1000;
    return AnlzWriter::resample(columns, static_cast<int>(std::max<qint64>(count, 0)));
}

quint8 deviceColorCode(RgbColor color) {
    const quint32 rgb = color;
    const int red = static_cast<int>((rgb >> 16) & 0xff);
    const int green = static_cast<int>((rgb >> 8) & 0xff);
    const int blue = static_cast<int>(rgb & 0xff);

    int best = 0;
    qint64 bestDistance = std::numeric_limits<qint64>::max();
    for (int i = 0; i < static_cast<int>(kDevicePalette.size()); ++i) {
        const auto& entry = kDevicePalette.at(i);
        const qint64 dr = red - entry[0];
        const qint64 dg = green - entry[1];
        const qint64 db = blue - entry[2];
        const qint64 distance = dr * dr + dg * dg + db * db;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return static_cast<quint8>(best);
}

std::array<quint8, 3> deviceColor(quint8 code, quint32 slot) {
    // Observed slot defaults: a hot cue with no explicit colour falls back to
    // the palette entry rekordbox shows for that slot.
    quint32 index = code;
    if (index == 0) {
        switch (slot) {
        case 1:
        case 9:
            index = 43;
            break;
        case 2:
        case 10:
            index = 8;
            break;
        case 3:
        case 11:
            index = 23;
            break;
        case 4:
        case 12:
            index = 60;
            break;
        default:
            index = 0;
            break;
        }
    }
    if (index >= kDevicePalette.size()) {
        return {0, 0, 0};
    }
    return kDevicePalette.at(index);
}

} // namespace rekordbox
} // namespace mixxx
