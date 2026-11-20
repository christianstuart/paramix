#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <cstdint>

namespace mixxx {
namespace rekordbox {

/// A rekordbox `artists` row.
struct PdbArtist {
    quint32 id = 0;
    QString name;
};

/// A rekordbox `albums` row.
struct PdbAlbum {
    quint32 id = 0;
    quint32 artistId = 0;
    QString name;
};

/// A rekordbox `genres` row.
struct PdbGenre {
    quint32 id = 0;
    QString name;
};

/// A rekordbox `keys` row.
struct PdbKey {
    quint32 id = 0;
    QString name;
};

/// A rekordbox `colors` row. `id` is 1..8; the writer always emits the eight
/// device colors and takes a provided name for a matching id.
struct PdbColor {
    quint16 id = 0;
    QString name;
};

/// A rekordbox `playlist_tree` row: a playlist or a folder.
struct PdbPlaylistNode {
    quint32 id = 0;
    quint32 parentId = 0;
    quint32 sortOrder = 0;
    bool isFolder = false;
    QString name;
};

/// A rekordbox `playlist_entries` row.
struct PdbPlaylistEntry {
    quint32 entryIndex = 0;
    quint32 trackId = 0;
    quint32 playlistId = 0;
};

/// A rekordbox `tracks` row.
///
/// The IDs refer to rows in the other tables. `colorId` is a row in the
/// `colors` table (1..8), and `rating` is 0..5 stars.
struct PdbTrack {
    quint32 id = 0;
    quint32 artistId = 0;
    quint32 albumId = 0;
    quint32 genreId = 0;
    quint32 keyId = 0;
    quint32 labelId = 0;
    quint32 artworkId = 0;
    quint8 colorId = 0;
    quint8 rating = 0;
    /// Beats per minute multiplied by 100.
    quint32 tempoX100 = 0;
    quint16 durationSec = 0;
    quint16 year = 0;
    quint32 bitrate = 0;
    quint32 sampleRate = 0;
    quint32 fileSize = 0;
    quint32 trackNumber = 0;
    quint16 playCount = 0;
    quint16 discNumber = 0;
    quint16 sampleDepth = 0;
    /// Values rekordbox copies from the source library, 0 when unknown.
    quint32 masterContentId = 0;
    quint32 masterDbId = 0;
    quint32 contentLink = 0;

    bool hotCueAutoLoad = false;
    QString title;
    QString filename;
    /// Media-relative, for example `/Contents/Artist/Album/Track.mp3`.
    QString filePath;
    /// Media-relative, for example `/PIONEER/USBANLZ/P001/00000001/ANLZ0000.DAT`.
    QString analyzePath;
    QString comment;
    /// Collection date: what a player shows as DATE ADDED.
    QString dateAdded;
    /// The audio file's creation date.
    QString dateCreated;
    QString releaseDate;
    QString mixName;
    QString isrc;
};

/// The single live `property` row (page type 19).
struct PdbProperty {
    QString deviceName;
    QString dbVersion = QStringLiteral("1000");
    /// Ten ASCII bytes, `YYYY-MM-DD`.
    QString createdDate;
    quint32 contents = 0;
    /// 0 default, 1..8 the device colors Pink..Purple.
    quint8 backgroundColor = 0;
};

/// Everything `export.pdb` needs.
struct PdbLibrary {
    QVector<PdbTrack> tracks;
    QVector<PdbGenre> genres;
    QVector<PdbArtist> artists;
    QVector<PdbAlbum> albums;
    QVector<PdbKey> keys;
    QVector<PdbColor> colors;
    QVector<PdbPlaylistNode> playlists;
    QVector<PdbPlaylistEntry> entries;
    PdbProperty property;
};

/// Writer for rekordbox's `export.pdb` DeviceSQL database.
///
/// The page layout, row layouts and the twenty-table file shape are taken from
/// chrisle/rbxport's `rbl-pdb` crate at commit
/// d0ff75b378ec85be891d9f3067ada1e82bad4cae and from
/// `lib/rekordbox-metadata/rekordbox_pdb.ksy`.
class PdbWriter {
  public:
    static constexpr int kPageSize = 4096;

    /// Writes the whole file. `property.contents` is set to the track count.
    static QByteArray write(const PdbLibrary& library);

    /// `DeviceSQL` string encodings: short ASCII, long ASCII or long UTF-16LE.
    static QByteArray deviceSqlString(const QString& text);

    static QByteArray trackRow(const PdbTrack& track);
    static QByteArray artistRow(const PdbArtist& artist);
    static QByteArray albumRow(const PdbAlbum& album);
    static QByteArray genreRow(const PdbGenre& genre);
    static QByteArray keyRow(const PdbKey& key);
    static QByteArray colorRow(const PdbColor& color);
    static QByteArray playlistRow(const PdbPlaylistNode& node);
    static QByteArray playlistEntryRow(const PdbPlaylistEntry& entry);
    static QByteArray propertyRow(const PdbProperty& property);

    /// `DeviceSQL`'s audio format code for a file name.
    static quint16 audioFileType(const QString& filename);
};

} // namespace rekordbox
} // namespace mixxx
