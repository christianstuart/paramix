#include "qml/qmllibrarytracklistmodel.h"

#include <qnamespace.h>

#include <QObject>
#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVariant>
#include <QVector>

#include "library/basetracktablemodel.h"
#include "library/columncache.h"
#include "library/proxytrackmodel.h"
#include "moc_qmllibrarytracklistmodel.cpp"
#include "qml/asyncimageprovider.h"
#include "qml/qmllibrarytracklistcolumn.h"
#include "qml_owned_ptr.h"
#include "qmltrackproxy.h"
#include "stems/stemcache.h"
#include "stems/stemservice.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/logger.h"
#include "util/parented_ptr.h"

namespace mixxx {
namespace qml {
namespace {
const mixxx::Logger kLogger("QmlLibraryTrackListModel");

const QHash<int, QByteArray> kRoleNames = {
        {Qt::DisplayRole, "display"},
        {Qt::DecorationRole, "decoration"},
        {QmlLibraryTrackListModel::Delegate, "delegate"},
        {QmlLibraryTrackListModel::Track, "track"},
        {QmlLibraryTrackListModel::FileURL, "file_url"},
        {QmlLibraryTrackListModel::CoverArt, "cover_art"},
        {QmlLibraryTrackListModel::HasStems, "has_stems"},
};

QColor colorFromRgbCode(double colorValue) {
    if (colorValue < 0 || colorValue > 0xFFFFFF) {
        return {};
    }

    QRgb rgbValue = static_cast<QRgb>(colorValue) | 0xFF000000;
    return QColor(rgbValue);
}

constexpr int kStemServiceRetryIntervalMs = 1000;
constexpr int kStemServiceMaxRetryCount = 30;
} // namespace

QmlLibraryTrackListModel::QmlLibraryTrackListModel(
        const QList<QmlLibraryTrackListColumn*>& librarySource,
        QAbstractItemModel* pModel,
        QObject* pParent)
        : QIdentityProxyModel(pParent),
          m_columns() {
    m_columns.reserve(librarySource.size());
    for (const auto* pColumn : std::as_const(librarySource)) {
        m_columns.emplace_back(make_parented<QmlLibraryTrackListColumn>(this,
                pColumn->label(),
                pColumn->fillSpan(),
                pColumn->columnIdx(),
                pColumn->preferredWidth(),
                pColumn->autoHideWidth(),
                pColumn->delegate(),
                pColumn->role()));
    }

    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel);
    VERIFY_OR_DEBUG_ASSERT(pTrackModel) {
        return;
    }
    pTrackModel->select();
    setSourceModel(pModel);

    m_stemServiceRetryTimer.setInterval(kStemServiceRetryIntervalMs);
    connect(&m_stemServiceRetryTimer,
            &QTimer::timeout,
            this,
            &QmlLibraryTrackListModel::ensureStemServiceConnected);
    ensureStemServiceConnected();

    // The cover column uses invokables instead of required model roles, so it
    // needs an explicit dependency to re-evaluate when a track's colour or
    // cover art changes.
    connect(this,
            &QAbstractItemModel::dataChanged,
            this,
            [this](const QModelIndex&, const QModelIndex&, const QVector<int>&) {
                bumpRevision();
            });
    connect(this,
            &QAbstractItemModel::modelReset,
            this,
            &QmlLibraryTrackListModel::bumpRevision);
    connect(this,
            &QAbstractItemModel::layoutChanged,
            this,
            [this](const QList<QPersistentModelIndex>&,
                    QAbstractItemModel::LayoutChangeHint) {
                bumpRevision();
            });
}

void QmlLibraryTrackListModel::ensureStemServiceConnected() {
    if (m_stemServiceConnected) {
        m_stemServiceRetryTimer.stop();
        return;
    }
    if (!mixxx::stems::StemService::isCreated()) {
        ++m_stemServiceRetryCount;
        if (m_stemServiceRetryCount >= kStemServiceMaxRetryCount) {
            m_stemServiceRetryTimer.stop();
            kLogger.debug() << "Giving up waiting for StemService";
            return;
        }
        if (!m_stemServiceRetryTimer.isActive()) {
            m_stemServiceRetryTimer.start();
        }
        return;
    }

    auto* pStemService = mixxx::stems::StemService::instance();
    if (!pStemService) {
        return;
    }
    m_stemServiceConnected = true;
    m_stemServiceRetryTimer.stop();
    m_stemTrackIds = pStemService->trackIdsWithStemSets();
    // The service emits from its worker thread. The receiver context (`this`)
    // disconnects automatically when the model is destroyed.
    connect(pStemService, &mixxx::stems::StemService::stemsReady, this, [this](TrackId trackId) {
                m_stemTrackIds.insert(trackId);
                refreshHasStems(trackId, true); }, Qt::QueuedConnection);
    connect(pStemService, &mixxx::stems::StemService::trackStatusChanged, this, [this](TrackId trackId, mixxx::stems::StemService::Status status, int /*progress*/, const QString& /*message*/) {
                // Only the transition to Ready can change has_stems.
                if (status == mixxx::stems::StemService::Status::Ready) {
                    m_stemTrackIds.insert(trackId);
                    refreshHasStems(trackId, false);
                } }, Qt::QueuedConnection);
}

void QmlLibraryTrackListModel::refreshHasStems(
        TrackId trackId, bool populateStemInfo) {
    if (!trackId.isValid()) {
        return;
    }
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());
    if (pTrackModel == nullptr) {
        return;
    }
    const int lastColumn = columnCount() - 1;
    if (lastColumn < 0) {
        return;
    }
    // The track model already maintains a track-id to row index. Use it
    // instead of walking every row for every finished separation job, which
    // turns a batch of jobs into quadratic work on large libraries. A
    // sorting/filtering ProxyTrackModel reports rows from its composed model,
    // so keep the row scan for that case.
    QVector<int> rows;
    if (dynamic_cast<ProxyTrackModel*>(pTrackModel) != nullptr) {
        for (int row = 0; row < rowCount(); ++row) {
            if (pTrackModel->getTrackId(sourceModel()->index(row, 0)) == trackId) {
                rows.append(row);
            }
        }
    } else {
        rows = pTrackModel->getTrackRows(trackId);
    }
    bool stemInfoPopulated = false;
    for (int row : rows) {
        if (populateStemInfo && !stemInfoPopulated) {
            const auto pTrack = pTrackModel->getTrack(sourceModel()->index(row, 0));
            if (pTrack && pTrack->getStemInfo().isEmpty()) {
                // The stem metadata is normally imported when the cached
                // source is opened. Separation finished in the background, so
                // attach it here to make the row and the HasStems role reflect
                // the new cache.
                pTrack->setStemInfo(mixxx::stems::StemCache::stemInfo());
            }
            stemInfoPopulated = true;
        }
        emit dataChanged(index(row, 0),
                index(row, lastColumn),
                QVector<int>{HasStems});
    }
}

void QmlLibraryTrackListModel::bumpRevision() {
    ++m_revision;
    emit revisionChanged();
}

QmlTrackProxy* QmlLibraryTrackListModel::trackProxyForTrack(
        const TrackPointer& pTrack) const {
    if (!pTrack) {
        return nullptr;
    }
    const TrackId trackId = pTrack->getId();
    if (trackId.isValid()) {
        const auto it = m_trackProxies.constFind(trackId);
        if (it != m_trackProxies.constEnd()) {
            QmlTrackProxy* pProxy = it.value().data();
            if (pProxy) {
                return pProxy;
            }
        }
    }

    auto pProxy = make_qml_owned<QmlTrackProxy>(pTrack);
    if (trackId.isValid()) {
        m_trackProxies.insert(trackId, QPointer<QmlTrackProxy>(pProxy.get()));
        // Sweeping the weak cache on every insert is quadratic once QML's GC
        // lags a fast scroll. Sweep periodically, and clear the weak entries
        // outright if the cache is still huge; sharing is re-established on
        // the next request.
        if (m_trackProxySweepCountdown-- <= 0) {
            m_trackProxySweepCountdown = 256;
            for (auto it = m_trackProxies.begin(); it != m_trackProxies.end();) {
                if (it.value().isNull()) {
                    it = m_trackProxies.erase(it);
                } else {
                    ++it;
                }
            }
            if (m_trackProxies.size() > 4096) {
                m_trackProxies.clear();
            }
        }
    }
    return pProxy.get();
}

QVariant QmlLibraryTrackListModel::data(const QModelIndex& proxyIndex, int role) const {
    if (!proxyIndex.isValid()) {
        return {};
    }

    VERIFY_OR_DEBUG_ASSERT(checkIndex(proxyIndex)) {
        return {};
    }
    auto columnIdx = proxyIndex.column();
    VERIFY_OR_DEBUG_ASSERT(columnIdx >= 0 || columnIdx < m_columns.size()) {
        return {};
    }

    auto* const pTrackTableModel = qobject_cast<BaseTrackTableModel*>(sourceModel());
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());

    const auto& pColumn = m_columns[columnIdx];

    switch (role) {
    case Track: {
        if (pTrackModel == nullptr) {
            return {};
        }
        const QModelIndex sourceIndex = QIdentityProxyModel::mapToSource(proxyIndex);
        // Most cells in a row request the same track. The id lookup lets the
        // cached proxy answer without loading the track again.
        const TrackId trackId = pTrackModel->getTrackId(sourceIndex);
        if (trackId.isValid()) {
            const auto it = m_trackProxies.constFind(trackId);
            if (it != m_trackProxies.constEnd() && !it.value().isNull()) {
                return QVariant::fromValue(it.value().data());
            }
        }
        const auto pTrack = pTrackModel->getTrack(sourceIndex);
        return QVariant::fromValue(trackProxyForTrack(pTrack));
    }
    case Qt::DecorationRole:
        return getDecorationColor(proxyIndex.row());
    case CoverArt:
        return getCoverArtUrl(proxyIndex.row());
    case FileURL: {
        if (pTrackModel == nullptr) {
            return {};
        }
        return pTrackModel->getTrackUrl(QIdentityProxyModel::mapToSource(proxyIndex));
    }
    case HasStems: {
        // The stem service status is intentionally not queried per cell,
        // because that may hit the database. The id lookup is cheaper than
        // loading the whole track, and most rows will already be in the set.
        if (pTrackModel == nullptr) {
            return false;
        }
        const TrackId trackId =
                pTrackModel->getTrackId(QIdentityProxyModel::mapToSource(proxyIndex));
        if (!trackId.isValid()) {
            return false;
        }
        if (m_stemTrackIds.contains(trackId)) {
            return true;
        }
        const auto pTrack = pTrackModel->getTrack(
                QIdentityProxyModel::mapToSource(proxyIndex));
        return pTrack && !pTrack->getStemInfo().isEmpty();
    }
    case Delegate:
        return QVariant::fromValue(pColumn->delegate());
        break;
    }
    if (pColumn->columnIdx() < 0) {
        // Use proxyIndex.column()
        return QIdentityProxyModel::data(proxyIndex, role);
    }
    return QIdentityProxyModel::data(
            proxyIndex.siblingAtColumn(pTrackTableModel != nullptr
                            ? pTrackTableModel->fieldIndex(
                                      static_cast<ColumnCache::Column>(
                                              pColumn->columnIdx()))
                            : pColumn->columnIdx()),
            role);
}

int QmlLibraryTrackListModel::columnCount(const QModelIndex& parent) const {
    VERIFY_OR_DEBUG_ASSERT(static_cast<QmlLibraryTrackListModel*>(
                                   parent.internalPointer()) != this) {
        return 0;
    }
    return m_columns.size();
}

QVariant QmlLibraryTrackListModel::headerData(
        int section, Qt::Orientation orientation, int role) const {
    VERIFY_OR_DEBUG_ASSERT(section >= 0 || section < m_columns.size()) {
        return {};
    }
    // TODO role
    return m_columns[section]->label();
}

QHash<int, QByteArray> QmlLibraryTrackListModel::roleNames() const {
    return kRoleNames;
}

QUrl QmlLibraryTrackListModel::getUrl(int row) const {
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());

    if (pTrackModel == nullptr) {
        // TODO search for column with role
        return {};
    }
    return pTrackModel->getTrackUrl(sourceModel()->index(row, 0));
}

QUrl QmlLibraryTrackListModel::getCoverArtUrl(int row) const {
    const QModelIndex rowIndex = index(row, 0);
    if (!rowIndex.isValid()) {
        return {};
    }
    auto* const pTrackTableModel = qobject_cast<BaseTrackTableModel*>(sourceModel());
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());

    QString location;
    if (pTrackTableModel != nullptr) {
        location = QIdentityProxyModel::data(
                rowIndex.siblingAtColumn(pTrackTableModel->fieldIndex(
                        ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION)),
                Qt::DisplayRole)
                           .toString();
    } else if (pTrackModel != nullptr) {
        const auto pTrack = pTrackModel->getTrack(rowIndex);
        if (!pTrack) {
            return {};
        }
        location = pTrack->getCoverInfo().coverLocation;
    }
    if (location.isEmpty()) {
        return {};
    }
    return AsyncImageProvider::trackLocationToCoverArtUrl(location);
}

QColor QmlLibraryTrackListModel::getDecorationColor(int row) const {
    const QModelIndex rowIndex = index(row, 0);
    if (!rowIndex.isValid()) {
        return {};
    }
    auto* const pTrackTableModel = qobject_cast<BaseTrackTableModel*>(sourceModel());
    if (pTrackTableModel == nullptr) {
        return {};
    }
    return colorFromRgbCode(QIdentityProxyModel::data(
            rowIndex.siblingAtColumn(pTrackTableModel->fieldIndex(
                    ColumnCache::COLUMN_LIBRARYTABLE_COLOR)),
            Qt::DisplayRole)
                    .toDouble());
}

QmlTrackProxy* QmlLibraryTrackListModel::getTrack(int row) const {
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());

    if (pTrackModel == nullptr) {
        // TODO search for column with role
        return {};
    }
    return trackProxyForTrack(
            pTrackModel->getTrack(sourceModel()->index(row, 0)));
}

TrackModel::Capabilities QmlLibraryTrackListModel::getCapabilities() const {
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());

    if (pTrackModel != nullptr) {
        return pTrackModel->getCapabilities();
    }
    return TrackModel::Capability::None;
}
bool QmlLibraryTrackListModel::hasCapabilities(TrackModel::Capabilities caps) const {
    return (getCapabilities() & caps) == caps;
}

void QmlLibraryTrackListModel::search(const QString& searchText) {
    auto* const pTrackModel = dynamic_cast<TrackModel*>(sourceModel());
    VERIFY_OR_DEBUG_ASSERT(pTrackModel) {
        return;
    }
    pTrackModel->search(searchText);
    emit searchRequested(searchText);
}

void QmlLibraryTrackListModel::sort(int column, Qt::SortOrder order) {
    VERIFY_OR_DEBUG_ASSERT(column >= 0 || column < m_columns.size()) {
        return;
    }
    const auto& pColumn = m_columns[column];
    emit layoutAboutToBeChanged(QList<QPersistentModelIndex>(),
            QAbstractItemModel::VerticalSortHint);
    if (pColumn->columnIdx() < 0) {
        // Use proxyIndex.column()
        return sourceModel()->sort(column, order);
    }
    auto* const pTrackTableModel = qobject_cast<BaseTrackTableModel*>(sourceModel());
    sourceModel()->sort(pTrackTableModel != nullptr
                    ? pTrackTableModel->fieldIndex(
                              static_cast<ColumnCache::Column>(
                                      pColumn->columnIdx()))
                    : pColumn->columnIdx(),
            order);
    emit layoutChanged(QList<QPersistentModelIndex>(), QAbstractItemModel::VerticalSortHint);
}

} // namespace qml
} // namespace mixxx
