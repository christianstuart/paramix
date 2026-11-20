#pragma once

#include <QAtomicInteger>
#include <QDate>
#include <QList>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QThread>
#include <QVector>
#include <memory>

#include "library/export/rekordbox/rekordboxpdbwriter.h"
#include "library/trackset/crate/crateid.h"
#include "track/track_decl.h"
#include "track/trackid.h"
#include "track/trackref.h"

class TrackCollectionManager;
class Waveform;

namespace mixxx {

struct RekordboxExportRequest;

namespace rekordbox {

/// The fields of one track that `export.pdb` needs. Kept separate from `Track`
/// so the database layout can be tested without a track collection.
struct RekordboxTrackInfo {
    quint32 id = 0;
    QString title;
    QString artist;
    QString albumArtist;
    QString album;
    QString genre;
    QString keyText;
    QString comment;
    QString filename;
    QString mediaPath;
    QString analyzePath;
    quint32 durationSec = 0;
    quint32 tempoX100 = 0;
    quint16 year = 0;
    quint16 trackNumber = 0;
    quint16 discNumber = 0;
    quint16 playCount = 0;
    quint8 rating = 0;
    quint32 bitrate = 0;
    quint32 sampleRate = 0;
    quint32 fileSize = 0;
    QString dateAdded;
    QString dateCreated;
};

struct RekordboxPlaylistInfo {
    QString name;
    QList<quint32> trackIds;
};

/// Allocates the artist/album/genre/key ids and fills the playlist tree.
/// Track and playlist ids are supplied by the caller so the result is stable.
PdbLibrary buildPdbLibrary(const QVector<RekordboxTrackInfo>& tracks,
        const QVector<RekordboxPlaylistInfo>& playlists,
        const QString& deviceName,
        const QDate& exportDate);

/// Replaces characters that are invalid in a FAT/exFAT path component.
/// Returns `Unknown` for an empty result.
QString sanitizePathComponent(const QString& text);

/// The path below the device root, for example
/// `Contents/Artist/Album/12 - Track.mp3`.
QString trackRelativePath(
        const QString& artist, const QString& album, const QString& filename, quint32 trackId);

/// The media-relative path stored in the PDB and the ANLZ `PPTH` section.
QString mediaPath(const QString& relativePath);

/// The media-relative directory of a track's ANLZ files.
QString analysisMediaDir(quint32 trackId);

/// The media-relative `ANLZ0000.DAT` path stored in the PDB.
QString analysisMediaPath(quint32 trackId);

/// The MP3 decoder timing offset in milliseconds, matching the importer.
/// Non-MP3 files return 0.
int mp3TimingOffsetMs(const QString& location);

/// The export job writes a rekordbox device tree on a worker thread. Tracks
/// are loaded on the collection thread, then copied and analyzed on the
/// worker thread.
class RekordboxExportJob : public QThread {
    Q_OBJECT
  public:
    RekordboxExportJob(QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            QSharedPointer<RekordboxExportRequest> pRequest);

    ~RekordboxExportJob() override;

    void run() override;

  signals:
    void jobMaximum(int maximum);
    void jobProgress(int progress);
    void completed(int numTracks, int numPlaylists);
    void failed(const QString& message);

  public slots:
    void slotCancel();

  private slots:
    // Loading must happen on the collection thread, not the worker thread.
    void loadIds(const QSet<CrateId>& crateIds, const QSet<int>& playlistIds);
    void loadTrack(const TrackRef& trackRef);
    void loadPlaylist(int playlistId);

  private:
    QList<TrackRef> m_trackRefs;
    QList<QPair<int, QString>> m_playlistIdsAndNames;
    TrackPointer m_pLastLoadedTrack;
    std::unique_ptr<Waveform> m_pLastLoadedWaveform;
    QList<TrackId> m_lastLoadedPlaylistTrackIds;

    QAtomicInteger<int> m_cancellationRequested;
    TrackCollectionManager* m_pTrackCollectionManager;
    QSharedPointer<RekordboxExportRequest> m_pRequest;
    QString m_lastErrorMessage;
};

} // namespace rekordbox
} // namespace mixxx
