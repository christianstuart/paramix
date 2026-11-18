#include "qml/qmlsidebarmodelproxy.h"

#include <QString>
#include <QVariant>
#include <algorithm>
#include <functional>
#include <utility>

#include "library/autodj/autodjfeature.h"
#include "library/autodj/autodjprocessor.h"
#include "library/library.h"
#include "library/playlisttablemodel.h"
#include "library/trackmodel.h"
#include "moc_qmlsidebarmodelproxy.cpp"
#include "qml/qmllibraryproxy.h"
#include "util/assert.h"
#include "util/parented_ptr.h"

namespace mixxx {
namespace qml {

namespace {
const QHash<int, QByteArray> kRoleNames = {
        {Qt::DisplayRole, "label"},
        {QmlSidebarModelProxy::IconUrlRole, "icon"},
        {SidebarModel::DataRole, "data"},
};
} // namespace

QHash<int, QByteArray> QmlSidebarModelProxy::roleNames() const {
    return kRoleNames;
}

QVariant QmlSidebarModelProxy::data(const QModelIndex& index, int role) const {
    if (role == IconUrlRole) {
        if (index.isValid() && index.internalPointer() == this &&
                index.row() >= 0 && index.row() < m_sFeatures.size()) {
            // Return a string (not a QUrl) so that QML can test it with `!!icon`.
            const QUrl iconUrl = m_sFeatures.at(index.row())->iconUrl();
            return iconUrl.isValid() ? iconUrl.toString() : QString();
        }
        // Tree items use QIcon which cannot be rendered by QML directly.
        return QVariant();
    }
    return SidebarModel::data(index, role);
}

QVariant QmlSidebarModelProxy::get(int row) const {
    QModelIndex idx = index(row, 0);
    QVariantMap dataMap;
    for (auto it = kRoleNames.constBegin(); it != kRoleNames.constEnd(); it++) {
        dataMap.insert(it.value(), data(idx, it.key()));
    }
    return dataMap;
}

void QmlSidebarModelProxy::activate(const QModelIndex& index) {
    if (!index.isValid() || index.model() != this) {
        return;
    }
    SidebarModel::clicked(index);
}

void QmlSidebarModelProxy::expand(const QModelIndex& index) {
    if (!index.isValid() || index.model() != this) {
        return;
    }
    // Populates lazy child models (playlists, crates, folders, ...).
    SidebarModel::doubleClicked(index);
}

QmlSidebarModelProxy::QmlSidebarModelProxy(QObject* parent)
        : SidebarModel(parent),
          m_pTracklist(nullptr),
          m_featuresUpdated(false),
          m_pAutoDjFeature(nullptr),
          m_pAutoDjQueueModel(nullptr) {
}
QmlSidebarModelProxy::~QmlSidebarModelProxy() = default;

void QmlSidebarModelProxy::update() {
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return;
    }
    SidebarModel* pLibrarySidebar = pLibrary->sidebarModel();
    VERIFY_OR_DEBUG_ASSERT(pLibrarySidebar) {
        return;
    }
    // Library features are added once during Library construction and never
    // removed, so mirroring them more than once would duplicate connections.
    if (m_featuresUpdated) {
        return;
    }
    m_featuresUpdated = true;

    beginResetModel();
    for (LibraryFeature* pFeature : pLibrarySidebar->features()) {
        VERIFY_OR_DEBUG_ASSERT(pFeature) {
            continue;
        }
        addLibraryFeature(pFeature);
        connect(pFeature,
                &LibraryFeature::showTrackModel,
                this,
                &QmlSidebarModelProxy::slotShowTrackModel);
        if (auto* pAutoDjFeature = dynamic_cast<AutoDJFeature*>(pFeature)) {
            m_pAutoDjFeature = pAutoDjFeature;
            m_pAutoDjQueueModel = pAutoDjFeature->autoDjQueueModel();
        }
    }
    endResetModel();

    activateDefaultSelection();
}

void QmlSidebarModelProxy::activateDefaultSelection() {
    if (m_pTracklist || m_sFeatures.isEmpty()) {
        return;
    }
    const QModelIndex defaultIndex = index(0, 0);
    VERIFY_OR_DEBUG_ASSERT(defaultIndex.isValid()) {
        return;
    }
    // Show the default source (tracks) like the legacy library does when a
    // skin is loaded.
    SidebarModel::clicked(defaultIndex);
}

void QmlSidebarModelProxy::setColumns(
        const QList<QmlLibraryTrackListColumn*>& columns) {
    m_columns.clear();
    m_columns.reserve(columns.size());
    for (const auto* pColumn : columns) {
        VERIFY_OR_DEBUG_ASSERT(pColumn) {
            continue;
        }
        // Take a snapshot: the QML-owned template objects may be garbage
        // collected whereas the models created later must stay intact.
        m_columns.emplace_back(make_parented<QmlLibraryTrackListColumn>(this,
                pColumn->label(),
                pColumn->fillSpan(),
                pColumn->columnIdx(),
                pColumn->preferredWidth(),
                pColumn->autoHideWidth(),
                pColumn->delegate(),
                pColumn->role()));
    }
}

QmlLibraryTrackListModel* QmlSidebarModelProxy::trackListForModel(
        QAbstractItemModel* pModel) {
    for (const auto& entry : std::as_const(m_trackLists)) {
        if (entry.pSourceModel.data() == pModel) {
            return entry.pTrackList;
        }
    }

    VERIFY_OR_DEBUG_ASSERT(dynamic_cast<TrackModel*>(pModel)) {
        return nullptr;
    }

    QList<QmlLibraryTrackListColumn*> columns;
    columns.reserve(m_columns.size());
    for (const auto& column : std::as_const(m_columns)) {
        columns.append(column.get());
    }

    auto* pTrackList = new QmlLibraryTrackListModel(columns, pModel, this);
    // The model is owned by this proxy (QObject tree), not by the QML engine.
    QQmlEngine::setObjectOwnership(pTrackList, QQmlEngine::CppOwnership);
    connect(pTrackList,
            &QmlLibraryTrackListModel::searchRequested,
            this,
            &QmlSidebarModelProxy::slotTrackListSearch);
    m_trackLists.append(TrackListEntry{pModel, pTrackList});
    return pTrackList;
}

void QmlSidebarModelProxy::slotShowTrackModel(QAbstractItemModel* pModel) {
    VERIFY_OR_DEBUG_ASSERT(pModel) {
        return;
    }
    QmlLibraryTrackListModel* pTrackList = trackListForModel(pModel);
    if (!pTrackList) {
        // Not a track table model (e.g. a feature that only provides a
        // QWidget view which is not available in QML).
        return;
    }
    // The search is shared between all sources: apply the remembered query if
    // the model was prepared with a different one (or none at all).
    // The Auto DJ queue is excluded: the processor pulls its next track from
    // row 0 of the model, so filtering would change the playback order.
    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel);
    if (pTrackModel && pModel != m_pAutoDjQueueModel &&
            pTrackModel->currentSearch() != m_searchText) {
        pTrackList->search(m_searchText);
    }
    // Never destroy a model that has been exposed to QML: switching back to a
    // previously shown source reuses the cached model.
    if (m_pTracklist == pTrackList) {
        return;
    }
    const bool wasAutoDjActive = isAutoDjActive();
    m_pTracklist = pTrackList;
    emit tracklistChanged();
    if (wasAutoDjActive != isAutoDjActive()) {
        emit autoDjActiveChanged();
    }
}

void QmlSidebarModelProxy::setSearchText(const QString& searchText) {
    if (m_searchText == searchText) {
        return;
    }
    m_searchText = searchText;
    emit searchTextChanged();
    // The Auto DJ queue must not be filtered, see slotShowTrackModel().
    if (m_pTracklist && !isAutoDjActive()) {
        m_pTracklist->search(m_searchText);
    }
}

void QmlSidebarModelProxy::search(const QString& searchText) {
    setSearchText(searchText);
}

bool QmlSidebarModelProxy::isAutoDjActive() const {
    return m_pAutoDjQueueModel && m_pTracklist &&
            m_pTracklist->sourceModel() == m_pAutoDjQueueModel;
}

int QmlSidebarModelProxy::autoDjTransitionSeconds() const {
    if (!m_pAutoDjFeature) {
        return 0;
    }
    return static_cast<int>(m_pAutoDjFeature->autoDjProcessor()->getTransitionTime());
}

void QmlSidebarModelProxy::setAutoDjTransitionSeconds(int seconds) {
    if (!m_pAutoDjFeature) {
        return;
    }
    m_pAutoDjFeature->autoDjProcessor()->setTransitionTime(seconds);
    // AutoDJProcessor does not emit transitionTimeChanged, so notify here.
    emit autoDjTransitionSecondsChanged();
}

bool QmlSidebarModelProxy::autoDjRepeat() const {
    return m_pAutoDjFeature && m_pAutoDjFeature->isRepeatPlaylist();
}

void QmlSidebarModelProxy::setAutoDjRepeat(bool repeat) {
    if (!m_pAutoDjFeature) {
        return;
    }
    m_pAutoDjFeature->setRepeatPlaylist(repeat);
    emit autoDjRepeatChanged();
}

void QmlSidebarModelProxy::clearAutoDjQueue() {
    if (!m_pAutoDjFeature) {
        return;
    }
    // AutoDJFeature::clear() is the classic confirmation + DAO clear.
    m_pAutoDjFeature->clear();
}

void QmlSidebarModelProxy::removeAutoDjTracks(const QList<int>& rows) {
    if (!m_pAutoDjQueueModel || rows.isEmpty()) {
        return;
    }
    QModelIndexList indices;
    indices.reserve(rows.size());
    for (int row : rows) {
        const QModelIndex index = m_pAutoDjQueueModel->index(row, 0);
        if (index.isValid()) {
            indices.append(index);
        }
    }
    m_pAutoDjQueueModel->removeTracks(indices);
}

void QmlSidebarModelProxy::moveAutoDjTracks(const QList<int>& rows, int delta) {
    if (!m_pAutoDjQueueModel || rows.isEmpty() || delta == 0) {
        return;
    }
    QList<int> sortedRows = rows;
    std::sort(sortedRows.begin(), sortedRows.end());
    const int rowCount = m_pAutoDjQueueModel->rowCount();
    if (delta < 0) {
        for (int row : std::as_const(sortedRows)) {
            if (row > 0) {
                m_pAutoDjQueueModel->moveTrack(
                        m_pAutoDjQueueModel->index(row, 0),
                        m_pAutoDjQueueModel->index(row - 1, 0));
            }
        }
        return;
    }
    // Bottom-up, so that moving a block of rows keeps them adjacent.
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
    for (int row : std::as_const(sortedRows)) {
        if (row + 1 >= rowCount) {
            continue;
        }
        // The table model expects the row after the destination. An
        // out-of-range destination appends at the end of the queue.
        m_pAutoDjQueueModel->moveTrack(
                m_pAutoDjQueueModel->index(row, 0),
                m_pAutoDjQueueModel->index(row + 2, 0));
    }
}

void QmlSidebarModelProxy::slotTrackListSearch(const QString& searchText) {
    // The QML side applied the search directly on the current track list.
    // Remember it so that it survives switching to another source.
    if (m_searchText == searchText) {
        return;
    }
    m_searchText = searchText;
    emit searchTextChanged();
}

} // namespace qml
} // namespace mixxx
