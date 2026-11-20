#include <gtest/gtest.h>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>
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

    // A fresh `.DAT` carries the zero VBR table described by `vbr_tag`: one
    // unused word and 400 zero entries, the form used for a track whose
    // bitrate is not indexed.
    auto* vbr = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_VBR);
    ASSERT_NE(nullptr, vbr);
    EXPECT_EQ(16u, vbr->len_header());
    EXPECT_EQ(1616u, vbr->len_tag());

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

TEST(RekordboxExportAnlz, CueListsRoundTripThroughKaitaiReader) {
    // The reader is the oracle for both cue list forms: `cue_tag` (`PCPT`) and
    // `cue_extended_tag` (`PCP2`) in `rekordbox_anlz.ksy`. No expected byte
    // string from a third-party implementation is used.
    AnlzCue hot;
    hot.slot = 1;
    hot.timeMs = 83032;
    hot.colorCode = 46;
    hot.comment = QStringLiteral("Drop");

    AnlzCue memory;
    memory.slot = 0;
    memory.timeMs = 1000;

    const auto dat = ParsedAnlz(
            AnlzWriter::render(AnlzWriter::cueSections({hot, memory}, false)));
    EXPECT_EQ((std::vector<std::string>{"PCOB", "PCOB"}), tagsOf(*dat.file));

    auto* hotList = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES, 0);
    ASSERT_NE(nullptr, hotList);
    auto* hotBody = static_cast<rekordbox_anlz_t::cue_tag_t*>(hotList->body());
    EXPECT_EQ(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES, hotBody->type());
    ASSERT_EQ(1, static_cast<int>(hotBody->num_cues()));
    const auto* hotEntry = hotBody->cues()->at(0).get();
    EXPECT_EQ(1u, hotEntry->hot_cue());
    EXPECT_EQ(83032u, hotEntry->time());
    EXPECT_EQ(0xffffffffu, hotEntry->loop_time());
    EXPECT_EQ(rekordbox_anlz_t::CUE_ENTRY_TYPE_MEMORY_CUE, hotEntry->type());
    // The `.ksy` leaves the last header word unexplained; the writer records
    // the observed convention, 0xffffffff for a hot list.
    EXPECT_EQ(0xffffffffu, hotBody->memory_count());

    auto* memoryList = findSection(dat.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES, 1);
    ASSERT_NE(nullptr, memoryList);
    auto* memoryBody = static_cast<rekordbox_anlz_t::cue_tag_t*>(memoryList->body());
    EXPECT_EQ(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES, memoryBody->type());
    ASSERT_EQ(1, static_cast<int>(memoryBody->num_cues()));
    EXPECT_EQ(0u, memoryBody->cues()->at(0)->hot_cue());
    EXPECT_EQ(1000u, memoryBody->cues()->at(0)->time());
    EXPECT_EQ(0u, memoryBody->memory_count());

    const auto ext = ParsedAnlz(
            AnlzWriter::render(AnlzWriter::cueSections({hot, memory}, true)));
    EXPECT_EQ((std::vector<std::string>{"PCOB", "PCOB", "PCO2", "PCO2"}),
            tagsOf(*ext.file));

    // An `.EXT`'s legacy `PCOB` list carries only hot cues D-P: A-C are in the
    // `.DAT` and memory cues are covered by `PCO2`. Both lists are therefore
    // empty for a slot-A hot cue and one memory cue.
    auto* legacyHot = findSection(ext.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES, 0);
    ASSERT_NE(nullptr, legacyHot);
    EXPECT_EQ(0, static_cast<int>(static_cast<rekordbox_anlz_t::cue_tag_t*>(legacyHot->body())->num_cues()));
    auto* legacyMemory = findSection(ext.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES, 1);
    ASSERT_NE(nullptr, legacyMemory);
    EXPECT_EQ(0,
            static_cast<int>(
                    static_cast<rekordbox_anlz_t::cue_tag_t*>(legacyMemory->body())->num_cues()));

    auto* extHot = findSection(ext.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES_2, 0);
    ASSERT_NE(nullptr, extHot);
    auto* extHotBody = static_cast<rekordbox_anlz_t::cue_extended_tag_t*>(extHot->body());
    ASSERT_EQ(1, static_cast<int>(extHotBody->num_cues()));
    const auto* extEntry = extHotBody->cues()->at(0).get();
    EXPECT_EQ(1u, extEntry->hot_cue());
    EXPECT_EQ(83032u, extEntry->time());
    EXPECT_EQ(0xffffffffu, extEntry->loop_time());
    EXPECT_EQ(utf16BeBytes(QStringLiteral("Drop"), true), extEntry->comment());
    EXPECT_EQ(46u, extEntry->color_code());
    EXPECT_EQ(0xffu, extEntry->color_red());
    EXPECT_EQ(0x00u, extEntry->color_green());
    EXPECT_EQ(0x5cu, extEntry->color_blue());

    auto* extMemory = findSection(ext.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES_2, 1);
    ASSERT_NE(nullptr, extMemory);
    auto* extMemoryBody =
            static_cast<rekordbox_anlz_t::cue_extended_tag_t*>(extMemory->body());
    ASSERT_EQ(1, static_cast<int>(extMemoryBody->num_cues()));
    EXPECT_EQ(1000u, extMemoryBody->cues()->at(0)->time());
    EXPECT_EQ(0u, extMemoryBody->cues()->at(0)->color_id());
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

TEST(RekordboxExportAnlz, WaveformEncodersFollowTheirDocumentedPacking) {
    // `rekordbox_anlz.ksy` fixes each tag's entry width but not its bit
    // layout. The layout is recorded in docs/rekordbox-export.md ("Waveforms")
    // from rekordbox's own files, and these assertions check the encoders
    // against it. No third-party test vector is used.
    const auto wordOf = [](const QByteArray& bytes) {
        EXPECT_EQ(2, bytes.size());
        return static_cast<quint16>((static_cast<quint8>(bytes.at(0)) << 8) |
                static_cast<quint8>(bytes.at(1)));
    };

    BandColumn lowOnly;
    lowOnly.low = 255;
    lowOnly.peak = 255;
    BandColumn midOnly;
    midOnly.mid = 255;
    midOnly.peak = 255;
    BandColumn highOnly;
    highOnly.high = 255;
    highOnly.peak = 255;

    // `PWV5` is rrrgggbbbhhhhh00: the dominant band fills its own field.
    const quint16 lowWord = wordOf(AnlzWriter::encodePwv5({lowOnly}));
    EXPECT_EQ(7, static_cast<int>((lowWord >> 13) & 0x7));
    EXPECT_EQ(0, static_cast<int>((lowWord >> 10) & 0x7));
    EXPECT_EQ(0, static_cast<int>((lowWord >> 7) & 0x7));
    const quint16 midWord = wordOf(AnlzWriter::encodePwv5({midOnly}));
    EXPECT_EQ(0, static_cast<int>((midWord >> 13) & 0x7));
    EXPECT_EQ(5, static_cast<int>((midWord >> 10) & 0x7));
    EXPECT_EQ(0, static_cast<int>((midWord >> 7) & 0x7));
    const quint16 highWord = wordOf(AnlzWriter::encodePwv5({highOnly}));
    EXPECT_EQ(0, static_cast<int>((highWord >> 13) & 0x7));
    EXPECT_EQ(0, static_cast<int>((highWord >> 10) & 0x7));
    EXPECT_EQ(7, static_cast<int>((highWord >> 7) & 0x7));
    // The low two bits are unused and the height occupies bits 2-6. A full
    // peak is the detail ceiling, which `PWV3` also reaches.
    EXPECT_EQ(0, static_cast<int>(lowWord & 0x3));
    EXPECT_EQ(0x1f, static_cast<int>((lowWord >> 2) & 0x1f));
    EXPECT_EQ(0x1f,
            static_cast<int>(static_cast<quint8>(AnlzWriter::encodePwv3({lowOnly}).at(0)) &
                    0x1f));

    BandColumn full;
    full.low = 255;
    full.mid = 255;
    full.high = 255;
    full.peak = 255;
    const auto pwv4 = AnlzWriter::encodePwv4({full});
    ASSERT_EQ(AnlzWriter::kOverviewColumns * 6, pwv4.size());
    // Six-byte entry: peak, two unused bytes, then the three bands.
    EXPECT_EQ(127, static_cast<quint8>(pwv4.at(0)));
    EXPECT_EQ(0, static_cast<quint8>(pwv4.at(1)));
    EXPECT_EQ(0, static_cast<quint8>(pwv4.at(2)));
    EXPECT_EQ(127, static_cast<quint8>(pwv4.at(3)));
    EXPECT_EQ(101, static_cast<quint8>(pwv4.at(4)));
    EXPECT_EQ(82, static_cast<quint8>(pwv4.at(5)));

    // `PWV6` is three bytes per entry at the documented band scale.
    const auto pwv6 = AnlzWriter::encodePwv6({full});
    ASSERT_EQ(AnlzWriter::kOverviewColumns * 3, pwv6.size());
    EXPECT_EQ(44, static_cast<quint8>(pwv6.at(0)));
    // `PWV7` is three seven-bit bands.
    EXPECT_EQ(127, static_cast<quint8>(AnlzWriter::encodePwv7({full}).at(0)));

    const auto columns = rampColumns(3000);
    EXPECT_EQ(AnlzWriter::kPreviewColumns, AnlzWriter::encodePwav(columns).size());
    EXPECT_EQ(AnlzWriter::kTinyColumns, AnlzWriter::encodePwv2(columns).size());
    EXPECT_EQ(3000, AnlzWriter::encodePwv3(columns).size());
    EXPECT_EQ(AnlzWriter::kOverviewColumns * 6, AnlzWriter::encodePwv4(columns).size());
    EXPECT_EQ(6000, AnlzWriter::encodePwv5(columns).size());
    EXPECT_EQ(AnlzWriter::kOverviewColumns * 3, AnlzWriter::encodePwv6(columns).size());
    EXPECT_EQ(9000, AnlzWriter::encodePwv7(columns).size());

    // A silent column is not zero: players read zero as "not analyzed yet".
    const QVector<BandColumn> empty;
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
