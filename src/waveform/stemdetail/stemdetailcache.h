#pragma once

#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QTimer>
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
            float magnification);

  signals:
    /// Emitted on the GUI thread when a new picture has been inserted.
    void detailReady();

  private slots:
    void startPendingBuild();
    void onBuilt(const StemDetailKey& key, std::shared_ptr<const StemDetailTrack> detail);

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
    static constexpr int kDebounceMilliseconds = 150;

    mutable QMutex m_mutex;
    QThread* m_pThread;
    StemDetailBuilder* m_pBuilder;
    QTimer m_debounce;
    StemDetailKey m_pendingKey;
    TrackPointer m_pendingTrack;
    bool m_hasPending = false;
    bool m_buildInFlight = false;
    StemDetailKey m_inFlightKey;
    QList<StemDetailKey> m_order;
    QList<std::shared_ptr<const StemDetailTrack>> m_details;
    QSet<StemDetailKey> m_failed;
};
