#include "qml/asyncimageprovider.h"

#include <QCache>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <algorithm>

#include "library/coverartcache.h"
#include "moc_asyncimageprovider.cpp"
#include "track/track.h"

namespace {
const QString kCoverArtPrefix = QStringLiteral("coverart/");

// Bounded, thread-safe cache of decoded (and already scaled) cover art.
// The provider is asked for the same track from several cells and while the
// user scrolls back and forth; without this every request re-opened and
// re-decoded the image.
constexpr int kCoverArtCacheBytes = 32 * 1024 * 1024;
QMutex s_coverArtCacheMutex;
QCache<QString, QImage> s_coverArtCache(kCoverArtCacheBytes);

QString coverArtCacheKey(const QString& id,
        const QSize& requestedSize,
        const QString& trackLocation) {
    QString key = id;
    if (!requestedSize.isValid()) {
        key += QStringLiteral("@full");
    } else {
        key += QLatin1Char('@') + QString::number(requestedSize.width()) +
                QLatin1Char('x') + QString::number(requestedSize.height());
    }
    // Embedded cover art changes when the track file is rewritten (tag edit).
    // Fold the file modification time into the key so a metadata/cover edit
    // does not serve the old picture for the rest of the session. Separate
    // cover image files still need a restart; the common embedded case is
    // handled without a database lookup.
    const QFileInfo trackInfo(trackLocation);
    if (trackInfo.exists()) {
        key += QLatin1Char('@') +
                QString::number(trackInfo.lastModified().toMSecsSinceEpoch());
    }
    return key;
}
} // namespace

namespace mixxx {

namespace qml {

AsyncImageResponse::AsyncImageResponse(
        QString id,
        QSize requestedSize,
        std::shared_ptr<TrackCollectionManager> pTrackCollectionManager)
        : m_id(std::move(id)),
          m_requestedSize(requestedSize),
          m_pTrackCollectionManager(std::move(pTrackCollectionManager)) {
    setAutoDelete(false);
}

QQuickTextureFactory* AsyncImageResponse::textureFactory() const {
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void AsyncImageResponse::run() {
    if (!m_id.startsWith(kCoverArtPrefix)) {
        qWarning() << "ImageProvider: Unsupported ID" << m_id;
        emit finished();
        return;
    }
    const QString trackLocation = AsyncImageProvider::coverArtUrlIdToTrackLocation(m_id);
    const QString cacheKey =
            coverArtCacheKey(m_id, m_requestedSize, trackLocation);
    {
        QMutexLocker locker(&s_coverArtCacheMutex);
        if (const QImage* pCachedImage = s_coverArtCache.object(cacheKey)) {
            m_image = *pCachedImage;
            emit finished();
            return;
        }
    }

    const auto trackRef = TrackRef::fromFilePath(trackLocation);

    // TODO: Only load CoverInfo from TrackCollectionManager
    // instead of the entire, stateful track object.
    TrackPointer pTrack;
    if (QThread::currentThread() != m_pTrackCollectionManager->thread()) {
        QMetaObject::invokeMethod(
                m_pTrackCollectionManager.get(),
                [this, trackRef] {
                    return m_pTrackCollectionManager->getTrackByRef(trackRef);
                },
                // This invocation will block the current thread!
                Qt::BlockingQueuedConnection,
                &pTrack);
    } else {
        pTrack = m_pTrackCollectionManager->getTrackByRef(trackRef);
    }
    if (!pTrack) {
        qWarning() << "ImageProvider: Failed to load track" << trackRef;
        emit finished();
        return;
    }
    const auto coverInfo = CoverInfo(pTrack->getCoverInfo(), trackLocation);
    // Release the track reference asap, i.e. before loading the image
    pTrack.reset();

    const CoverInfo::LoadedImage loadedImage = coverInfo.loadImage();
    switch (loadedImage.result) {
    case CoverInfo::LoadedImage::Result::NoImage:
        break;
    case CoverInfo::LoadedImage::Result::Ok:
        DEBUG_ASSERT(!loadedImage.image.isNull());
        if (m_requestedSize.isValid()) {
            // Keep the source aspect so the QML fillMode crops instead of
            // painting a pre-squashed thumbnail.
            m_image = loadedImage.image.scaled(
                    m_requestedSize, Qt::KeepAspectRatioByExpanding);
        } else {
            m_image = loadedImage.image;
        }
        break;
    default:
        qWarning() << "ImageProvider: Failed to load cover art" << trackRef;
        break;
    }

    if (!m_image.isNull()) {
        const int cacheCost = static_cast<int>(std::clamp<qsizetype>(
                m_image.sizeInBytes(),
                1,
                static_cast<qsizetype>(kCoverArtCacheBytes)));
        QMutexLocker locker(&s_coverArtCacheMutex);
        s_coverArtCache.insert(cacheKey, new QImage(m_image), cacheCost);
    }

    emit finished();
}

AsyncImageProvider::AsyncImageProvider(
        std::shared_ptr<TrackCollectionManager> pTrackCollectionManager)
        : QQuickAsyncImageProvider(),
          m_pTrackCollectionManager(pTrackCollectionManager) {
    // Each response blocks its pool thread while it asks the track collection
    // manager on the main thread for the track. Keep that pool small so a fast
    // scroll cannot queue dozens of blocked threads.
    pool.setMaxThreadCount(std::clamp(QThread::idealThreadCount() / 2, 2, 6));
}

QQuickImageResponse* AsyncImageProvider::requestImageResponse(
        const QString& id, const QSize& requestedSize) {
    AsyncImageResponse* response = new AsyncImageResponse(
            id, requestedSize, m_pTrackCollectionManager);
    pool.start(response);
    return response;
}

// static
const QString AsyncImageProvider::kProviderName = QStringLiteral("mixxx");

// static
QUrl AsyncImageProvider::trackLocationToCoverArtUrl(const QString& trackLocation) {
    QUrl url("image://" + kProviderName + "/" + kCoverArtPrefix);
    return url.resolved(
            QString::fromLatin1(trackLocation.toUtf8().toBase64(
                    QByteArray::Base64UrlEncoding)));
}

// static
QString AsyncImageProvider::coverArtUrlIdToTrackLocation(const QString& coverArtUrlId) {
    return QString::fromUtf8(QByteArray::fromBase64(
            coverArtUrlId.mid(kCoverArtPrefix.size()).toLatin1(), QByteArray::Base64UrlEncoding));
}

} // namespace qml
} // namespace mixxx
