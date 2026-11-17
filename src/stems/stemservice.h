#pragma once

#include <QMap>
#include <QMetaType>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QThread>
#include <QWaitCondition>
#include <atomic>

#include "preferences/usersettings.h"
#include "stems/stemfileformat.h"
#include "track/track_decl.h"
#include "track/trackid.h"
#include "util/db/dbconnectionpool.h"
#include "util/singleton.h"

class StemSetDao;
class TrackCollectionManager;

namespace mixxx::stems {

class StemSeparator;

/// One queued export. The set is written to `outputDirectory/<setName>`.
struct StemExportRequest {
    TrackPointer track;
    /// Parent directory of the set. Empty selects a default below the
    /// settings directory.
    QString outputDirectory;
    /// Directory name of the set. Empty derives a name from the track title.
    QString setName;
};

/// Low-priority background worker that separates tracks and writes native
/// stem sets. Owns the model session and runs the whole-track loop, so it is
/// never called from the real-time audio thread.
class StemService : public QThread, public Singleton<StemService> {
    Q_OBJECT

  public:
    enum class Status {
        NotSeparated,
        Queued,
        Separating,
        Ready,
        MissingStemFile,
        Failed,
    };
    Q_ENUM(Status)

    static StemService* createInstance(
            UserSettingsPointer pConfig, mixxx::DbConnectionPoolPtr pDbConnectionPool);

    ~StemService() override;

    /// Appends a job to the queue and starts the worker if needed. Thread-safe.
    bool queueExport(const StemExportRequest& request);

    /// Track ids with an exported stem set recorded in the database. Used by
    /// the library to show a stems badge for tracks whose stem metadata has
    /// not been loaded into the track cache yet. One query, GUI thread.
    QSet<TrackId> trackIdsWithStemSets() const;

    /// Re-queues jobs that were still pending when the app exited. Must be
    /// called on the main thread, because it resolves tracks by id.
    void restoreQueuedJobs(TrackCollectionManager* pTrackCollectionManager);

    /// Deletes the exported sets recorded for a track and forgets them. Only
    /// directories that contain a `stem-set.json` are removed.
    void removeStems(TrackId trackId);

    /// Stops dequeuing new jobs. A running job pauses at its next block.
    /// This is the explicit/user pause; it composes with the background pause
    /// below.
    void pause();
    void resume();
    bool isPaused() const;

    /// Pauses or resumes separation on behalf of a background coordinator,
    /// e.g. while the audio engine is overloaded. A background resume never
    /// clears an explicit pause() and vice versa. Thread-safe.
    void setBackgroundPaused(bool paused);

    /// True while a job is queued or separating.
    bool hasPendingWork() const;

    /// Priority of the separation worker thread.
    static constexpr QThread::Priority kWorkerPriority = QThread::LowPriority;

    /// Cancels the running job and clears the queue. The current output is
    /// discarded. Thread-safe.
    void cancelAll();

    /// Last known status of a track. `NotSeparated` when the service has never
    /// seen it.
    Status statusForTrack(TrackId trackId) const;

    /// Message attached to the last status change, if any.
    QString lastMessageForTrack(TrackId trackId) const;

  signals:
    /// `progress` is a percentage in [0, 100].
    void trackStatusChanged(TrackId trackId,
            mixxx::stems::StemService::Status status,
            int progress,
            const QString& message);

    /// The 8-channel playback cache for a track is ready, so waveform analysis
    /// can now produce the per-stem data.
    void stemsReady(TrackId trackId);

    /// Emitted when the service starts or stops having queued/running work.
    /// Used to yield CPU to stem separation and to resume it afterwards.
    void busyChanged(bool busy);

  protected:
    void run() override;

  private:
    struct Job {
        StemExportRequest request;
        TrackId trackId;
        // Resolved on the caller's thread so the worker never touches ConfigObject.
        QString outputDirectory;
        QString setName;
        QString modelPath;
        // True when the job should ask the runtime for a GPU provider.
        bool gpuAcceleration = false;
        StemExportSettings settings;
    };

    friend class Singleton<StemService>;

    StemService(UserSettingsPointer pConfig, mixxx::DbConnectionPoolPtr pDbConnectionPool);

    void processJob(Job& job,
            std::unique_ptr<StemSeparator>* ppSeparator,
            QString* pLoadedModelPath,
            QString* pLoadedDevice,
            StemSetDao& dao);
    void setStatus(
            TrackId trackId, Status status, int progress, const QString& message = QString());
    QString defaultOutputDirectory() const;
    QString defaultSetName(const TrackPointer& pTrack) const;

    const UserSettingsPointer m_pConfig;
    const mixxx::DbConnectionPoolPtr m_pDbConnectionPool;

    mutable QMutex m_mutex;
    QWaitCondition m_waitCondition;
    QQueue<Job> m_jobs;
    QMap<TrackId, Status> m_statuses;
    QMap<TrackId, QString> m_messages;
    bool m_stopRequested = false;
    // Effective pause state, read by the worker.
    std::atomic<bool> m_paused{false};
    // Pause owners. Kept separate so neither side can clear the other's pause.
    std::atomic<bool> m_userPaused{false};
    std::atomic<bool> m_backgroundPaused{false};
    // True while at least one job is queued or separating. Written under
    // m_mutex, read lock-free for cheap polling.
    std::atomic<bool> m_busy{false};
    std::atomic<bool> m_cancelRequested{false};
    // Set after a GPU provider fails mid-job. The process then stays on CPU
    // instead of paying the same failure on every later track (ADR-001 §2.3).
    std::atomic<bool> m_gpuDisabled{false};
};

} // namespace mixxx::stems

Q_DECLARE_METATYPE(mixxx::stems::StemService::Status)
