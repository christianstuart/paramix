#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <atomic>
#include <memory>

#include "waveform/stemdetail/stemdetailtrack.h"
#include "waveform/stemdetail/stempagekey.h"

class QThread;
class StemPageBuilder;

/// Process-wide cache of rendered stem picture pages.
///
/// `lookup()` never blocks on page generation and may return null while a page
/// is being built. `request()` schedules a build once per key on a worker
/// thread and is safe to call from the render thread. Completed pages are
/// inserted on the cache's thread and recycled with an LRU limit.
class StemPageCache final : public QObject {
    Q_OBJECT
  public:
    using ImagePtr = std::shared_ptr<const QImage>;

    explicit StemPageCache(QObject* parent = nullptr);
    ~StemPageCache() override;

    /// Returns a cached page and marks it recently used. Thread-safe.
    ImagePtr lookup(const StemPageKey& key) const;

    /// Returns a cached page or schedules a build. Thread-safe.
    ImagePtr request(const StemPageKey& key,
            std::shared_ptr<const StemDetailTrack> detail);

    /// Suspends starting new page builds. Already running builds finish.
    void setBackgroundSuspended(bool suspended);

    struct Stats {
        quint64 hits = 0;
        quint64 misses = 0;
        quint64 builds = 0;
        quint64 failed = 0;
        int cachedPages = 0;
        int pendingPages = 0;
    };
    Stats stats() const;

  private slots:
    void onBuilt(const StemPageKey& key, const QImage& image);

  private:
    bool findUnlocked(const StemPageKey& key, ImagePtr* pImage) const;
    void insert(const StemPageKey& key, const ImagePtr& image);
    void touchUnlocked(const StemPageKey& key) const;

    static constexpr int kMaxCachedPages = 16;

    mutable QMutex m_mutex;
    mutable QHash<StemPageKey, ImagePtr> m_images;
    mutable QList<StemPageKey> m_order;
    QSet<StemPageKey> m_pending;
    QSet<StemPageKey> m_failed;

    QThread* m_pThread;
    StemPageBuilder* m_pBuilder;
    std::atomic<bool> m_backgroundSuspended{false};
    mutable std::atomic<quint64> m_hits{0};
    mutable std::atomic<quint64> m_misses{0};
    std::atomic<quint64> m_builds{0};
    std::atomic<quint64> m_failedCount{0};
};
