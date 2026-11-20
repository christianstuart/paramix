#include "waveform/stemdetail/stemdetailcache.h"

#include <QMetaObject>
#include <QThread>

#include "moc_stemdetailcache.cpp"
#include "track/track.h"
#include "waveform/stemdetail/stemdetailbuilder.h"

StemDetailCache::StemDetailCache(QObject* parent)
        : QObject(parent),
          m_pThread(new QThread()),
          m_pBuilder(new StemDetailBuilder()) {
    qRegisterMetaType<StemDetailKey>("StemDetailKey");
    qRegisterMetaType<std::shared_ptr<const StemDetailTrack>>(
            "std::shared_ptr<const StemDetailTrack>");

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMilliseconds);
    connect(&m_debounce, &QTimer::timeout, this, &StemDetailCache::startPendingBuild);

    m_pThread->setObjectName(QStringLiteral("StemDetailBuilder"));
    m_pBuilder->moveToThread(m_pThread);
    connect(m_pBuilder,
            &StemDetailBuilder::built,
            this,
            &StemDetailCache::onBuilt,
            Qt::QueuedConnection);
    // Background work: keep it below the audio callback, the engine and the
    // GUI. The renderer keeps drawing the previous picture until a build is
    // ready, so a lower priority only delays the first stem waveform.
    m_pThread->start(QThread::LowPriority);
}

StemDetailCache::~StemDetailCache() {
    m_debounce.stop();
    m_pBuilder->cancel();
    m_pThread->quit();
    m_pThread->wait();
    delete m_pBuilder;
    delete m_pThread;
}

std::shared_ptr<const StemDetailTrack> StemDetailCache::request(
        const TrackPointer& pTrack,
        int samplesPerColumn,
        int height,
        float magnification) {
    if (!pTrack || !pTrack->getId().isValid() || samplesPerColumn <= 0 || height <= 0) {
        return nullptr;
    }

    StemDetailKey key;
    key.trackId = pTrack->getId();
    key.sampleRate = pTrack->getSampleRate().value();
    key.samplesPerColumn = samplesPerColumn;
    key.height = height;
    key.magnification = magnification;

    std::shared_ptr<const StemDetailTrack> pDetail;
    {
        QMutexLocker locker(&m_mutex);
        if (findUnlocked(key, &pDetail)) {
            return pDetail;
        }
        // Keep drawing the previous picture of the same track while the new
        // zoom level is built. Never show another track's picture.
        for (int i = 0; i < m_details.size(); ++i) {
            if (m_details.at(i)->trackId == key.trackId) {
                pDetail = m_details.at(i);
                break;
            }
        }
        if (m_failed.contains(key)) {
            return pDetail;
        }
        if (m_hasLastScheduledKey && m_lastScheduledKey == key) {
            // A build for this exact viewport is already queued or running.
            // request() runs on every frame, so do not post another queued
            // invocation until that build completes.
            return pDetail;
        }
        m_lastScheduledKey = key;
        m_hasLastScheduledKey = true;
    }

    // request() may run on a render thread. The debounce timer and the worker
    // belong to the cache's thread, so scheduling is marshalled there.
    QMetaObject::invokeMethod(
            this,
            [this, key, pTrack]() {
                scheduleBuild(key, pTrack);
            },
            Qt::QueuedConnection);
    return pDetail;
}

void StemDetailCache::setBackgroundSuspended(bool suspended) {
    if (m_backgroundSuspended.exchange(suspended) == suspended) {
        return;
    }
    if (suspended) {
        // A pending request keeps m_hasPending; only its timer is stopped.
        m_debounce.stop();
        return;
    }
    if (!m_hasPending) {
        return;
    }
    // Resume the deferred request exactly once. Restarting the debounce rather
    // than calling startPendingBuild() directly also covers the case where a
    // build was already running when the suspension was requested: that build
    // is allowed to finish instead of being cancelled and thrown away.
    m_debounce.stop();
    m_debounce.setInterval(kDebounceMilliseconds);
    m_debounce.start();
}

void StemDetailCache::scheduleBuild(const StemDetailKey& key,
        const TrackPointer& pTrack) {
    if ((m_hasPending && m_pendingKey == key) ||
            (m_buildInFlight && m_inFlightKey == key)) {
        return;
    }

    std::shared_ptr<const StemDetailTrack> pDetail;
    int debounceMs = kDebounceMilliseconds;
    {
        QMutexLocker locker(&m_mutex);
        if (findUnlocked(key, &pDetail)) {
            return;
        }
        for (const auto& pExisting : std::as_const(m_details)) {
            if (!pExisting || pExisting->trackId != key.trackId ||
                    pExisting->laneCount <= 0) {
                continue;
            }
            const StemDetailLane& lane = pExisting->lanes.front();
            if (lane.spc == key.samplesPerColumn &&
                    (lane.height != key.height ||
                            pExisting->magnification != key.magnification)) {
                debounceMs = kShapeDebounceMilliseconds;
            }
            break;
        }
        m_pendingKey = key;
        m_pendingTrack = pTrack;
        m_hasPending = true;
        m_activity.store(true, std::memory_order_relaxed);
    }
    m_debounce.stop();
    if (m_backgroundSuspended.load(std::memory_order_relaxed)) {
        // Keep the request pending; setBackgroundSuspended(false) starts it.
        return;
    }
    m_debounce.setInterval(debounceMs);
    m_debounce.start();
}

bool StemDetailCache::find(const StemDetailKey& key,
        std::shared_ptr<const StemDetailTrack>* pDetail) {
    QMutexLocker locker(&m_mutex);
    return findUnlocked(key, pDetail);
}

bool StemDetailCache::findUnlocked(const StemDetailKey& key,
        std::shared_ptr<const StemDetailTrack>* pDetail) {
    for (int i = 0; i < m_order.size(); ++i) {
        if (m_order.at(i) != key) {
            continue;
        }
        *pDetail = m_details.at(i);
        if (i > 0) {
            m_order.move(i, 0);
            m_details.move(i, 0);
        }
        return true;
    }
    return false;
}

void StemDetailCache::insert(const StemDetailKey& key,
        const std::shared_ptr<const StemDetailTrack>& pDetail) {
    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < m_order.size(); ++i) {
        if (m_order.at(i) != key) {
            continue;
        }
        m_details[i] = pDetail;
        if (i > 0) {
            m_order.move(i, 0);
            m_details.move(i, 0);
        }
        return;
    }
    m_order.prepend(key);
    m_details.prepend(pDetail);
    while (m_order.size() > kMaxEntries) {
        m_order.removeLast();
        m_details.removeLast();
    }
}

void StemDetailCache::startPendingBuild() {
    if (m_backgroundSuspended.load(std::memory_order_relaxed)) {
        // Keep the request pending; resume restarts the debounce.
        return;
    }
    if (!m_hasPending) {
        return;
    }

    std::shared_ptr<const StemDetailTrack> pDetail;
    if (find(m_pendingKey, &pDetail)) {
        m_hasPending = false;
        m_activity.store(false, std::memory_order_relaxed);
        emit detailReady();
        return;
    }

    if (m_buildInFlight) {
        // The running build is for a superseded key. The worker stops at its
        // next decoded chunk and drops the result.
        m_pBuilder->cancel();
    }
    m_inFlightKey = m_pendingKey;
    m_buildInFlight = true;
    m_hasPending = false;

    // Capture the post-cancel generation. A build that was queued before this
    // cancel carries an older token and exits before decoding anything.
    const quint64 generation = m_pBuilder->generation();
    QMetaObject::invokeMethod(m_pBuilder,
            "build",
            Qt::QueuedConnection,
            Q_ARG(StemDetailKey, m_inFlightKey),
            Q_ARG(TrackPointer, m_pendingTrack),
            Q_ARG(quint64, generation));
    m_pendingTrack.reset();
}

void StemDetailCache::onBuilt(const StemDetailKey& key,
        std::shared_ptr<const StemDetailTrack> pDetail) {
    if (!m_buildInFlight || key != m_inFlightKey) {
        // A cancelled build that finished before noticing the flag.
        return;
    }
    m_buildInFlight = false;
    if (!pDetail) {
        QMutexLocker locker(&m_mutex);
        if (m_hasLastScheduledKey && m_lastScheduledKey == key) {
            m_hasLastScheduledKey = false;
        }
        if (m_failed.size() > 16) {
            m_failed.clear();
        }
        m_failed.insert(key);
        m_activity.store(m_hasPending, std::memory_order_relaxed);
        return;
    }
    insert(key, pDetail);
    {
        QMutexLocker locker(&m_mutex);
        if (m_hasLastScheduledKey && m_lastScheduledKey == key) {
            m_hasLastScheduledKey = false;
        }
    }
    m_activity.store(m_hasPending, std::memory_order_relaxed);
    emit detailReady();
}
