#include "qml/qmlsidebarmodelproxy.h"

#include <QFileDialog>
#include <QPersistentModelIndex>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <algorithm>
#include <utility>

#include "library/autodj/autodjfeature.h"
#include "library/autodj/autodjprocessor.h"
#include "library/browse/browsefeature.h"
#include "library/library.h"
#include "library/playlisttablemodel.h"
#include "library/trackmodel.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/trackset/playlistfeature.h"
#include "library/trackset/setlogfeature.h"
#include "library/treeitem.h"
#include "moc_qmlsidebarmodelproxy.cpp"
#include "preferences/configobject.h"
#include "preferences/usersettings.h"
#include "qml/qmlconfigproxy.h"
#include "qml/qmllibraryproxy.h"
#include "util/assert.h"
#include "util/parented_ptr.h"

namespace mixxx {
namespace qml {

namespace {
const QHash<int, QByteArray> kRoleNames = {
        {Qt::DisplayRole, "label"},
        {QmlSidebarModelProxy::IconUrlRole, "icon"},
        {QmlSidebarModelProxy::AvailableRole, "available"},
        {SidebarModel::DataRole, "data"},
};

const ConfigKey kHiddenSourcesConfigKey(
        QStringLiteral("[Library]"), QStringLiteral("HiddenSidebarSources"));

/// Stable, translation independent id of a source. The icon name is unique per
/// feature; external collections without one fall back to the class name.
QString featureId(const LibraryFeature* pFeature) {
    if (!pFeature) {
        return QString();
    }
    const QString iconName = pFeature->iconName();
    return iconName.isEmpty()
            ? QString::fromLatin1(pFeature->metaObject()->className())
            : iconName;
}

QStringList hiddenSourceIds() {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pConfig) {
        return {};
    }
    return pConfig->getValueString(kHiddenSourcesConfigKey)
            .split(QLatin1Char(','), Qt::SkipEmptyParts);
}

LibraryFeature* featureForIndex(const SidebarModel* pSidebar,
        const QList<LibraryFeature*>& features,
        const QModelIndex& index);
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
    if (role == AvailableRole) {
        if (!index.isValid()) {
            return true;
        }
        if (index.internalPointer() == this) {
            const LibraryFeature* pFeature = m_sFeatures.value(index.row());
            return pFeature ? pFeature->isAvailable() : true;
        }
        // Children belong to their feature, so they inherit its availability.
        const auto* pItem = static_cast<const TreeItem*>(index.internalPointer());
        const LibraryFeature* pFeature = pItem ? pItem->feature() : nullptr;
        return pFeature ? pFeature->isAvailable() : true;
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
    const LibraryFeature* pFeature = featureForIndex(this, m_sFeatures, index);
    if (pFeature && !pFeature->isAvailable()) {
        // The external library was not found; the row is inert.
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

QString QmlSidebarModelProxy::addAction(const QModelIndex& index) const {
    if (!index.isValid() || index.model() != this ||
            index.internalPointer() != this) {
        return QString();
    }
    // Mirrors the first entry contextMenu() offers for the same row.
    LibraryFeature* pFeature = m_sFeatures.value(index.row());
    if (dynamic_cast<PlaylistFeature*>(pFeature)) {
        return QStringLiteral("createPlaylist");
    }
    if (dynamic_cast<CrateFeature*>(pFeature)) {
        return QStringLiteral("createCrate");
    }
    if (dynamic_cast<BrowseFeature*>(pFeature)) {
        return QStringLiteral("addToLibrary");
    }
    return QString();
}

QModelIndex QmlSidebarModelProxy::parentIndex(const QModelIndex& index) const {
    return index.isValid() ? index.parent() : QModelIndex();
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
    // removed, so wiring them more than once would duplicate connections. Only
    // the visible subset is mirrored below, and setSourceHidden() rebuilds it.
    if (!m_featuresUpdated) {
        m_featuresUpdated = true;
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
    }

    rebuild();
    activateDefaultSelection();
}

void QmlSidebarModelProxy::rebuild() {
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return;
    }
    SidebarModel* pLibrarySidebar = pLibrary->sidebarModel();
    VERIFY_OR_DEBUG_ASSERT(pLibrarySidebar) {
        return;
    }

    // Keep showing the same source unless it was just hidden.
    LibraryFeature* pSelectedFeature = featureForIndex(this, m_sFeatures, selectedIndex());
    const QStringList hiddenIds = hiddenSourceIds();

    beginResetModel();
    m_sFeatures.clear();
    for (LibraryFeature* pFeature : pLibrarySidebar->features()) {
        if (pFeature && !hiddenIds.contains(featureId(pFeature))) {
            m_sFeatures.append(pFeature);
        }
    }
    endResetModel();

    const int row = qMax(0, m_sFeatures.indexOf(pSelectedFeature));
    const QModelIndex target = index(row, 0);
    if (!target.isValid()) {
        return;
    }
    // Show its tracks (or the first source when the previous one was hidden)
    // and restore the selection the reset just dropped.
    SidebarModel::clicked(target);
    emit selectIndex(target, false);
}

QVariantList QmlSidebarModelProxy::sources() const {
    QVariantList result;
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return result;
    }
    SidebarModel* pLibrarySidebar = pLibrary->sidebarModel();
    VERIFY_OR_DEBUG_ASSERT(pLibrarySidebar) {
        return result;
    }
    const QStringList hiddenIds = hiddenSourceIds();
    const QList<LibraryFeature*>& features = pLibrarySidebar->features();
    result.reserve(static_cast<qsizetype>(features.size()));
    for (LibraryFeature* pFeature : features) {
        if (!pFeature) {
            continue;
        }
        const QString id = featureId(pFeature);
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), id);
        entry.insert(QStringLiteral("title"), pFeature->title().toString());
        entry.insert(QStringLiteral("icon"), pFeature->iconUrl().toString());
        entry.insert(QStringLiteral("available"), pFeature->isAvailable());
        entry.insert(QStringLiteral("hidden"), hiddenIds.contains(id));
        result.append(entry);
    }
    return result;
}

void QmlSidebarModelProxy::setSourceHidden(const QString& sourceId, bool hidden) {
    if (sourceId.isEmpty()) {
        return;
    }
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return;
    }
    SidebarModel* pLibrarySidebar = pLibrary->sidebarModel();
    VERIFY_OR_DEBUG_ASSERT(pLibrarySidebar) {
        return;
    }
    // Ignore ids that no longer exist so a stale entry cannot hide anything.
    bool known = false;
    for (const LibraryFeature* pFeature : pLibrarySidebar->features()) {
        if (featureId(pFeature) == sourceId) {
            known = true;
            break;
        }
    }
    if (!known) {
        return;
    }

    QStringList hiddenIds = hiddenSourceIds();
    if (hiddenIds.contains(sourceId) == hidden) {
        return;
    }
    if (hidden) {
        hiddenIds.append(sourceId);
    } else {
        hiddenIds.removeAll(sourceId);
    }
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pConfig) {
        return;
    }
    pConfig->setValue(kHiddenSourcesConfigKey, hiddenIds.join(QLatin1Char(',')));

    rebuild();
    emit sourcesChanged();
}

void QmlSidebarModelProxy::showAllSources() {
    if (hiddenSourceIds().isEmpty()) {
        return;
    }
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pConfig) {
        return;
    }
    pConfig->setValue(kHiddenSourcesConfigKey, QString());

    rebuild();
    emit sourcesChanged();
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
                pColumn->key(),
                pColumn->label(),
                pColumn->fillSpan(),
                pColumn->columnIdx(),
                pColumn->preferredWidth(),
                pColumn->autoHideWidth(),
                pColumn->delegate(),
                pColumn->role(),
                pColumn->hidden()));
    }
}

QVariantList QmlSidebarModelProxy::columnLayout() const {
    QVariantList layout;
    layout.reserve(static_cast<qsizetype>(m_columns.size()));
    for (const auto& pColumn : m_columns) {
        QVariantMap entry;
        entry.insert(QStringLiteral("key"), pColumn->key());
        entry.insert(QStringLiteral("hidden"), pColumn->hidden());
        layout.append(entry);
    }
    return layout;
}

void QmlSidebarModelProxy::applyColumnsToTrackLists() {
    QStringList orderKeys;
    QStringList hiddenKeys;
    orderKeys.reserve(static_cast<qsizetype>(m_columns.size()));
    for (const auto& pColumn : m_columns) {
        orderKeys.append(pColumn->key());
        if (pColumn->hidden()) {
            hiddenKeys.append(pColumn->key());
        }
    }
    for (const auto& entry : std::as_const(m_trackLists)) {
        entry.pTrackList->applyColumnLayout(orderKeys, hiddenKeys);
    }
}

void QmlSidebarModelProxy::moveColumn(int from, int to) {
    const int count = static_cast<int>(m_columns.size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
        return;
    }
    auto column = std::move(m_columns[from]);
    m_columns.erase(m_columns.begin() + from);
    m_columns.insert(m_columns.begin() + to, std::move(column));
    applyColumnLayout(columnLayout());
}

void QmlSidebarModelProxy::setColumnHidden(int column, bool hidden) {
    if (column < 0 || column >= static_cast<int>(m_columns.size())) {
        return;
    }
    if (m_columns[column]->hidden() == hidden) {
        return;
    }
    m_columns[column]->setHidden(hidden);
    applyColumnLayout(columnLayout());
}

void QmlSidebarModelProxy::applyColumnLayout(const QVariantList& layout) {
    QStringList orderKeys;
    QStringList hiddenKeys;
    for (const QVariant& item : layout) {
        const QVariantMap entry = item.toMap();
        const QString key = entry.value(QStringLiteral("key")).toString();
        if (key.isEmpty()) {
            continue;
        }
        orderKeys.append(key);
        if (entry.value(QStringLiteral("hidden")).toBool()) {
            hiddenKeys.append(key);
        }
    }

    std::vector<parented_ptr<QmlLibraryTrackListColumn>> ordered;
    ordered.reserve(m_columns.size());
    QSet<QString> placed;
    for (const QString& key : std::as_const(orderKeys)) {
        if (placed.contains(key)) {
            continue;
        }
        for (auto& pColumn : m_columns) {
            if (pColumn && pColumn->key() == key) {
                ordered.push_back(std::move(pColumn));
                placed.insert(key);
                break;
            }
        }
    }
    // Columns missing from the saved layout keep their relative order.
    for (auto& pColumn : m_columns) {
        if (pColumn) {
            ordered.push_back(std::move(pColumn));
        }
    }

    const QSet<QString> hidden(hiddenKeys.begin(), hiddenKeys.end());
    m_columns = std::move(ordered);
    for (auto& pColumn : m_columns) {
        pColumn->setHidden(hidden.contains(pColumn->key()));
    }
    applyColumnsToTrackLists();
    emit columnLayoutChanged();
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
    const int rowCount = m_pAutoDjQueueModel->rowCount();
    QList<int> sortedRows;
    sortedRows.reserve(rows.size());
    for (int row : rows) {
        if (row >= 0 && row < rowCount) {
            sortedRows.append(row);
        }
    }
    if (sortedRows.isEmpty()) {
        return;
    }
    std::sort(sortedRows.begin(), sortedRows.end());

    // The selection moves as one block, like WTrackTableView::moveRows(): a
    // block that already touches the edge it moves toward stays where it is.
    if (delta < 0) {
        if (sortedRows.first() == 0) {
            return;
        }
        // Top to bottom: each move only shifts the rows above the block, so
        // the remaining row numbers stay valid and the block keeps its order.
        for (int row : std::as_const(sortedRows)) {
            m_pAutoDjQueueModel->moveTrack(
                    m_pAutoDjQueueModel->index(row, 0),
                    m_pAutoDjQueueModel->index(row - 1, 0));
        }
        return;
    }

    if (sortedRows.last() == rowCount - 1) {
        return;
    }
    // Bottom to top: each move only shifts the rows below the block.
    for (int i = sortedRows.size() - 1; i >= 0; --i) {
        const int row = sortedRows.at(i);
        // The table model expects the row after the destination. A row that
        // moves to the end passes an out-of-range row, which appends it.
        m_pAutoDjQueueModel->moveTrack(
                m_pAutoDjQueueModel->index(row, 0),
                m_pAutoDjQueueModel->index(row + 2, 0));
    }
}

namespace {

void appendMenuAction(QVariantList* pNodes,
        const QString& action,
        const QString& text,
        bool enabled = true) {
    QVariantMap node;
    node.insert(QStringLiteral("kind"), QStringLiteral("action"));
    node.insert(QStringLiteral("action"), action);
    node.insert(QStringLiteral("text"), text);
    node.insert(QStringLiteral("enabled"), enabled);
    pNodes->append(node);
}

void appendMenuSeparator(QVariantList* pNodes) {
    if (pNodes->isEmpty()) {
        return;
    }
    QVariantMap node;
    node.insert(QStringLiteral("kind"), QStringLiteral("separator"));
    pNodes->append(node);
}

LibraryFeature* featureForIndex(const SidebarModel* pSidebar,
        const QList<LibraryFeature*>& features,
        const QModelIndex& index) {
    if (!index.isValid()) {
        return nullptr;
    }
    if (index.internalPointer() == pSidebar) {
        // The internal pointer of a top-level row is the sidebar model itself.
        return features.value(index.row());
    }
    auto* pTreeItem = static_cast<TreeItem*>(index.internalPointer());
    return pTreeItem ? pTreeItem->feature() : nullptr;
}

} // namespace

QVariantList QmlSidebarModelProxy::contextMenu(const QModelIndex& index) const {
    QVariantList nodes;
    if (!index.isValid() || index.model() != this) {
        return nodes;
    }
    LibraryFeature* pFeature = featureForIndex(this, m_sFeatures, index);
    if (!pFeature) {
        return nodes;
    }

    const bool isRootRow = index.internalPointer() == this;
    if (isRootRow) {
        if (dynamic_cast<PlaylistFeature*>(pFeature)) {
            appendMenuAction(&nodes,
                    QStringLiteral("createPlaylist"),
                    tr("Create New Playlist"));
            appendMenuSeparator(&nodes);
            appendMenuAction(&nodes,
                    QStringLiteral("importPlaylist"),
                    tr("Import Playlist"));
        } else if (dynamic_cast<CrateFeature*>(pFeature)) {
            appendMenuAction(&nodes,
                    QStringLiteral("createCrate"),
                    tr("Create New Crate"));
            appendMenuSeparator(&nodes);
            appendMenuAction(&nodes,
                    QStringLiteral("importPlaylist"),
                    tr("Import Playlist"));
        } else if (dynamic_cast<BrowseFeature*>(pFeature)) {
            appendMenuAction(&nodes,
                    QStringLiteral("addToLibrary"),
                    tr("Add to Library"));
            appendMenuAction(&nodes,
                    QStringLiteral("rescanLibrary"),
                    tr("Rescan Library"));
        }
        appendMenuSeparator(&nodes);
        appendMenuAction(&nodes,
                QStringLiteral("hideSource"),
                tr("Hide This Source"));
        appendMenuAction(&nodes,
                QStringLiteral("showSources"),
                tr("Sources..."));
        return nodes;
    }

    if (auto* pSetlog = dynamic_cast<SetlogFeature*>(pFeature)) {
        if (pSetlog->isYearIndex(index)) {
            appendMenuAction(&nodes,
                    QStringLiteral("lockChildPlaylists"),
                    tr("Lock all child playlists"));
            appendMenuAction(&nodes,
                    QStringLiteral("unlockChildPlaylists"),
                    tr("Unlock all child playlists"));
            appendMenuSeparator(&nodes);
            appendMenuAction(&nodes,
                    QStringLiteral("purgeChildPlaylists"),
                    tr("Delete all unlocked child playlists"));
            return nodes;
        }
    }
    if (auto* pPlaylist = dynamic_cast<BasePlaylistFeature*>(pFeature)) {
        const bool locked = pPlaylist->isItemLocked(index);
        auto* pSetlog = dynamic_cast<SetlogFeature*>(pFeature);
        // Today's history playlist cannot be removed or locked.
        const bool historyProtected =
                pSetlog && pSetlog->isCurrentPlaylistIndex(index);
        appendMenuAction(&nodes,
                QStringLiteral("rename"),
                tr("Rename"),
                !locked);
        if (!pSetlog) {
            appendMenuAction(&nodes, QStringLiteral("duplicate"), tr("Duplicate"));
        }
        appendMenuAction(&nodes,
                QStringLiteral("remove"),
                tr("Remove"),
                !locked && !historyProtected);
        appendMenuAction(&nodes,
                QStringLiteral("lock"),
                locked ? tr("Unlock") : tr("Lock"),
                !historyProtected);
        appendMenuSeparator(&nodes);
        appendMenuAction(&nodes,
                QStringLiteral("export"),
                tr("Export Playlist"));
        return nodes;
    }
    if (auto* pCrate = dynamic_cast<CrateFeature*>(pFeature)) {
        const bool locked = pCrate->isItemLocked(index);
        appendMenuAction(&nodes, QStringLiteral("rename"), tr("Rename"), !locked);
        appendMenuAction(&nodes, QStringLiteral("duplicate"), tr("Duplicate"));
        appendMenuAction(&nodes, QStringLiteral("remove"), tr("Remove"), !locked);
        appendMenuAction(&nodes,
                QStringLiteral("lock"),
                locked ? tr("Unlock") : tr("Lock"));
        appendMenuSeparator(&nodes);
        appendMenuAction(&nodes,
                QStringLiteral("export"),
                tr("Export Crate as Playlist"));
        return nodes;
    }
    return nodes;
}

void QmlSidebarModelProxy::invokeContextMenuAction(
        const QModelIndex& index, const QString& action) {
    if (!index.isValid() || index.model() != this || action.isEmpty()) {
        return;
    }
    // Defer: rename/delete open modal dialogs or reset the sidebar model while
    // QML is still unwinding the menu's trigger handler (the same reason
    // QmlLibraryTrackMenu::invoke() defers). A reset deletes the TreeItems
    // behind a plain QModelIndex, so track the row with a persistent index and
    // drop the action if it did not survive.
    const QPersistentModelIndex persistentIndex(index);
    QTimer::singleShot(0, this, [this, persistentIndex, action]() {
        if (!persistentIndex.isValid()) {
            return;
        }
        invokeContextMenuActionNow(persistentIndex, action);
    });
}

void QmlSidebarModelProxy::invokeContextMenuActionNow(
        const QModelIndex& index, const QString& action) {
    if (!index.isValid() || index.model() != this || action.isEmpty()) {
        return;
    }
    LibraryFeature* pFeature = featureForIndex(this, m_sFeatures, index);
    if (!pFeature) {
        return;
    }

    if (action == QLatin1String("addToLibrary")) {
        if (dynamic_cast<BrowseFeature*>(pFeature)) {
            addFolderToLibrary();
        }
        return;
    }
    if (action == QLatin1String("rescanLibrary")) {
        if (!dynamic_cast<BrowseFeature*>(pFeature)) {
            return;
        }
        Library* pLibrary = QmlLibraryProxy::get();
        TrackCollectionManager* pTrackCollectionManager =
                pLibrary ? pLibrary->trackCollectionManager() : nullptr;
        if (pTrackCollectionManager &&
                !pTrackCollectionManager->isLibraryScanActive()) {
            pTrackCollectionManager->startLibraryScan();
        }
        return;
    }

    if (index.internalPointer() == this) {
        if (action == QLatin1String("hideSource")) {
            setSourceHidden(featureId(m_sFeatures.value(index.row())), true);
            return;
        }
        if (action == QLatin1String("showSources")) {
            emit sourceSettingsRequested();
            return;
        }
        Library* pLibrary = QmlLibraryProxy::get();
        if (!pLibrary) {
            return;
        }
        if (action == QLatin1String("createPlaylist")) {
            pLibrary->slotCreatePlaylist();
        } else if (action == QLatin1String("createCrate")) {
            pLibrary->slotCreateCrate();
        } else if (action == QLatin1String("importPlaylist")) {
            if (auto* pPlaylist = dynamic_cast<BasePlaylistFeature*>(pFeature)) {
                pPlaylist->importItem();
            } else if (auto* pCrate = dynamic_cast<CrateFeature*>(pFeature)) {
                pCrate->importItem();
            }
        }
        return;
    }

    if (auto* pSetlog = dynamic_cast<SetlogFeature*>(pFeature)) {
        if (action == QLatin1String("lockChildPlaylists")) {
            pSetlog->lockChildPlaylists(index, true);
            return;
        }
        if (action == QLatin1String("unlockChildPlaylists")) {
            pSetlog->lockChildPlaylists(index, false);
            return;
        }
        if (action == QLatin1String("purgeChildPlaylists")) {
            pSetlog->purgeChildPlaylists(index);
            return;
        }
        if (pSetlog->isCurrentPlaylistIndex(index) &&
                (action == QLatin1String("remove") ||
                        action == QLatin1String("lock"))) {
            return;
        }
    }
    if (auto* pPlaylist = dynamic_cast<BasePlaylistFeature*>(pFeature)) {
        if (action == QLatin1String("rename")) {
            pPlaylist->renameItem(index);
        } else if (action == QLatin1String("duplicate")) {
            pPlaylist->duplicateItem(index);
        } else if (action == QLatin1String("remove")) {
            pPlaylist->deleteItem(index);
        } else if (action == QLatin1String("lock")) {
            pPlaylist->toggleLockItem(index);
        } else if (action == QLatin1String("export")) {
            pPlaylist->exportItem(index);
        }
        return;
    }
    if (auto* pCrate = dynamic_cast<CrateFeature*>(pFeature)) {
        if (action == QLatin1String("rename")) {
            pCrate->renameItem(index);
        } else if (action == QLatin1String("duplicate")) {
            pCrate->duplicateItem(index);
        } else if (action == QLatin1String("remove")) {
            pCrate->deleteItem(index);
        } else if (action == QLatin1String("lock")) {
            pCrate->toggleLockItem(index);
        } else if (action == QLatin1String("export")) {
            pCrate->exportItem(index);
        }
    }
}

void QmlSidebarModelProxy::addFolderToLibrary() {
    Library* pLibrary = QmlLibraryProxy::get();
    if (!pLibrary) {
        return;
    }
    const QString directory = QFileDialog::getExistingDirectory(
            nullptr,
            tr("Add to Library"),
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (directory.isEmpty()) {
        return;
    }
    if (pLibrary->requestAddDir(directory)) {
        // A new directory is only visible after the scanner has run.
        pLibrary->trackCollectionManager()->startLibraryScan();
    }
}

bool QmlSidebarModelProxy::dropUrls(
        const QModelIndex& index, const QList<QUrl>& urls) {
    if (!index.isValid() || index.model() != this || urls.isEmpty()) {
        return false;
    }
    const LibraryFeature* pFeature = featureForIndex(this, m_sFeatures, index);
    if (pFeature && !pFeature->isAvailable()) {
        // A source whose external library was not found cannot take tracks.
        return false;
    }
    // dragMoveAccept() lets the feature reject locked or invalid targets;
    // dropAccept() asserts on those.
    if (!dragMoveAccept(index, urls)) {
        return false;
    }
    // SidebarModel::dropAccept() forwards to the playlist/crate feature, which
    // imports unknown tracks and appends them to its track set.
    return dropAccept(index, urls, nullptr);
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
