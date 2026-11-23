#include "waveform/stemdetail/stempagecache.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>

#include "moc_stempagecache.cpp"
#include "waveform/stemdetail/stempagebuilder.h"

StemPageCache::StemPageCache(QObject* parent)
        : QObject(parent),
          m_pThread(new QThread()),
          m_pBuilder(new StemPageBuilder()) {
    qRegisterMetaType<StemPageKey>("StemPageKey");
    qRegisterMetaType<std::shared_ptr<const StemDetailTrack>>(
            "std::shared_ptr<const StemDetailTrack>");

    m_pThread->setObjectName(QStringLiteral("StemPageBuilder"));
    m_pBuilder->moveToThread(m_pThread);
    connect(m_pBuilder,
            &StemPageBuilder::built,
            this,
            &StemPageCache::onBuilt,
            Qt::QueuedConnection);
    m_pThread->start(QThread::LowPriority);
}

StemPageCache::~StemPageCache() {
    m_pThread->quit();
    m_pThread->wait();
    delete m_pBuilder;
    delete m_pThread;
}

bool StemPageCache::findUnlocked(const StemPageKey& key, ImagePtr* pImage) const {
    auto it = m_images.constFind(key);
    if (it == m_images.constEnd()) {
        return false;
    }
    *pImage = it.value();
    touchUnlocked(key);
    return true;
}

void StemPageCache::touchUnlocked(const StemPageKey& key) const {
    const int index = static_cast<int>(m_order.indexOf(key));
    if (index > 0) {
        m_order.move(index, 0);
    }
}

void StemPageCache::insert(const StemPageKey& key, const ImagePtr& image) {
    QMutexLocker locker(&m_mutex);
    const int index = static_cast<int>(m_order.indexOf(key));
    if (index >= 0) {
        m_images.insert(key, image);
        if (index > 0) {
            m_order.move(index, 0);
        }
    } else {
        m_order.prepend(key);
        m_images.insert(key, image);
    }
    while (m_order.size() > kMaxCachedPages) {
        m_images.remove(m_order.takeLast());
    }
}

StemPageCache::ImagePtr StemPageCache::lookup(const StemPageKey& key) const {
    ImagePtr image;
    {
        QMutexLocker locker(&m_mutex);
        if (findUnlocked(key, &image)) {
            m_hits.fetch_add(1, std::memory_order_relaxed);
            return image;
        }
    }
    m_misses.fetch_add(1, std::memory_order_relaxed);
    return {};
}

StemPageCache::ImagePtr StemPageCache::request(
        const StemPageKey& key,
        std::shared_ptr<const StemDetailTrack> detail) {
    if (!detail || detail->laneCount <= 0) {
        return {};
    }

    ImagePtr image;
    bool schedule = false;
    {
        QMutexLocker locker(&m_mutex);
        if (findUnlocked(key, &image)) {
            m_hits.fetch_add(1, std::memory_order_relaxed);
            return image;
        }
        if (m_pending.contains(key) || m_failed.contains(key) ||
                m_backgroundSuspended.load(std::memory_order_relaxed)) {
            return {};
        }
        m_pending.insert(key);
        schedule = true;
    }

    if (schedule) {
        const bool queued = QMetaObject::invokeMethod(m_pBuilder,
                "build",
                Qt::QueuedConnection,
                Q_ARG(StemPageKey, key),
                Q_ARG(std::shared_ptr<const StemDetailTrack>, detail));
        if (!queued) {
            QMutexLocker locker(&m_mutex);
            m_pending.remove(key);
        }
    }
    return {};
}

void StemPageCache::onBuilt(const StemPageKey& key, const QImage& image) {
    ImagePtr pImage;
    if (!image.isNull()) {
        pImage = std::make_shared<const QImage>(image);
        m_builds.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_failedCount.fetch_add(1, std::memory_order_relaxed);
    }
    {
        QMutexLocker locker(&m_mutex);
        m_pending.remove(key);
        if (!pImage) {
            m_failed.insert(key);
            return;
        }
    }
    insert(key, pImage);
}

void StemPageCache::setBackgroundSuspended(bool suspended) {
    m_backgroundSuspended.store(suspended, std::memory_order_relaxed);
}

StemPageCache::Stats StemPageCache::stats() const {
    Stats stats;
    stats.hits = m_hits.load(std::memory_order_relaxed);
    stats.misses = m_misses.load(std::memory_order_relaxed);
    stats.builds = m_builds.load(std::memory_order_relaxed);
    stats.failed = m_failedCount.load(std::memory_order_relaxed);
    QMutexLocker locker(&m_mutex);
    stats.cachedPages = m_images.size();
    stats.pendingPages = m_pending.size();
    return stats;
}
