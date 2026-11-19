#include <gtest/gtest.h>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
#include <array>
#include <memory>
#include <string>
#include <vector>

// The Kaitai-generated reader has no installed include directory for the
// test target; the test links the same library the importer uses.
#include <rekordbox_anlz.h>

#include "library/export/rekordbox/rekordboxanlzwriter.h"
#include "track/beats.h"
#include "track/cue.h"
#include "waveform/waveform.h"

namespace mixxx::rekordbox {
namespace {

struct ParsedAnlz {
    std::unique_ptr<kaitai::kstream> stream;
    std::unique_ptr<rekordbox_anlz_t> file;

    explicit ParsedAnlz(const QByteArray& bytes)
            : stream(std::make_unique<kaitai::kstream>(
                      std::string(bytes.constData(), static_cast<size_t>(bytes.size())))),
              file(std::make_unique<rekordbox_anlz_t>(stream.get())) {
    }
};

std::string tagName(rekordbox_anlz_t::section_tags_t tag) {
    switch (tag) {
    case rekordbox_anlz_t::SECTION_TAGS_CUES:
        return "PCOB";
    case rekordbox_anlz_t::SECTION_TAGS_CUES_2:
        return "PCO2";
    case rekordbox_anlz_t::SECTION_TAGS_PATH:
        return "PPTH";
    case rekordbox_anlz_t::SECTION_TAGS_VBR:
        return "PVBR";
    case rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID:
        return "PQTZ";
    case rekordbox_anlz_t::SECTION_TAGS_WAVE_PREVIEW:
        return "PWAV";
    case rekordbox_anlz_t::SECTION_TAGS_WAVE_TINY:
        return "PWV2";
    case rekordbox_anlz_t::SECTION_TAGS_WAVE_SCROLL:
        return "PWV3";
    case rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_PREVIEW:
        return "PWV4";
    case rekordbox_anlz_t::SECTION_TAGS_WAVE_COLOR_SCROLL:
        return "PWV5";
    case rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_PREVIEW:
        return "PWV6";
    case rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_SCROLL:
        return "PWV7";
    case rekordbox_anlz_t::SECTION_TAGS_SONG_STRUCTURE:
        return "PSSI";
    default:
        return "????";
    }
}

std::vector<std::string> tagsOf(const rekordbox_anlz_t& file) {
    std::vector<std::string> tags;
    for (const auto& section : *file.sections()) {
        tags.push_back(tagName(section->fourcc()));
    }
    return tags;
}

rekordbox_anlz_t::tagged_section_t* findSection(
        rekordbox_anlz_t* file,
        rekordbox_anlz_t::section_tags_t tag,
        int index = 0) {
    for (const auto& section : *file->sections()) {
        if (section->fourcc() == tag) {
            if (index == 0) {
                return section.get();
            }
            --index;
        }
    }
    return nullptr;
}

/// The Kaitai reader is built with KS_STR_ENCODING_NONE, so a `str` field is
/// the raw UTF-16BE bytes. `path` excludes the trailing NUL because its field
/// size is `len_path - 2`; a cue comment includes one because `len_comment`
/// counts it.
std::string utf16BeBytes(const QString& text, bool withNull) {
    std::string out;
    out.reserve(static_cast<size_t>((text.size() + (withNull ? 1 : 0)) * 2));
    for (int i = 0; i < text.size(); ++i) {
        const ushort unit = text.at(i).unicode();
        out.push_back(static_cast<char>((unit >> 8) & 0xff));
        out.push_back(static_cast<char>(unit & 0xff));
    }
    if (withNull) {
        out.push_back('\0');
        out.push_back('\0');
    }
    return out;
}

QVector<BandColumn> rampColumns(int count) {
    QVector<BandColumn> columns;
    columns.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int value = count <= 1 ? 0 : i * 255 / (count - 1);
        BandColumn column;
        column.low = static_cast<quint8>(value);
        column.mid = static_cast<quint8>(value / 2);
        column.high = static_cast<quint8>(value / 4);
        column.peak = static_cast<quint8>(value);
        columns.append(column);
    }
    return columns;
}

} // namespace

TEST(RekordboxExportAnlz, WriterMatchesRekordboxSectionLayout) {
    const auto files = AnlzWriter::author(
            QStringLiteral("?/track.mp3"), {}, {}, {});

    ParsedAnlz dat(files.dat);
    EXPECT_EQ(static_cast<uint32_t>(files.dat.size()), dat.file->len_file());
    EXPECT_EQ(28u, dat.file->len_header());
    EXPECT_EQ((std::vector<std::string>{"PPTH",
                      "PVBR",
                      "PQTZ",
                      "PWAV",
                      "PWV2",
                      "PCOB",
                      "PCOB"}),
            tagsOf(*dat.file));

    auto* path = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_PATH);
    ASSERT_NE(nullptr, path);
    auto* pathBody = static_cast<rekordbox_anlz_t::path_tag_t*>(path->body());
    EXPECT_EQ(utf16BeBytes(QStringLiteral("?/track.mp3"), false), pathBody->path());

    // A fresh `.DAT` carries the zero VBR table: a zero header word and 401
    // zero words, which is what rekordbox writes for a track it cannot index.
    auto* vbr = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_VBR);
    ASSERT_NE(nullptr, vbr);
    EXPECT_EQ(16u, vbr->len_header());
    EXPECT_EQ(1620u, vbr->len_tag());

    auto* preview = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_WAVE_PREVIEW);
    ASSERT_NE(nullptr, preview);
    EXPECT_EQ(static_cast<uint32_t>(AnlzWriter::kPreviewColumns),
            static_cast<rekordbox_anlz_t::wave_preview_tag_t*>(preview->body())->len_data());
    auto* tiny = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_WAVE_TINY);
    ASSERT_NE(nullptr, tiny);
    EXPECT_EQ(static_cast<uint32_t>(AnlzWriter::kTinyColumns),
            static_cast<rekordbox_anlz_t::wave_preview_tag_t*>(tiny->body())->len_data());

    ParsedAnlz ext(files.ext);
    EXPECT_EQ((std::vector<std::string>{"PPTH",
                      "PWV3",
                      "PCOB",
                      "PCOB",
                      "PCO2",
                      "PCO2",
                      "PWV5",
                      "PWV4"}),
            tagsOf(*ext.file));

    ParsedAnlz twoEx(files.twoEx);
    EXPECT_EQ((std::vector<std::string>{"PPTH", "PWV7", "PWV6"}),
            tagsOf(*twoEx.file));

    auto* threeBand = findSection(
            twoEx.file.get(), rekordbox_anlz_t::SECTION_TAGS_WAVE_3BAND_PREVIEW);
    ASSERT_NE(nullptr, threeBand);
    auto* threeBandBody =
            static_cast<rekordbox_anlz_t::wave_3band_preview_tag_t*>(threeBand->body());
    EXPECT_EQ(3u, threeBandBody->len_entry_bytes());
    EXPECT_EQ(static_cast<uint32_t>(AnlzWriter::kOverviewColumns),
            threeBandBody->len_entries());
    EXPECT_EQ(static_cast<size_t>(AnlzWriter::kOverviewColumns) * 3,
            threeBandBody->entries().size());
}

TEST(RekordboxExportAnlz, BeatGridRoundTripsThroughKaitaiReader) {
    const auto sampleRate = audio::SampleRate(44100);

    QVector<audio::FramePos> positions;
    for (int i = 0; i < 9; ++i) {
        positions.append(audio::FramePos(22050.0 * i));
    }
    const auto beats = Beats::fromBeatPositions(
            sampleRate,
            positions,
            QStringLiteral("db=1|dbbar=4|dbphase=0"));
    ASSERT_TRUE(beats);

    const auto grid = beatsFromBeats(
            *beats, audio::FramePos(22050.0 * 8));
    ASSERT_EQ(9, grid.size());
    for (int i = 0; i < grid.size(); ++i) {
        EXPECT_EQ(static_cast<quint16>(i % 4 + 1), grid.at(i).beatNumber);
        EXPECT_EQ(12000, grid.at(i).tempoX100);
    }
    EXPECT_EQ(1u, grid.at(0).timeMs);
    EXPECT_EQ(500u, grid.at(1).timeMs);
    EXPECT_EQ(4000u, grid.at(8).timeMs);

    const auto files = AnlzWriter::author(
            QStringLiteral("?/track.mp3"), grid, {}, {});
    ParsedAnlz dat(files.dat);
    auto* section = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID);
    ASSERT_NE(nullptr, section);
    auto* body = static_cast<rekordbox_anlz_t::beat_grid_tag_t*>(section->body());
    ASSERT_EQ(9u, body->num_beats());
    ASSERT_EQ(9u, body->beats()->size());
    for (int i = 0; i < 9; ++i) {
        const auto* beat = body->beats()->at(i).get();
        EXPECT_EQ(grid.at(i).beatNumber, beat->beat_number());
        EXPECT_EQ(grid.at(i).tempoX100, beat->tempo());
        EXPECT_EQ(grid.at(i).timeMs, beat->time());
    }
}

TEST(RekordboxExportAnlz, DownbeatPhaseShiftsBeatNumbers) {
    const auto sampleRate = audio::SampleRate(44100);
    QVector<audio::FramePos> positions;
    for (int i = 0; i < 8; ++i) {
        positions.append(audio::FramePos(22050.0 * i));
    }
    const auto beats = Beats::fromBeatPositions(
            sampleRate,
            positions,
            QStringLiteral("db=1|dbbar=4|dbphase=1"));
    ASSERT_TRUE(beats);

    const auto grid = beatsFromBeats(*beats, audio::FramePos(22050.0 * 7));
    ASSERT_EQ(8, grid.size());
    EXPECT_EQ(4, grid.at(0).beatNumber);
    EXPECT_EQ(1, grid.at(1).beatNumber);
    EXPECT_EQ(2, grid.at(2).beatNumber);
    EXPECT_EQ(3, grid.at(3).beatNumber);
    EXPECT_EQ(4, grid.at(4).beatNumber);
}

TEST(RekordboxExportAnlz, CueEntryMatchesTheReferenceImplementationBytes) {
    // A single hot cue A at 83032 ms with palette color 46. The expected
    // `PCP2` payload is the byte string rbxport's `rbl-anlz` crate asserts
    // for the same cue.
    AnlzCue cue;
    cue.slot = 1;
    cue.timeMs = 83032;
    cue.colorCode = 46;

    const auto sections = AnlzWriter::cueSections({cue}, true);
    ASSERT_EQ(4, sections.size());
    EXPECT_EQ(std::string("PCOB"), std::string(sections.at(0).tag.data(), 4));
    EXPECT_EQ(std::string("PCOB"), std::string(sections.at(1).tag.data(), 4));
    EXPECT_EQ(std::string("PCO2"), std::string(sections.at(2).tag.data(), 4));
    EXPECT_EQ(std::string("PCO2"), std::string(sections.at(3).tag.data(), 4));

    const QByteArray expected(
            "50435032000000100000005800000001010003e800014458ffffffff000100000000000000"
            "000000000000002eff005c00000000000000000000000000000000000000000000000000"
            "000000000000000000000000000000");
    EXPECT_EQ(expected, sections.at(2).payload.toHex());

    // An empty DAT cue list keeps rekordbox's own header: the hot list
    // points at 0xffffffff and the memory list at 0xffffffff when empty.
    AnlzCue memoryCue;
    memoryCue.slot = 0;
    memoryCue.timeMs = 1000;
    const auto emptyHot = AnlzWriter::cueSections({memoryCue}, false);
    ASSERT_EQ(2, emptyHot.size());
    EXPECT_EQ(QByteArray("0000000100000000ffffffff"), emptyHot.at(0).header.toHex());
    EXPECT_EQ(QByteArray("000000000000000100000000"), emptyHot.at(1).header.toHex());
}

TEST(RekordboxExportAnlz, CuesConvertToDeviceListsAndParseBack) {
    QList<CuePointer> cues;
    cues.append(CuePointer(new Cue(
            CueType::HotCue, 0, audio::FramePos(44100.0), audio::FramePos(), RgbColor(qRgb(255, 0, 92)))));
    cues.back()->setLabel(QStringLiteral("Drop"));
    cues.append(CuePointer(new Cue(
            CueType::Loop, 1, audio::FramePos(88200.0), audio::FramePos(132300.0), RgbColor(0))));
    cues.append(CuePointer(new Cue(
            CueType::MainCue, Cue::kNoHotCue, audio::FramePos(22050.0), audio::FramePos(), RgbColor(0))));
    // Intro cues are not device cues and must not be exported.
    cues.append(CuePointer(new Cue(
            CueType::Intro, Cue::kNoHotCue, audio::FramePos(11025.0), audio::FramePos(), RgbColor(0))));

    const auto converted = cuesFromCues(cues, audio::SampleRate(44100));
    ASSERT_EQ(3, converted.size());
    EXPECT_EQ(1u, converted.at(0).slot);
    EXPECT_FALSE(converted.at(0).isLoop);
    EXPECT_EQ(2u, converted.at(1).slot);
    EXPECT_TRUE(converted.at(1).isLoop);
    EXPECT_EQ(3000u, converted.at(1).loopEndMs);
    EXPECT_EQ(0u, converted.at(2).slot);

    const auto files = AnlzWriter::author(
            QStringLiteral("?/track.mp3"), {}, converted, {});
    ParsedAnlz dat(files.dat);
    auto* datHot = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES, 0);
    ASSERT_NE(nullptr, datHot);
    auto* datHotBody = static_cast<rekordbox_anlz_t::cue_tag_t*>(datHot->body());
    EXPECT_EQ(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES, datHotBody->type());
    ASSERT_EQ(2u, datHotBody->cues()->size());
    EXPECT_EQ(1u, datHotBody->cues()->at(0)->hot_cue());
    EXPECT_EQ(1000u, datHotBody->cues()->at(0)->time());
    EXPECT_EQ(rekordbox_anlz_t::CUE_ENTRY_TYPE_MEMORY_CUE,
            datHotBody->cues()->at(0)->type());
    EXPECT_EQ(2u, datHotBody->cues()->at(1)->hot_cue());
    EXPECT_EQ(rekordbox_anlz_t::CUE_ENTRY_TYPE_LOOP,
            datHotBody->cues()->at(1)->type());
    EXPECT_EQ(2000u, datHotBody->cues()->at(1)->time());
    EXPECT_EQ(3000u, datHotBody->cues()->at(1)->loop_time());

    auto* datMemory = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES, 1);
    ASSERT_NE(nullptr, datMemory);
    auto* datMemoryBody = static_cast<rekordbox_anlz_t::cue_tag_t*>(datMemory->body());
    EXPECT_EQ(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES, datMemoryBody->type());
    ASSERT_EQ(1u, datMemoryBody->cues()->size());
    EXPECT_EQ(0u, datMemoryBody->cues()->at(0)->hot_cue());
    EXPECT_EQ(500u, datMemoryBody->cues()->at(0)->time());

    ParsedAnlz ext(files.ext);
    auto* extHot = findSection(ext.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES_2, 0);
    ASSERT_NE(nullptr, extHot);
    auto* extHotBody = static_cast<rekordbox_anlz_t::cue_extended_tag_t*>(extHot->body());
    ASSERT_EQ(2u, extHotBody->cues()->size());
    EXPECT_EQ(utf16BeBytes(QStringLiteral("Drop"), true),
            extHotBody->cues()->at(0)->comment());
    EXPECT_EQ(46u, extHotBody->cues()->at(0)->color_code());
    EXPECT_EQ(0xffu, extHotBody->cues()->at(0)->color_red());
    EXPECT_EQ(0x00u, extHotBody->cues()->at(0)->color_green());
    EXPECT_EQ(0x5cu, extHotBody->cues()->at(0)->color_blue());

    auto* extMemory = findSection(ext.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES_2, 1);
    ASSERT_NE(nullptr, extMemory);
    auto* extMemoryBody =
            static_cast<rekordbox_anlz_t::cue_extended_tag_t*>(extMemory->body());
    ASSERT_EQ(1u, extMemoryBody->cues()->size());
    EXPECT_EQ(500u, extMemoryBody->cues()->at(0)->time());
    EXPECT_EQ(0u, extMemoryBody->cues()->at(0)->color_id());
}

TEST(RekordboxExportAnlz, WaveformEncodersMatchReferenceValues) {
    struct Reference {
        BandColumn column;
        quint16 pwv5Word;
        quint8 pwv3Height;
    };
    // Measured PWV5 words and PWV3 heights from rbxport's own tests.
    const std::array<Reference, 10> reference = {{
            {{44, 102, 109, 186}, 0x4fc0, 16},
            {{217, 94, 55, 198}, 0xe9cc, 19},
            {{189, 45, 35, 222}, 0xe55c, 23},
            {{179, 112, 106, 252}, 0xef78, 30},
            {{233, 76, 21, 253}, 0xe8fc, 31},
            {{176, 173, 127, 239}, 0xd7ec, 27},
            {{162, 90, 57, 190}, 0xee44, 17},
            {{41, 68, 149, 182}, 0x2bc0, 16},
            {{173, 97, 41, 236}, 0xed6c, 27},
            {{133, 216, 145, 253}, 0x97fc, 31},
    }};

    const QVector<BandColumn> empty;
    for (const Reference& expected : reference) {
        const auto pwv5 = AnlzWriter::encodePwv5({expected.column});
        ASSERT_EQ(2, pwv5.size());
        const quint16 word = static_cast<quint16>(
                (static_cast<quint8>(pwv5.at(0)) << 8) |
                static_cast<quint8>(pwv5.at(1)));
        EXPECT_EQ(expected.pwv5Word, word);
        const auto pv3 = AnlzWriter::encodePwv3({expected.column});
        ASSERT_EQ(1, pv3.size());
        EXPECT_EQ(expected.pwv3Height, static_cast<quint8>(pv3.at(0) & 0x1f));
    }

    BandColumn full;
    full.low = 255;
    full.mid = 255;
    full.high = 255;
    full.peak = 255;
    const auto pwv4 = AnlzWriter::encodePwv4({full});
    ASSERT_EQ(AnlzWriter::kOverviewColumns * 6, pwv4.size());
    EXPECT_EQ(127, static_cast<quint8>(pwv4.at(3)));
    EXPECT_EQ(101, static_cast<quint8>(pwv4.at(4)));
    EXPECT_EQ(82, static_cast<quint8>(pwv4.at(5)));

    const auto columns = rampColumns(3000);
    EXPECT_EQ(AnlzWriter::kPreviewColumns, AnlzWriter::encodePwav(columns).size());
    EXPECT_EQ(AnlzWriter::kTinyColumns, AnlzWriter::encodePwv2(columns).size());
    EXPECT_EQ(3000, AnlzWriter::encodePwv3(columns).size());
    EXPECT_EQ(AnlzWriter::kOverviewColumns * 6, AnlzWriter::encodePwv4(columns).size());
    EXPECT_EQ(6000, AnlzWriter::encodePwv5(columns).size());
    EXPECT_EQ(AnlzWriter::kOverviewColumns * 3, AnlzWriter::encodePwv6(columns).size());
    EXPECT_EQ(9000, AnlzWriter::encodePwv7(columns).size());

    // A silent column is not zero: players read zero as "not analyzed yet".
    EXPECT_EQ(2, AnlzWriter::encodePwav(empty).at(0) & 0x1f);
    EXPECT_EQ(1, AnlzWriter::encodePwv2(empty).at(0));
    for (char byte : AnlzWriter::encodePwv2(columns)) {
        EXPECT_LE(static_cast<quint8>(byte), 15);
    }
}

TEST(RekordboxExportAnlz, WaveformSummaryConvertsToBandColumns) {
    Waveform waveform(44100, 44100, 44100, 8, 0);
    const int count = waveform.getDataSize();
    ASSERT_GT(count, 0);

    for (int i = 0; i < count; ++i) {
        WaveformFilteredData& filtered = waveform.data()[i].filtered;
        filtered.low = static_cast<unsigned char>(10 + i);
        filtered.mid = static_cast<unsigned char>(20 + i);
        filtered.high = static_cast<unsigned char>(30 + i);
        filtered.all = static_cast<unsigned char>(40 + i);
    }

    const auto columns = columnsFromWaveform(waveform);
    ASSERT_EQ(count, columns.size());
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(10 + i, static_cast<int>(columns.at(i).low));
        EXPECT_EQ(20 + i, static_cast<int>(columns.at(i).mid));
        EXPECT_EQ(30 + i, static_cast<int>(columns.at(i).high));
        EXPECT_EQ(40 + i, static_cast<int>(columns.at(i).peak));
    }

    EXPECT_EQ(150, columnsForDuration(columns, 1000).size());
    EXPECT_TRUE(columnsForDuration({}, 1000).isEmpty());
    EXPECT_TRUE(columnsForDuration(columns, 0).isEmpty());
}

TEST(RekordboxExportAnlz, PaletteColorCodeIsTheNearestDeviceColor) {
    EXPECT_EQ(43u, deviceColorCode(RgbColor(qRgb(255, 0, 23))));
    EXPECT_EQ(46u, deviceColorCode(RgbColor(qRgb(255, 0, 92))));
    EXPECT_EQ(62u, deviceColorCode(RgbColor(qRgb(26, 0, 255))));
    EXPECT_EQ(0u, deviceColorCode(RgbColor(0)));
}

} // namespace mixxx::rekordbox
