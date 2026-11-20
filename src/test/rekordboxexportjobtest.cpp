#include <gtest/gtest.h>

#include <QDate>
#include <QString>
#include <QVector>

#include "library/export/rekordbox/rekordboxexportjob.h"

namespace mixxx {
namespace rekordbox {
namespace {

RekordboxTrackInfo makeTrack(quint32 id,
        const QString& artist,
        const QString& albumArtist,
        const QString& album,
        const QString& genre,
        const QString& keyText) {
    RekordboxTrackInfo track;
    track.id = id;
    track.artist = artist;
    track.albumArtist = albumArtist;
    track.album = album;
    track.genre = genre;
    track.keyText = keyText;
    track.title = QStringLiteral("Track %1").arg(id);
    track.filename = QStringLiteral("track%1.mp3").arg(id);
    track.mediaPath = QStringLiteral("/Contents/Artist/Album/%1 - track%2.mp3")
                              .arg(id)
                              .arg(id);
    track.analyzePath = analysisMediaPath(id);
    track.durationSec = 240;
    track.tempoX100 = 12400;
    track.rating = 4;
    track.dateAdded = QStringLiteral("2026-11-20");
    track.dateCreated = QStringLiteral("2026-11-19");
    return track;
}

TEST(RekordboxExportJob, SanitizesPathComponents) {
    EXPECT_EQ(QStringLiteral("AC_DC_ Live_"),
            sanitizePathComponent(QStringLiteral("AC/DC: Live?")));
    EXPECT_EQ(QStringLiteral("Unknown"), sanitizePathComponent(QString()));
    EXPECT_EQ(QStringLiteral("Unknown"), sanitizePathComponent(QStringLiteral("  ...  ")));
    EXPECT_EQ(QStringLiteral("a_b"), sanitizePathComponent(QStringLiteral("a\\b")));
}

TEST(RekordboxExportJob, BuildsDevicePaths) {
    EXPECT_EQ(QStringLiteral("Contents/Artist/Album/12 - track.mp3"),
            trackRelativePath(QStringLiteral("Artist"),
                    QStringLiteral("Album"),
                    QStringLiteral("track.mp3"),
                    12));
    EXPECT_EQ(QStringLiteral("/Contents/Artist/Album/12 - track.mp3"),
            mediaPath(QStringLiteral("Contents/Artist/Album/12 - track.mp3")));
    EXPECT_EQ(QStringLiteral("/PIONEER/USBANLZ/P001/00000001"),
            analysisMediaDir(1));
    EXPECT_EQ(QStringLiteral("/PIONEER/USBANLZ/P002/00001001"),
            analysisMediaDir(1001));
    EXPECT_EQ(QStringLiteral("/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT"),
            analysisMediaPath(1));
    EXPECT_EQ(0, mp3TimingOffsetMs(QStringLiteral("track.wav")));
}

TEST(RekordboxExportJob, BuildsPdbTablesAndPlaylists) {
    QVector<RekordboxTrackInfo> tracks;
    tracks.append(makeTrack(1,
            QStringLiteral("Artist"),
            QStringLiteral("Album Artist"),
            QStringLiteral("Album"),
            QStringLiteral("House"),
            QStringLiteral("8A")));
    tracks.append(makeTrack(2,
            QStringLiteral("Artist"),
            QStringLiteral("Album Artist"),
            QStringLiteral("Album"),
            QStringLiteral("House"),
            QStringLiteral("8A")));
    tracks.append(makeTrack(3, {}, {}, {}, {}, {}));

    RekordboxPlaylistInfo playlist;
    playlist.name = QStringLiteral("Set");
    playlist.trackIds = {1, 3, 99};

    const PdbLibrary library = buildPdbLibrary(
            tracks, {playlist}, QStringLiteral("PARAMIX"), QDate(2026, 11, 20));

    ASSERT_EQ(3, library.tracks.size());
    EXPECT_EQ(QStringLiteral("Track 1"), library.tracks.at(0).title);
    EXPECT_EQ(analysisMediaPath(1), library.tracks.at(0).analyzePath);
    EXPECT_EQ(1u, library.tracks.at(1).albumId);

    ASSERT_EQ(2, library.artists.size());
    ASSERT_EQ(1, library.albums.size());
    EXPECT_EQ(library.artists.at(1).id, library.albums.first().artistId);
    ASSERT_EQ(1, library.genres.size());
    ASSERT_EQ(1, library.keys.size());

    const PdbTrack& third = library.tracks.at(2);
    EXPECT_EQ(0u, third.artistId);
    EXPECT_EQ(0u, third.albumId);
    EXPECT_EQ(0u, third.genreId);
    EXPECT_EQ(0u, third.keyId);

    ASSERT_EQ(1, library.playlists.size());
    EXPECT_EQ(QStringLiteral("Set"), library.playlists.first().name);
    ASSERT_EQ(2, library.entries.size());
    EXPECT_EQ(1u, library.entries.at(0).trackId);
    EXPECT_EQ(3u, library.entries.at(1).trackId);
    EXPECT_EQ(0u, library.entries.at(0).entryIndex);
    EXPECT_EQ(1u, library.entries.at(1).entryIndex);

    EXPECT_EQ(QStringLiteral("PARAMIX"), library.property.deviceName);
    EXPECT_EQ(QStringLiteral("2026-11-20"), library.property.createdDate);
}

} // namespace
} // namespace rekordbox
} // namespace mixxx
