#include "qml/qmlstemserviceproxy.h"

#include <QFileInfo>
#include <QTimer>

#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "moc_qmlstemserviceproxy.cpp"
#include "preferences/configobject.h"
#include "qml/qmlconfigproxy.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmltrackproxy.h"
#include "stems/demucsonnxseparator.h"
#include "stems/stemservice.h"
#include "track/track.h"

namespace mixxx {
namespace qml {

namespace {

const ConfigKey kStemModelPathConfigKey("[Stems]", "ModelPath");

static_assert(static_cast<int>(mixxx::stems::StemService::Status::NotSeparated) ==
                QmlStemServiceProxy::NotSeparated);
static_assert(static_cast<int>(mixxx::stems::StemService::Status::Queued) ==
                QmlStemServiceProxy::Queued);
static_assert(static_cast<int>(mixxx::stems::StemService::Status::Separating) ==
                QmlStemServiceProxy::Separating);
static_assert(static_cast<int>(mixxx::stems::StemService::Status::Ready) ==
                QmlStemServiceProxy::Ready);
static_assert(static_cast<int>(mixxx::stems::StemService::Status::MissingStemFile) ==
                QmlStemServiceProxy::MissingStemFile);
static_assert(static_cast<int>(mixxx::stems::StemService::Status::Failed) ==
                QmlStemServiceProxy::Failed);

qint64 toQmlTrackId(TrackId trackId) {
    bool ok;
    const qint64 value = trackId.toVariant().toLongLong(&ok);
    return ok ? value : -1;
}

} // namespace

QmlStemServiceProxy::QmlStemServiceProxy(QObject* parent)
        : QObject(parent) {
    attachToService();
    m_hasModel = computeHasModel();
    // `hasModel` is evaluated live, but QML bindings must be invalidated when
    // the model is changed in the settings. The config proxy is a lazily
    // created QML singleton, so this only works if it already exists.
    if (auto* pConfigProxy = qobject_cast<QmlConfigProxy*>(QmlConfigProxyBase::s_pInstance)) {
        connect(pConfigProxy,
                &QmlConfigProxy::stemModelPathChanged,
                this,
                &QmlStemServiceProxy::refreshHasModel);
    }
}

// static
QmlStemServiceProxy* QmlStemServiceProxy::create(
        QQmlEngine* pQmlEngine, [[maybe_unused]] QJSEngine* pJsEngine) {
    return new QmlStemServiceProxy(pQmlEngine);
}

void QmlStemServiceProxy::scheduleAttachRetry() {
    if (m_pService || m_pAttachTimer) {
        return;
    }
    m_pAttachTimer = new QTimer(this);
    m_pAttachTimer->setInterval(1000);
    connect(m_pAttachTimer, &QTimer::timeout, this, &QmlStemServiceProxy::attachToService);
    m_pAttachTimer->start();
}

void QmlStemServiceProxy::attachToService() {
    if (m_pService) {
        if (m_pAttachTimer) {
            m_pAttachTimer->stop();
        }
        return;
    }
    m_pService = mixxx::stems::StemService::instance();
    if (!m_pService) {
        // The service is created by CoreServices. Getters fall back to safe
        // defaults until it exists.
        scheduleAttachRetry();
        return;
    }
    if (m_pAttachTimer) {
        m_pAttachTimer->stop();
    }
    // The service emits from its worker thread, so the state below must only
    // be touched on the GUI thread.
    connect(m_pService,
            &mixxx::stems::StemService::trackStatusChanged,
            this,
            [this](TrackId trackId,
                    mixxx::stems::StemService::Status status,
                    int progress,
                    const QString& message) {
                handleTrackStatusChanged(toQmlTrackId(trackId),
                        static_cast<int>(status),
                        progress,
                        message);
            },
            Qt::QueuedConnection);
    connect(m_pService,
            &mixxx::stems::StemService::stemsReady,
            this,
            [this](TrackId trackId) {
                handleStemsReady(toQmlTrackId(trackId));
            },
            Qt::QueuedConnection);
    // CoreServices destroys the service on shutdown. Do not keep a dangling
    // pointer around; the getters must stay safe.
    connect(m_pService, &QObject::destroyed, this, [this]() {
        m_pService = nullptr;
    });
}

bool QmlStemServiceProxy::isAvailable() const {
    return mixxx::stems::isDemucsOnnxAvailable();
}

bool QmlStemServiceProxy::computeHasModel() const {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pConfig) {
        return false;
    }
    const QString modelPath =
            pConfig->getValue(kStemModelPathConfigKey, QString()).trimmed();
    return !modelPath.isEmpty() && QFileInfo::exists(modelPath);
}

bool QmlStemServiceProxy::hasModel() const {
    return computeHasModel();
}

void QmlStemServiceProxy::refreshHasModel() {
    const bool hasModelNow = computeHasModel();
    if (m_hasModel == hasModelNow) {
        return;
    }
    m_hasModel = hasModelNow;
    emit hasModelChanged();
}

QString QmlStemServiceProxy::trackTitleForTrack(qint64 trackId) const {
    const Library* pLibrary = QmlLibraryProxy::get();
    TrackCollectionManager* pTrackCollectionManager =
            pLibrary ? pLibrary->trackCollectionManager() : nullptr;
    if (pTrackCollectionManager && trackId >= 0) {
        const TrackPointer pTrack = pTrackCollectionManager->getTrackById(
                TrackId(QVariant(trackId)));
        if (pTrack && !pTrack->getTitle().trimmed().isEmpty()) {
            return pTrack->getTitle();
        }
    }
    return m_titles.value(trackId);
}

void QmlStemServiceProxy::handleTrackStatusChanged(
        qint64 trackId, int status, int progress, const QString& message) {
    const int queuedBefore = queuedCount();
    const int activeBefore = activeCount();
    bool progressDirty = false;

    switch (static_cast<Status>(status)) {
    case Status::Queued:
        // The job left the running state, if it ever was there.
        m_separatingTracks.remove(trackId);
        m_queuedTracks.insert(trackId);
        break;
    case Status::Separating:
        // A queued job starts running: count it once, as part of the active
        // jobs, not as queued and active.
        m_queuedTracks.remove(trackId);
        m_separatingTracks.insert(trackId);
        if (m_currentTrackId != trackId) {
            m_currentTrackId = trackId;
            m_currentTrackTitle = trackTitleForTrack(trackId);
            m_progress = progress;
            progressDirty = true;
        } else if (m_progress != progress) {
            m_progress = progress;
            progressDirty = true;
        }
        break;
    default:
        // Any terminal state (ready, missing, failed, canceled) ends the job.
        m_queuedTracks.remove(trackId);
        m_separatingTracks.remove(trackId);
        if (m_currentTrackId == trackId) {
            m_currentTrackId = -1;
            m_currentTrackTitle.clear();
            m_progress = 0;
            progressDirty = true;
        }
        m_titles.remove(trackId);
        break;
    }

    if (queuedBefore != queuedCount() || activeBefore != activeCount()) {
        emit queueChanged();
    }
    if (progressDirty) {
        emit progressChanged();
    }
    refreshHasModel();
    emit trackStatusChanged(trackId, status, progress, message);
}

void QmlStemServiceProxy::handleStemsReady(qint64 trackId) {
    emit stemsReady(trackId);
}

bool QmlStemServiceProxy::generateStems(QmlTrackProxy* pTrack) {
    attachToService();
    if (!pTrack || !pTrack->internal() || !pTrack->internal()->getId().isValid()) {
        return false;
    }
    if (!m_pService) {
        return false;
    }
    const TrackPointer pInternalTrack = pTrack->internal();
    mixxx::stems::StemExportRequest request;
    request.track = pInternalTrack;
    const bool queued = m_pService->queueExport(request);
    if (queued) {
        m_titles.insert(toQmlTrackId(pInternalTrack->getId()),
                pInternalTrack->getTitle());
    }
    refreshHasModel();
    return queued;
}

void QmlStemServiceProxy::removeStems(QmlTrackProxy* pTrack) {
    attachToService();
    if (!pTrack || !pTrack->internal() || !m_pService) {
        return;
    }
    const TrackId trackId = pTrack->internal()->getId();
    if (!trackId.isValid()) {
        return;
    }
    m_pService->removeStems(trackId);
}

int QmlStemServiceProxy::statusForTrack(QmlTrackProxy* pTrack) {
    attachToService();
    if (!pTrack || !pTrack->internal() || !m_pService) {
        return -1;
    }
    const TrackId trackId = pTrack->internal()->getId();
    if (!trackId.isValid()) {
        return -1;
    }
    return static_cast<int>(m_pService->statusForTrack(trackId));
}

QString QmlStemServiceProxy::messageForTrack(QmlTrackProxy* pTrack) {
    attachToService();
    if (!pTrack || !pTrack->internal() || !m_pService) {
        return {};
    }
    const TrackId trackId = pTrack->internal()->getId();
    if (!trackId.isValid()) {
        return {};
    }
    return m_pService->lastMessageForTrack(trackId);
}

bool QmlStemServiceProxy::hasStems(QmlTrackProxy* pTrack) {
    return pTrack && pTrack->internal() &&
            !pTrack->internal()->getStemInfo().isEmpty();
}

void QmlStemServiceProxy::cancelAll() {
    attachToService();
    if (!m_pService) {
        return;
    }
    m_pService->cancelAll();
}

} // namespace qml
} // namespace mixxx
