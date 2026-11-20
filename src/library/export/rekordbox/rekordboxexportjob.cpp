#include "library/export/rekordbox/rekordboxexportjob.h"

#include <mp3guessenc.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPair>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QStringList>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include "library/dao/analysisdao.h"
#include "library/dao/playlistdao.h"
#include "library/export/rekordbox/rekordboxanlzwriter.h"
#include "library/export/rekordbox/rekordboxexportrequest.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "moc_rekordboxexportjob.cpp"
#include "track/track.h"
#include "util/thread_affinity.h"
#include "waveform/waveformfactory.h"

namespace mixxx {
namespace rekordbox {

namespace {

constexpr qint64 kFreeSpaceSlackBytes = 16 * 1024 * 1024;
constexpr int kAnalysisFolderSize = 1000;

const QStringList kSupportedFileTypes = {
        QStringLiteral("aac"),
        QStringLiteral("aiff"),
        QStringLiteral("alac"),
        QStringLiteral("flac"),
        QStringLiteral("m4a"),
        QStringLiteral("mp3"),
        QStringLiteral("wav"),
};

struct ExportCancelled {};

[[noreturn]] void fail(const QString& message) {
    throw std::runtime_error(message.toUtf8().toStdString());
}

void writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        fail(QStringLiteral("Could not write %1: %2").arg(path, file.errorString()));
    }
    if (file.write(bytes) != bytes.size()) {
        fail(QStringLiteral("Could not write %1: %2").arg(path, file.errorString()));
    }
}

void addTimingOffset(QVector<AnlzBeat>* pBeats,
        QVector<AnlzCue>* pCues,
        int offsetMs) {
    if (offsetMs <= 0) {
        return;
    }
    const auto add = [offsetMs](quint32 timeMs) {
        return static_cast<quint32>(std::min<quint64>(
                static_cast<quint64>(timeMs) + static_cast<quint64>(offsetMs),
                std::numeric_limits<quint32>::max()));
    };
    for (AnlzBeat& beat : *pBeats) {
        beat.timeMs = add(beat.timeMs);
    }
    for (AnlzCue& cue : *pCues) {
        cue.timeMs = add(cue.timeMs);
        if (cue.isLoop) {
            cue.loopEndMs = add(cue.loopEndMs);
        }
    }
}

} // namespace

PdbLibrary buildPdbLibrary(const QVector<RekordboxTrackInfo>& tracks,
        const QVector<RekordboxPlaylistInfo>& playlists,
        const QString& deviceName,
        const QDate& exportDate) {
    PdbLibrary library;
    library.property.deviceName = deviceName;
    library.property.createdDate = exportDate.toString(QStringLiteral("yyyy-MM-dd"));

    QHash<QString, quint32> artistIds;
    QHash<QString, quint32> albumIds;
    QHash<QString, quint32> genreIds;
    QHash<QString, quint32> keyIds;
    quint32 nextArtistId = 1;
    quint32 nextAlbumId = 1;
    quint32 nextGenreId = 1;
    quint32 nextKeyId = 1;

    QSet<quint32> exportedTrackIds;
    for (const RekordboxTrackInfo& info : tracks) {
        exportedTrackIds.insert(info.id);

        PdbTrack track;
        track.id = info.id;
        track.title = info.title;
        track.filename = info.filename;
        track.filePath = info.mediaPath;
        track.analyzePath = info.analyzePath;
        track.comment = info.comment;
        track.durationSec = static_cast<quint16>(
                std::min<quint32>(info.durationSec, std::numeric_limits<quint16>::max()));
        track.tempoX100 = info.tempoX100;
        track.year = info.year;
        track.trackNumber = info.trackNumber;
        track.discNumber = info.discNumber;
        track.playCount = info.playCount;
        track.rating = static_cast<quint8>(std::min<quint8>(info.rating, 5));
        track.bitrate = info.bitrate;
        track.sampleRate = info.sampleRate;
        track.fileSize = info.fileSize;
        track.dateAdded = info.dateAdded;
        track.dateCreated = info.dateCreated;

        const auto intern = [](const QString& name,
                                    QHash<QString, quint32>* pIds,
                                    quint32* pNextId) -> quint32 {
            if (name.isEmpty()) {
                return 0;
            }
            auto it = pIds->find(name);
            if (it != pIds->end()) {
                return *it;
            }
            const quint32 id = (*pNextId)++;
            pIds->insert(name, id);
            return id;
        };

        track.artistId = intern(info.artist, &artistIds, &nextArtistId);
        track.genreId = intern(info.genre, &genreIds, &nextGenreId);
        track.keyId = intern(info.keyText, &keyIds, &nextKeyId);

        if (!info.album.isEmpty()) {
            const QString albumArtist = info.albumArtist.isEmpty() ? info.artist : info.albumArtist;
            const quint32 albumArtistId = intern(
                    albumArtist, &artistIds, &nextArtistId);
            const QString albumKey = albumArtist + QChar(0x1f) + info.album;
            auto it = albumIds.find(albumKey);
            if (it == albumIds.end()) {
                const quint32 id = nextAlbumId++;
                albumIds.insert(albumKey, id);
                library.albums.append(
                        PdbAlbum{id, albumArtistId, info.album});
                it = albumIds.find(albumKey);
            }
            track.albumId = *it;
        }

        library.tracks.append(track);
    }

    QVector<QPair<quint32, QString>> namedRows;
    namedRows.reserve(artistIds.size());
    for (auto it = artistIds.constBegin(); it != artistIds.constEnd(); ++it) {
        namedRows.append({it.value(), it.key()});
    }
    std::sort(namedRows.begin(), namedRows.end());
    for (const auto& row : std::as_const(namedRows)) {
        library.artists.append(PdbArtist{row.first, row.second});
    }

    namedRows.clear();
    namedRows.reserve(genreIds.size());
    for (auto it = genreIds.constBegin(); it != genreIds.constEnd(); ++it) {
        namedRows.append({it.value(), it.key()});
    }
    std::sort(namedRows.begin(), namedRows.end());
    for (const auto& row : std::as_const(namedRows)) {
        library.genres.append(PdbGenre{row.first, row.second});
    }

    namedRows.clear();
    namedRows.reserve(keyIds.size());
    for (auto it = keyIds.constBegin(); it != keyIds.constEnd(); ++it) {
        namedRows.append({it.value(), it.key()});
    }
    std::sort(namedRows.begin(), namedRows.end());
    for (const auto& row : std::as_const(namedRows)) {
        library.keys.append(PdbKey{row.first, row.second});
    }

    quint32 nextPlaylistId = 1;
    for (const RekordboxPlaylistInfo& playlist : playlists) {
        const quint32 playlistId = nextPlaylistId++;
        library.playlists.append(PdbPlaylistNode{
                playlistId, 0, playlistId - 1, false, playlist.name});

        quint32 entryIndex = 0;
        for (const quint32 trackId : playlist.trackIds) {
            if (!exportedTrackIds.contains(trackId)) {
                continue;
            }
            library.entries.append(
                    PdbPlaylistEntry{entryIndex++, trackId, playlistId});
        }
    }

    return library;
}

QString sanitizePathComponent(const QString& text) {
    static const QRegularExpression invalidChars(
            QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]"));
    QString sanitized = text;
    sanitized.replace(invalidChars, QStringLiteral("_"));
    sanitized = sanitized.trimmed();
    while (sanitized.endsWith(QChar('.')) || sanitized.endsWith(QChar(' '))) {
        sanitized.chop(1);
    }
    if (sanitized.isEmpty()) {
        return QStringLiteral("Unknown");
    }
    return sanitized.left(100);
}

QString trackRelativePath(
        const QString& artist, const QString& album, const QString& filename, quint32 trackId) {
    return QStringLiteral("Contents/%1/%2/%3 - %4")
            .arg(sanitizePathComponent(artist),
                    sanitizePathComponent(album),
                    QString::number(trackId),
                    sanitizePathComponent(filename));
}

QString mediaPath(const QString& relativePath) {
    QString path = relativePath;
    path.replace(QChar('\\'), QChar('/'));
    if (!path.startsWith(QChar('/'))) {
        path.prepend(QChar('/'));
    }
    return path;
}

QString analysisMediaDir(quint32 trackId) {
    const quint32 folder = (trackId - 1) / kAnalysisFolderSize + 1;
    return QStringLiteral("/PIONEER/USBANLZ/P%1/%2")
            .arg(folder, 3, 10, QChar('0'))
            .arg(trackId, 8, 10, QChar('0'));
}

QString analysisMediaPath(quint32 trackId) {
    return analysisMediaDir(trackId) + QStringLiteral("/ANLZ0000.DAT");
}

int mp3TimingOffsetMs(const QString& location) {
    if (!location.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive)) {
        return 0;
    }
    const int timingShiftCase = mp3guessenc_timing_shift_case(location.toStdString().c_str());
    switch (timingShiftCase) {
#ifdef __COREAUDIO__
    case EXIT_CODE_CASE_A:
        return 12;
    case EXIT_CODE_CASE_B:
        return 13;
    case EXIT_CODE_CASE_C:
        return 26;
    case EXIT_CODE_CASE_D:
        return 50;
#elif defined(__MAD__)
    case EXIT_CODE_CASE_A:
    case EXIT_CODE_CASE_D:
        return 26;
#elif defined(__FFMPEG__)
    case EXIT_CODE_CASE_D:
        return 26;
#endif
    default:
        return 0;
    }
}

RekordboxExportJob::RekordboxExportJob(QObject* parent,
        TrackCollectionManager* pTrackCollectionManager,
        QSharedPointer<RekordboxExportRequest> pRequest)
        : QThread{parent},
          m_pTrackCollectionManager(pTrackCollectionManager),
          m_pRequest{std::move(pRequest)} {
    if (parent != nullptr) {
        DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);
    } else {
        DEBUG_ASSERT(m_pTrackCollectionManager);
        moveToThread(m_pTrackCollectionManager->thread());
    }
}

RekordboxExportJob::~RekordboxExportJob() = default;

void RekordboxExportJob::loadIds(
        const QSet<CrateId>& crateIds, const QSet<int>& playlistIds) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);
    m_lastErrorMessage.clear();

    if (crateIds.isEmpty() && playlistIds.isEmpty()) {
        QSet<TrackRef> trackRefs;
        const auto dirInfos = m_pTrackCollectionManager->internalCollection()
                                      ->getDirectoryDAO()
                                      .loadAllDirectories();
        for (const mixxx::FileInfo& dirInfo : dirInfos) {
            const auto refs = m_pTrackCollectionManager->internalCollection()
                                      ->getTrackDAO()
                                      .getAllTrackRefs(dirInfo.toQDir());
            for (const auto& ref : refs) {
                trackRefs.insert(ref);
            }
        }
        m_trackRefs = QList<TrackRef>{trackRefs.begin(), trackRefs.end()};

        m_playlistIdsAndNames = m_pTrackCollectionManager->internalCollection()
                                        ->getPlaylistDAO()
                                        .getPlaylists(PlaylistDAO::PLHT_NOT_HIDDEN);
    } else {
        QSet<TrackRef> trackRefs;
        for (const auto& crateId : crateIds) {
            auto result = m_pTrackCollectionManager->internalCollection()
                                  ->crates()
                                  .selectCrateTracksSorted(crateId);
            while (result.next()) {
                const auto trackId = result.trackId();
                const auto location = m_pTrackCollectionManager->internalCollection()
                                              ->getTrackDAO()
                                              .getTrackLocation(trackId);
                trackRefs.insert(TrackRef::fromFilePath(location, trackId));
            }
        }

        for (const auto& playlistId : playlistIds) {
            const auto trackIds = m_pTrackCollectionManager->internalCollection()
                                          ->getPlaylistDAO()
                                          .getTrackIds(playlistId);
            for (const auto& trackId : trackIds) {
                const auto location = m_pTrackCollectionManager->internalCollection()
                                              ->getTrackDAO()
                                              .getTrackLocation(trackId);
                trackRefs.insert(TrackRef::fromFilePath(location, trackId));
            }
        }
        m_trackRefs = QList<TrackRef>{trackRefs.begin(), trackRefs.end()};

        for (const auto& idAndName : m_pTrackCollectionManager->internalCollection()
                        ->getPlaylistDAO()
                        .getPlaylists(PlaylistDAO::PLHT_NOT_HIDDEN)) {
            if (playlistIds.contains(idAndName.first)) {
                m_playlistIdsAndNames.append(idAndName);
            }
        }
    }

    std::sort(m_trackRefs.begin(),
            m_trackRefs.end(),
            [](const TrackRef& lhs, const TrackRef& rhs) {
                return lhs.getId() < rhs.getId();
            });
    std::sort(m_playlistIdsAndNames.begin(),
            m_playlistIdsAndNames.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.first < rhs.first;
            });

    if (m_trackRefs.isEmpty()) {
        m_lastErrorMessage = QStringLiteral("No tracks were selected for export.");
    }
}

void RekordboxExportJob::loadTrack(const TrackRef& trackRef) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);
    m_lastErrorMessage.clear();

    m_pLastLoadedTrack = m_pTrackCollectionManager->getTrackByRef(trackRef);
    if (!m_pLastLoadedTrack) {
        m_lastErrorMessage = QStringLiteral("Could not load track %1.")
                                     .arg(trackRef.getLocation());
        return;
    }

    auto& analysisDao = m_pTrackCollectionManager->internalCollection()->getAnalysisDAO();
    const auto waveformAnalyses = analysisDao.getAnalysesForTrackByType(
            m_pLastLoadedTrack->getId(), AnalysisDao::TYPE_WAVEFORM);
    if (!waveformAnalyses.isEmpty()) {
        m_pLastLoadedWaveform.reset(
                WaveformFactory::loadWaveformFromAnalysis(waveformAnalyses.first()));
    } else {
        m_pLastLoadedWaveform.reset();
    }
}

void RekordboxExportJob::loadPlaylist(int playlistId) {
    DEBUG_ASSERT_QOBJECT_THREAD_AFFINITY(m_pTrackCollectionManager);
    m_lastLoadedPlaylistTrackIds =
            m_pTrackCollectionManager->internalCollection()
                    ->getPlaylistDAO()
                    .getTrackIdsInPlaylistOrder(playlistId);
}

void RekordboxExportJob::run() {
    try {
        if (!m_pRequest->deviceRoot.exists()) {
            fail(QStringLiteral("The export directory %1 does not exist.")
                            .arg(m_pRequest->deviceRoot.absolutePath()));
        }
        const QString rootPath = m_pRequest->deviceRoot.absolutePath();
        // shortcut: the job refuses a non-empty export tree rather than merging
        // into or replacing an existing library; PR 4 owns the overwrite choice.
        if (QFileInfo::exists(QDir(rootPath).filePath(QStringLiteral("PIONEER"))) ||
                QFileInfo::exists(QDir(rootPath).filePath(QStringLiteral("Contents")))) {
            fail(QStringLiteral(
                    "The export directory already contains PIONEER or Contents. "
                    "Choose an empty device or erase the existing rekordbox export."));
        }

        QTemporaryDir stage(
                QDir(rootPath).filePath(QStringLiteral(".paramix-export-XXXXXX")));
        if (!stage.isValid()) {
            fail(QStringLiteral("Could not create a staging directory in %1.").arg(rootPath));
        }

        QMetaObject::invokeMethod(this,
                "loadIds",
                Qt::BlockingQueuedConnection,
                Q_ARG(QSet<CrateId>, m_pRequest->crateIdsToExport),
                Q_ARG(QSet<int>, m_pRequest->playlistIdsToExport));
        if (!m_lastErrorMessage.isEmpty()) {
            fail(m_lastErrorMessage);
        }

        qint64 requiredBytes = kFreeSpaceSlackBytes;
        for (const TrackRef& trackRef : std::as_const(m_trackRefs)) {
            const QFileInfo source(trackRef.getLocation());
            if (!source.exists()) {
                fail(QStringLiteral("The source file %1 does not exist.")
                                .arg(source.absoluteFilePath()));
            }
            requiredBytes += source.size();
        }
        const QStorageInfo storage(rootPath);
        if (storage.isValid() && storage.isReady() &&
                storage.bytesAvailable() < requiredBytes) {
            fail(QStringLiteral("Not enough free space in %1: need %2 MiB, have %3 MiB.")
                            .arg(rootPath)
                            .arg(requiredBytes / (1024 * 1024))
                            .arg(storage.bytesAvailable() / (1024 * 1024)));
        }

        emit jobMaximum(static_cast<int>(m_trackRefs.size() +
                m_playlistIdsAndNames.size() + 3));
        int progress = 0;
        emit jobProgress(progress);

        const QDir stageDir(stage.path());
        QDir().mkpath(stageDir.filePath(QStringLiteral("PIONEER/rekordbox")));
        QDir().mkpath(stageDir.filePath(QStringLiteral("Contents")));

        QVector<RekordboxTrackInfo> trackInfos;
        QHash<TrackId, quint32> trackIds;
        trackInfos.reserve(m_trackRefs.size());

        quint32 nextTrackId = 1;
        for (const TrackRef& trackRef : std::as_const(m_trackRefs)) {
            QMetaObject::invokeMethod(this,
                    "loadTrack",
                    Qt::BlockingQueuedConnection,
                    Q_ARG(TrackRef, trackRef));
            if (!m_lastErrorMessage.isEmpty()) {
                fail(m_lastErrorMessage);
            }
            if (m_cancellationRequested.loadAcquire() != 0) {
                throw ExportCancelled{};
            }

            const TrackPointer pTrack = m_pLastLoadedTrack;
            if (!kSupportedFileTypes.contains(pTrack->getType().toLower())) {
                qInfo() << "Skipping" << pTrack->getFileInfo().fileName()
                        << "because rekordbox does not support" << pTrack->getType();
                ++progress;
                emit jobProgress(progress);
                continue;
            }
            const quint32 trackId = nextTrackId++;
            trackIds.insert(pTrack->getId(), trackId);

            const QFileInfo source(pTrack->getFileInfo().location());
            const QString relativePath = trackRelativePath(pTrack->getArtist(),
                    pTrack->getAlbum(),
                    source.fileName(),
                    trackId);
            const QString destination = stageDir.filePath(relativePath);
            if (!QDir().mkpath(QFileInfo(destination).path())) {
                fail(QStringLiteral("Could not create %1.").arg(QFileInfo(destination).path()));
            }
            if (!QFile::copy(source.absoluteFilePath(), destination)) {
                fail(QStringLiteral("Could not copy %1 to %2.")
                                .arg(source.absoluteFilePath(), destination));
            }

            const QString analysisDir =
                    stageDir.filePath(analysisMediaDir(trackId).mid(1));
            if (!QDir().mkpath(analysisDir)) {
                fail(QStringLiteral("Could not create %1.").arg(analysisDir));
            }

            const auto sampleRate = pTrack->getSampleRate();
            const auto endPosition = sampleRate.isValid()
                    ? audio::FramePos(pTrack->getDuration() * sampleRate.value())
                    : audio::FramePos();
            QVector<AnlzBeat> beats;
            if (const auto pBeats = pTrack->getBeats()) {
                beats = beatsFromBeats(*pBeats, endPosition);
            }
            QVector<AnlzCue> cues = cuesFromCues(pTrack->getCuePoints(), sampleRate);
            const int timingOffsetMs = mp3TimingOffsetMs(source.absoluteFilePath());
            addTimingOffset(&beats, &cues, timingOffsetMs);

            QVector<BandColumn> columns;
            const quint32 durationMs = static_cast<quint32>(std::min(
                    std::round(pTrack->getDuration() * 1000.0),
                    static_cast<double>(std::numeric_limits<quint32>::max())));
            if (m_pLastLoadedWaveform) {
                columns = columnsForDuration(
                        columnsFromWaveform(*m_pLastLoadedWaveform), durationMs);
            }
            const AnalysisFiles files = AnlzWriter::author(
                    mediaPath(relativePath), beats, cues, columns);
            writeFile(QDir(analysisDir).filePath(QStringLiteral("ANLZ0000.DAT")), files.dat);
            writeFile(QDir(analysisDir).filePath(QStringLiteral("ANLZ0000.EXT")), files.ext);
            writeFile(QDir(analysisDir).filePath(QStringLiteral("ANLZ0000.2EX")), files.twoEx);

            RekordboxTrackInfo info;
            info.id = trackId;
            info.title = pTrack->getTitle();
            info.artist = pTrack->getArtist();
            info.albumArtist = pTrack->getAlbumArtist();
            info.album = pTrack->getAlbum();
            info.genre = pTrack->getGenre();
            info.keyText = pTrack->getKeyText();
            info.comment = pTrack->getComment();
            info.filename = source.fileName();
            info.mediaPath = mediaPath(relativePath);
            info.analyzePath = analysisMediaPath(trackId);
            info.durationSec = static_cast<quint32>(
                    std::max(0, pTrack->getDurationSecondsInt()));
            info.tempoX100 = static_cast<quint32>(
                    std::max(0.0, std::round(pTrack->getBpm() * 100.0)));
            info.year = pTrack->getYear().toUShort();
            info.trackNumber = pTrack->getTrackNumber().toUShort();
            info.playCount = static_cast<quint16>(std::min(
                    pTrack->getTimesPlayed(), static_cast<int>(std::numeric_limits<quint16>::max())));
            info.rating = static_cast<quint8>(
                    std::clamp(pTrack->getRating(), 0, 5));
            info.bitrate = static_cast<quint32>(
                    std::max(0, pTrack->getBitrate()));
            info.sampleRate = static_cast<quint32>(
                    std::max(0, static_cast<int>(sampleRate.value())));
            info.fileSize = static_cast<quint32>(std::min<qint64>(
                    source.size(), std::numeric_limits<quint32>::max()));
            info.dateAdded = pTrack->getDateAdded().isValid()
                    ? pTrack->getDateAdded().toString(QStringLiteral("yyyy-MM-dd"))
                    : QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
            const QDateTime created = source.birthTime().isValid()
                    ? source.birthTime()
                    : source.lastModified();
            info.dateCreated = created.toString(QStringLiteral("yyyy-MM-dd"));
            trackInfos.append(info);

            ++progress;
            emit jobProgress(progress);
        }

        QVector<RekordboxPlaylistInfo> playlists;
        if (trackInfos.isEmpty()) {
            fail(QStringLiteral(
                    "None of the selected tracks use a file type that rekordbox supports."));
        }
        for (const auto& idAndName : std::as_const(m_playlistIdsAndNames)) {
            if (m_cancellationRequested.loadAcquire() != 0) {
                throw ExportCancelled{};
            }
            QMetaObject::invokeMethod(this,
                    "loadPlaylist",
                    Qt::BlockingQueuedConnection,
                    Q_ARG(int, idAndName.first));
            const auto& trackIdsInPlaylist = m_lastLoadedPlaylistTrackIds;
            RekordboxPlaylistInfo playlist;
            playlist.name = idAndName.second;
            for (const TrackId& trackId : trackIdsInPlaylist) {
                if (const auto it = trackIds.constFind(trackId); it != trackIds.constEnd()) {
                    playlist.trackIds.append(*it);
                }
            }
            playlists.append(playlist);
            ++progress;
            emit jobProgress(progress);
        }

        QString deviceName = m_pRequest->deviceName;
        if (deviceName.isEmpty()) {
            deviceName = storage.displayName();
        }
        if (deviceName.isEmpty()) {
            deviceName = QStringLiteral("PARAMIX");
        }
        const PdbLibrary library = buildPdbLibrary(
                trackInfos, playlists, deviceName, QDate::currentDate());
        writeFile(stageDir.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb")),
                PdbWriter::write(library));

        ++progress;
        emit jobProgress(progress);

        QDir rootDir(rootPath);
        if (!rootDir.rename(stageDir.filePath(QStringLiteral("PIONEER")),
                    rootDir.filePath(QStringLiteral("PIONEER")))) {
            fail(QStringLiteral("Could not move PIONEER into %1.").arg(rootPath));
        }
        if (!rootDir.rename(stageDir.filePath(QStringLiteral("Contents")),
                    rootDir.filePath(QStringLiteral("Contents")))) {
            QDir(rootDir.filePath(QStringLiteral("PIONEER"))).removeRecursively();
            fail(QStringLiteral("Could not move Contents into %1.").arg(rootPath));
        }

        ++progress;
        emit jobProgress(progress);
        emit completed(static_cast<int>(trackInfos.size()),
                static_cast<int>(playlists.size()));
    } catch (const ExportCancelled&) {
        qInfo() << "Rekordbox export cancelled";
    } catch (const std::exception& e) {
        qWarning() << "Rekordbox export failed:" << e.what();
        m_lastErrorMessage = QString::fromUtf8(e.what());
        emit failed(m_lastErrorMessage);
    }
}

void RekordboxExportJob::slotCancel() {
    m_cancellationRequested = 1;
}

} // namespace rekordbox
} // namespace mixxx
