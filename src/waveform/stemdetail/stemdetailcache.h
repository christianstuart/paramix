#pragma once

#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <atomic>
#include <memory>

#include "track/track_decl.h"
#include "waveform/stemdetail/stemdetailtrack.h"

class QThread;
class StemDetailBuilder;

/// Process-wide cache of reduced stem detail tracks.
///
/// request() never blocks: it returns the current picture (or the previous
/// zoom for the same track) and schedules a debounced background build. At
/// most two tracks are kept; the least recently used entry is dropped.
class StemDetailCache final : public QObject {
    Q_OBJECT

  public:
    explicit StemDetailCache(QObject* parent = nullptr);
    ~StemDetailCache() override;

    /// Returns the best available picture for this viewport. May return null
    /// while the first build for a track is still running.
    std::shared_ptr<const StemDetailTrack> request(const TrackPointer& pTrack,
            int samplesPerColumn,
            int height,
            float magnification,
            bool mixdown = false);

    /// Suspends or resumes background builds on behalf of the audio-overload
    /// coordinator. Suspension only prevents new builds from starting: a build
    /// that is already running is allowed to finish, and a deferred or newly
    /// requested one stays pending and starts exactly once after resume().
    /// Must be called from the cache's thread (the GUI thread).
    void setBackgroundSuspended(bool suspended);

    /// False only when every build for this track failed and no picture of it
    /// exists; signal renderers then draw the track instead.
    bool mayDrawTrack(TrackId trackId) const;

    /// True while a build is pending or running. Lock-free, for diagnostics.
    bool isBuilding() const {
        return m_activity.load(std::memory_order_relaxed);
    }

  signals:
    /// Emitted on the GUI thread when a new picture has been inserted.
    void detailReady();

  private slots:
    void startPendingBuild();
    void onBuilt(const StemDetailKey& key, std::shared_ptr<const StemDetailTrack> detail);
    void onPartialBuilt(const StemDetailKey& key, std::shared_ptr<const StemDetailTrack> detail);

  private:
    /// Locks the LRU and the failed-key set. request() may run on a render
    /// thread, while timer/build callbacks run on the cache's own thread.
    bool find(const StemDetailKey& key, std::shared_ptr<const StemDetailTrack>* pDetail);
    bool findUnlocked(const StemDetailKey& key,
            std::shared_ptr<const StemDetailTrack>* pDetail);
    void insert(const StemDetailKey& key,
            const std::shared_ptr<const StemDetailTrack>& pDetail);
    void scheduleBuild(const StemDetailKey& key, const TrackPointer& pTrack);

    static constexpr int kMaxEntries = 2;
    static constexpr int kDebounceMilliseconds = 250;
    /// A pregain or lane-height change keeps the same shape of picture and
    /// only re-renders magnitudes. Wait longer so a knob sweep or a live
    /// resize does not decode the whole file several times.
    static constexpr int kShapeDebounceMilliseconds = 800;

    mutable QMutex m_mutex;
    QThread* m_pThread;
    StemDetailBuilder* m_pBuilder;
    QTimer m_debounce;
    StemDetailKey m_pendingKey;
    TrackPointer m_pendingTrack;
    bool m_hasPending = false;
    bool m_buildInFlight = false;
    StemDetailKey m_inFlightKey;
    /// Last key for which a build was queued. request() is called every frame
    /// while a build is running; without this, every frame would post another
    /// queued invocation to the cache thread.
    StemDetailKey m_lastScheduledKey;
    bool m_hasLastScheduledKey = false;
    /// Set by the audio-overload coordinator; read on the cache thread.
    std::atomic<bool> m_backgroundSuspended{false};
    /// True while a build is queued or running, for the diagnostics line.
    std::atomic<bool> m_activity{false};
    QList<StemDetailKey> m_order;
    QList<std::shared_ptr<const StemDetailTrack>> m_details;
    QSet<StemDetailKey> m_failed;
    /// Opening of the build in flight, until the complete picture replaces it.
    StemDetailKey m_partialKey;
    std::shared_ptr<const StemDetailTrack> m_pPartial;
};
