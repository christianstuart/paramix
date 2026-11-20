#include <gtest/gtest.h>

#include <QByteArray>
#include <QChar>
#include <QString>
#include <QVector>
#include <memory>
#include <string>
#include <vector>

// The Kaitai-generated reader is the importer's PDB reader. The test links
// the same library `RekordboxFeature` uses.
#include <rekordbox_pdb.h>

#include "library/export/rekordbox/rekordboxpdbwriter.h"

namespace mixxx {
namespace rekordbox {
namespace {

struct ParsedPdb {
    std::unique_ptr<kaitai::kstream> stream;
    std::unique_ptr<rekordbox_pdb_t> file;

    explicit ParsedPdb(const QByteArray& bytes)
            : stream(std::make_unique<kaitai::kstream>(
                      std::string(bytes.constData(), static_cast<size_t>(bytes.size())))),
              file(std::make_unique<rekordbox_pdb_t>(stream.get())) {
    }
};

rekordbox_pdb_t::table_t* tableOf(
        rekordbox_pdb_t* file, rekordbox_pdb_t::page_type_t type) {
    for (auto* table : *file->tables()) {
        if (table->type() == type) {
            return table;
        }
    }
    return nullptr;
}

/// Follows a table's page chain and collects every present row.
QVector<rekordbox_pdb_t::row_ref_t*> rowsOf(
        rekordbox_pdb_t* file, rekordbox_pdb_t::table_t* table) {
    (void)file;
    QVector<rekordbox_pdb_t::row_ref_t*> rows;
    if (table == nullptr) {
        return rows;
    }
    auto* page = table->first_page()->body();
    const quint32 lastPage = table->last_page()->index();
    for (int guard = 0; guard < 4096; ++guard) {
        if (page->is_data_page() && page->num_rows() > 0) {
            for (auto* group : *page->row_groups()) {
                for (auto* row : *group->rows()) {
                    if (row->present()) {
                        rows.append(row);
                    }
                }
            }
        }
        if (page->page_index() == lastPage) {
            break;
        }
        const quint32 next = page->next_page()->index();
        if (next == page->page_index()) {
            break;
        }
        page = page->next_page()->body();
    }
    return rows;
}

/// The Kaitai reader is built with `KS_STR_ENCODING_NONE`, so a text field is
/// the raw bytes of its encoding: ASCII, or UTF-16LE for long Unicode values.
QString decodeString(rekordbox_pdb_t::device_sql_string_t* string) {
    if (string == nullptr) {
        return {};
    }
    const auto* body = string->body();
    switch (string->length_and_kind()) {
    case 0x40: {
        const auto* text =
                static_cast<const rekordbox_pdb_t::device_sql_long_ascii_t*>(body);
        return QString::fromLatin1(text->text().c_str(),
                static_cast<int>(text->text().size()));
    }
    case 0x90: {
        const auto* text =
                static_cast<const rekordbox_pdb_t::device_sql_long_utf16le_t*>(body);
        const std::string& raw = text->text();
        QString out;
        for (size_t i = 0; i + 1 < raw.size(); i += 2) {
            const quint16 unit =
                    static_cast<quint8>(static_cast<unsigned char>(raw[i])) |
                    (static_cast<quint16>(
                             static_cast<quint8>(static_cast<unsigned char>(raw[i + 1])))
                            << 8);
            if (unit == 0) {
                break;
            }
            out.append(QChar(unit));
        }
        return out;
    }
    default: {
        const auto* text =
                static_cast<const rekordbox_pdb_t::device_sql_short_ascii_t*>(body);
        return QString::fromLatin1(text->text().c_str(),
                static_cast<int>(text->text().size()));
    }
    }
}

quint32 readLe32(const QByteArray& bytes, int at) {
    return static_cast<quint8>(bytes.at(at)) |
            (static_cast<quint32>(static_cast<quint8>(bytes.at(at + 1))) << 8) |
            (static_cast<quint32>(static_cast<quint8>(bytes.at(at + 2))) << 16) |
            (static_cast<quint32>(static_cast<quint8>(bytes.at(at + 3))) << 24);
}

PdbLibrary makeLibrary() {
    PdbLibrary library;
    library.property.deviceName = QStringLiteral("PARAMIX TEST");
    library.property.createdDate = QStringLiteral("2026-11-19");

    library.artists.append(PdbArtist{1, QStringLiteral("Artist One")});
    library.artists.append(PdbArtist{2, QStringLiteral("L'Artiste")});
    library.artists.append(PdbArtist{3, QStringLiteral("Café")});
    library.albums.append(PdbAlbum{10, 1, QStringLiteral("Album One")});
    library.genres.append(PdbGenre{100, QStringLiteral("House")});
    library.keys.append(PdbKey{200, QStringLiteral("8A")});

    PdbTrack first;
    first.id = 1;
    first.artistId = 1;
    first.albumId = 10;
    first.genreId = 100;
    first.keyId = 200;
    first.colorId = 3;
    first.rating = 4;
    first.tempoX100 = 12800;
    first.durationSec = 245;
    first.year = 2024;
    first.bitrate = 320;
    first.sampleRate = 44100;
    first.fileSize = 1234567;
    first.trackNumber = 1;
    first.playCount = 7;
    first.discNumber = 1;
    first.sampleDepth = 16;
    first.masterContentId = 1001;
    first.masterDbId = 42;
    first.contentLink = 0xc0700;
    first.hotCueAutoLoad = true;
    first.title = QStringLiteral("Track One");
    first.filename = QStringLiteral("track1.mp3");
    first.filePath = QStringLiteral("/Contents/Artist One/Album One/track1.mp3");
    first.analyzePath =
            QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT");
    first.comment = QStringLiteral("Peak time");
    first.dateAdded = QStringLiteral("2026-11-19");
    first.dateCreated = QStringLiteral("2026-11-01");
    first.releaseDate = QStringLiteral("2024-05-01");
    first.mixName = QStringLiteral("Original Mix");
    first.isrc = QStringLiteral("USABC1234567");
    library.tracks.append(first);

    PdbTrack second;
    second.id = 2;
    second.artistId = 3;
    second.colorId = 1;
    second.rating = 5;
    second.tempoX100 = 12400;
    second.durationSec = 300;
    second.year = 2020;
    second.bitrate = 1411;
    second.sampleRate = 96000;
    second.fileSize = 7654321;
    second.trackNumber = 2;
    second.sampleDepth = 24;
    // Long ASCII and a non-ASCII title exercise both long string encodings.
    second.title = QStringLiteral("Café Del Mar (Extended Mix)");
    second.filename = QStringLiteral("cafe del mar.flac");
    second.filePath = QStringLiteral(
            "/Contents/Artist/Album One/this is a deliberately very long path "
            "component used to force the long ascii string encoding in the "
            "track row");
    second.comment = QString(200, QLatin1Char('c'));
    second.analyzePath =
            QStringLiteral("/PIONEER/USBANLZ/P002/00000002/ANLZ0000.DAT");
    second.dateAdded = QStringLiteral("2026-11-20");
    library.tracks.append(second);

    library.playlists.append(
            PdbPlaylistNode{1, 0, 0, true, QStringLiteral("Folder")});
    library.playlists.append(
            PdbPlaylistNode{2, 1, 1, false, QStringLiteral("List A")});
    library.playlists.append(
            PdbPlaylistNode{3, 0, 2, false, QStringLiteral("List B")});

    library.entries.append(PdbPlaylistEntry{0, 1, 2});
    library.entries.append(PdbPlaylistEntry{1, 2, 2});
    library.entries.append(PdbPlaylistEntry{0, 2, 3});
    return library;
}

} // namespace

TEST(RekordboxExportPdb, WritesTheTwentyTableDirectory) {
    const QByteArray bytes = PdbWriter::write(makeLibrary());
    ParsedPdb pdb(bytes);

    EXPECT_EQ(static_cast<uint32_t>(PdbWriter::kPageSize), pdb.file->len_page());
    EXPECT_EQ(20u, pdb.file->num_tables());
    ASSERT_EQ(20u, pdb.file->tables()->size());
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(i, static_cast<int>(pdb.file->tables()->at(i)->type()));
    }

    // Every table, empty ones included, has an index page and a candidate.
    for (auto* table : *pdb.file->tables()) {
        EXPECT_NE(0x03ffffffu, table->first_page()->index());
        EXPECT_NE(0x03ffffffu, table->last_page()->index());
    }

    EXPECT_TRUE(rowsOf(pdb.file.get(),
            tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_LABELS))
                    .isEmpty());
    EXPECT_FALSE(rowsOf(pdb.file.get(),
            tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_COLUMNS))
                    .isEmpty());
    EXPECT_EQ(27,
            rowsOf(pdb.file.get(),
                    tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_COLUMNS))
                    .size());
    EXPECT_EQ(22,
            rowsOf(pdb.file.get(),
                    tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_UNKNOWN_17))
                    .size());
    EXPECT_EQ(17,
            rowsOf(pdb.file.get(),
                    tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_UNKNOWN_18))
                    .size());
}

TEST(RekordboxExportPdb, TracksAndNamedTablesRoundTrip) {
    const QByteArray bytes = PdbWriter::write(makeLibrary());
    ParsedPdb pdb(bytes);

    const auto trackRows = rowsOf(
            pdb.file.get(), tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_TRACKS));
    ASSERT_EQ(2, trackRows.size());
    auto* first = static_cast<rekordbox_pdb_t::track_row_t*>(trackRows.at(0)->body());
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(0, first->index_shift());
    EXPECT_EQ(1u, first->id());
    EXPECT_EQ(1u, first->artist_id());
    EXPECT_EQ(10u, first->album_id());
    EXPECT_EQ(100u, first->genre_id());
    EXPECT_EQ(200u, first->key_id());
    EXPECT_EQ(3u, first->color_id());
    EXPECT_EQ(4u, first->rating());
    EXPECT_EQ(12800u, first->tempo());
    EXPECT_EQ(245u, first->duration());
    EXPECT_EQ(2024u, first->year());
    EXPECT_EQ(320u, first->bitrate());
    EXPECT_EQ(44100u, first->sample_rate());
    EXPECT_EQ(1234567u, first->file_size());
    EXPECT_EQ(1u, first->track_number());
    EXPECT_EQ(7u, first->play_count());
    EXPECT_EQ(1u, first->disc_number());
    EXPECT_EQ(16u, first->sample_depth());
    EXPECT_EQ(0xc0700u, first->bitmask());
    EXPECT_EQ(QStringLiteral("Track One"), decodeString(first->title()));
    EXPECT_EQ(QStringLiteral("track1.mp3"), decodeString(first->filename()));
    EXPECT_EQ(QStringLiteral("/Contents/Artist One/Album One/track1.mp3"),
            decodeString(first->file_path()));
    EXPECT_EQ(QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT"),
            decodeString(first->analyze_path()));
    EXPECT_EQ(QStringLiteral("Peak time"), decodeString(first->comment()));
    // The Kaitai names for slots 10 and 15 follow the spec's older reading.
    // rekordbox uses slot 10 for the file's creation date and slot 15 for
    // DATE ADDED, which is the order the writer follows.
    EXPECT_EQ(QStringLiteral("2026-11-01"), decodeString(first->date_added()));
    EXPECT_EQ(QStringLiteral("2026-11-19"), decodeString(first->analyze_date()));
    EXPECT_EQ(QStringLiteral("2024-05-01"), decodeString(first->release_date()));
    EXPECT_EQ(QStringLiteral("Original Mix"), decodeString(first->mix_name()));
    EXPECT_EQ(QStringLiteral("USABC1234567"), decodeString(first->isrc()));
    EXPECT_EQ(QStringLiteral("ON"), decodeString(first->autoload_hot_cues()));

    auto* second = static_cast<rekordbox_pdb_t::track_row_t*>(trackRows.at(1)->body());
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(32, second->index_shift());
    EXPECT_EQ(QStringLiteral("Café Del Mar (Extended Mix)"), decodeString(second->title()));
    EXPECT_EQ(QStringLiteral("cafe del mar.flac"), decodeString(second->filename()));
    EXPECT_TRUE(decodeString(second->file_path()).size() > 126);
    EXPECT_EQ(200, decodeString(second->comment()).size());

    const auto artistRows = rowsOf(
            pdb.file.get(), tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_ARTISTS));
    ASSERT_EQ(3, artistRows.size());
    EXPECT_EQ(QStringLiteral("Artist One"),
            decodeString(static_cast<rekordbox_pdb_t::artist_row_t*>(artistRows.at(0)->body())
                            ->name()));
    EXPECT_EQ(QStringLiteral("Café"),
            decodeString(static_cast<rekordbox_pdb_t::artist_row_t*>(artistRows.at(2)->body())
                            ->name()));
    EXPECT_EQ(32, static_cast<rekordbox_pdb_t::artist_row_t*>(artistRows.at(1)->body())->index_shift());

    const auto albumRows = rowsOf(
            pdb.file.get(), tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_ALBUMS));
    ASSERT_EQ(1, albumRows.size());
    auto* album =
            static_cast<rekordbox_pdb_t::album_row_t*>(albumRows.at(0)->body());
    EXPECT_EQ(10u, album->id());
    EXPECT_EQ(1u, album->artist_id());
    EXPECT_EQ(QStringLiteral("Album One"), decodeString(album->name()));

    const auto genreRows = rowsOf(
            pdb.file.get(), tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_GENRES));
    ASSERT_EQ(1, genreRows.size());
    auto* genre =
            static_cast<rekordbox_pdb_t::genre_row_t*>(genreRows.at(0)->body());
    EXPECT_EQ(100u, genre->id());
    EXPECT_EQ(QStringLiteral("House"), decodeString(genre->name()));

    const auto keyRows = rowsOf(
            pdb.file.get(), tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_KEYS));
    ASSERT_EQ(1, keyRows.size());
    auto* key = static_cast<rekordbox_pdb_t::key_row_t*>(keyRows.at(0)->body());
    EXPECT_EQ(200u, key->id());
    EXPECT_EQ(200u, key->id2());
    EXPECT_EQ(QStringLiteral("8A"), decodeString(key->name()));

    const auto colorRows = rowsOf(
            pdb.file.get(), tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_COLORS));
    ASSERT_EQ(8, colorRows.size());
    auto* color =
            static_cast<rekordbox_pdb_t::color_row_t*>(colorRows.at(0)->body());
    EXPECT_EQ(1u, color->id());
    EXPECT_EQ(QStringLiteral("Pink"), decodeString(color->name()));
}

TEST(RekordboxExportPdb, PlaylistTreeAndEntriesRoundTrip) {
    const QByteArray bytes = PdbWriter::write(makeLibrary());
    ParsedPdb pdb(bytes);

    const auto playlistRows = rowsOf(pdb.file.get(),
            tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_TREE));
    ASSERT_EQ(3, playlistRows.size());
    auto* folder =
            static_cast<rekordbox_pdb_t::playlist_tree_row_t*>(playlistRows.at(0)->body());
    EXPECT_EQ(1u, folder->id());
    EXPECT_EQ(0u, folder->parent_id());
    EXPECT_EQ(0u, folder->sort_order());
    EXPECT_NE(0u, folder->raw_is_folder());
    EXPECT_EQ(QStringLiteral("Folder"), decodeString(folder->name()));

    auto* listA =
            static_cast<rekordbox_pdb_t::playlist_tree_row_t*>(playlistRows.at(1)->body());
    EXPECT_EQ(2u, listA->id());
    EXPECT_EQ(1u, listA->parent_id());
    EXPECT_EQ(1u, listA->sort_order());
    EXPECT_EQ(0u, listA->raw_is_folder());
    EXPECT_EQ(QStringLiteral("List A"), decodeString(listA->name()));

    const auto entryRows = rowsOf(pdb.file.get(),
            tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_PLAYLIST_ENTRIES));
    ASSERT_EQ(3, entryRows.size());
    auto* entry = static_cast<rekordbox_pdb_t::playlist_entry_row_t*>(
            entryRows.at(1)->body());
    EXPECT_EQ(1u, entry->entry_index());
    EXPECT_EQ(2u, entry->track_id());
    EXPECT_EQ(2u, entry->playlist_id());
}

TEST(RekordboxExportPdb, LargeTablesSpanPages) {
    PdbLibrary library;
    library.property.createdDate = QStringLiteral("2026-11-19");
    for (int i = 1; i <= 120; ++i) {
        PdbTrack track;
        track.id = static_cast<quint32>(i);
        track.title = QStringLiteral("Track %1").arg(i);
        track.filename = QStringLiteral("track%1.mp3").arg(i);
        library.tracks.append(track);
    }

    const QByteArray bytes = PdbWriter::write(library);
    ParsedPdb pdb(bytes);
    auto* table = tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_TRACKS);
    ASSERT_NE(nullptr, table);
    EXPECT_NE(table->first_page()->index(), table->last_page()->index());

    const auto trackRows = rowsOf(pdb.file.get(), table);
    ASSERT_EQ(120, trackRows.size());
    auto* last = static_cast<rekordbox_pdb_t::track_row_t*>(trackRows.last()->body());
    EXPECT_EQ(120u, last->id());
    EXPECT_EQ(QStringLiteral("Track 120"), decodeString(last->title()));
}

TEST(RekordboxExportPdb, PropertyRowMatchesRekordboxLayout) {
    const QByteArray bytes = PdbWriter::write(makeLibrary());
    ParsedPdb pdb(bytes);

    auto* table = tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_HISTORY);
    ASSERT_NE(nullptr, table);
    const auto propertyRows = rowsOf(pdb.file.get(), table);
    ASSERT_EQ(1, propertyRows.size());

    auto* row = propertyRows.at(0);
    const int at = static_cast<int>(row->_parent()->_parent()->page_index()) *
                    PdbWriter::kPageSize +
            row->row_base();
    ASSERT_LE(at + 32, bytes.size());
    EXPECT_EQ(0x80, static_cast<quint8>(bytes.at(at)));
    EXPECT_EQ(0x02, static_cast<quint8>(bytes.at(at + 1)));
    EXPECT_EQ(2u, readLe32(bytes, at + 4));
    EXPECT_EQ(0x17, static_cast<quint8>(bytes.at(at + 12)));
    EXPECT_EQ(QStringLiteral("2026-11-19"),
            QString::fromLatin1(bytes.mid(at + 13, 10)));
    EXPECT_EQ(0x19, static_cast<quint8>(bytes.at(at + 23)));
    EXPECT_EQ(0x1e, static_cast<quint8>(bytes.at(at + 24)));
    EXPECT_EQ(0x0b, static_cast<quint8>(bytes.at(at + 25)));
    EXPECT_EQ(QStringLiteral("1000"), QString::fromLatin1(bytes.mid(at + 26, 4)));
}

TEST(RekordboxExportPdb, StringAndAudioTypeEncoders) {
    const QByteArray shortText = PdbWriter::deviceSqlString(QStringLiteral("AB"));
    ASSERT_EQ(3, shortText.size());
    EXPECT_EQ(0x07, static_cast<quint8>(shortText.at(0)));
    EXPECT_EQ("AB", shortText.mid(1).toStdString());

    const QByteArray utf16 = PdbWriter::deviceSqlString(QStringLiteral("Café"));
    ASSERT_EQ(12, utf16.size());
    EXPECT_EQ(0x90, static_cast<quint8>(utf16.at(0)));
    EXPECT_EQ(0x0c, static_cast<quint8>(utf16.at(1)));

    const QString longAsciiText(200, QLatin1Char('a'));
    const QByteArray longAscii = PdbWriter::deviceSqlString(longAsciiText);
    ASSERT_EQ(4 + 200, longAscii.size());
    EXPECT_EQ(0x40, static_cast<quint8>(longAscii.at(0)));
    EXPECT_EQ(0xcc, static_cast<quint8>(longAscii.at(1)));
    EXPECT_EQ(0x00, static_cast<quint8>(longAscii.at(2)));

    EXPECT_EQ(1, PdbWriter::audioFileType(QStringLiteral("TRACK.MP3")));
    EXPECT_EQ(4, PdbWriter::audioFileType(QStringLiteral("track.m4a")));
    EXPECT_EQ(5, PdbWriter::audioFileType(QStringLiteral("track.flac")));
    EXPECT_EQ(11, PdbWriter::audioFileType(QStringLiteral("track.wav")));
    EXPECT_EQ(12, PdbWriter::audioFileType(QStringLiteral("track.aiff")));
    EXPECT_EQ(0, PdbWriter::audioFileType(QStringLiteral("track.ogg")));
}

} // namespace rekordbox
} // namespace mixxx
