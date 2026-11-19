#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <vector>

#include "library/libraryfeature.h"
#include "library/sidebarmodel.h"
#include "qmllibrarytracklistcolumn.h"
#include "qmllibrarytracklistmodel.h"
#include "util/parented_ptr.h"

class AutoDJFeature;
class PlaylistTableModel;

namespace mixxx {
namespace qml {

/// QML wrapper around the library's real sidebar model.
///
/// The model mirrors the features of `Library::sidebarModel()` so that the QML
/// library pane offers the same sources as the legacy QWidget library
/// (tracks, playlists, crates, AutoDJ, browse, recordings, ...).
///
/// Track models requested by a feature are wrapped into
/// QmlLibraryTrackListModel instances which are cached for the lifetime of
/// this object. A model that has been exposed to QML is never deleted
/// implicitly: QML views (QQuickTableView) keep raw pointers to it, so
/// releasing it while the view still references it leads to a use-after-free.
class QmlSidebarModelProxy : public SidebarModel {
    Q_OBJECT
    Q_PROPERTY(QmlLibraryTrackListModel* tracklist READ tracklist NOTIFY tracklistChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY
                    searchTextChanged)
    /// True while the Auto DJ queue is the track list shown to the right of the
    /// source tree. Drives the visibility of the Auto DJ bar.
    Q_PROPERTY(bool autoDjActive READ isAutoDjActive NOTIFY autoDjActiveChanged)
    /// Same preference and control as DlgAutoDJ::spinBoxTransition.
    Q_PROPERTY(int autoDjTransitionSeconds READ autoDjTransitionSeconds WRITE
                    setAutoDjTransitionSeconds NOTIFY autoDjTransitionSecondsChanged)
    /// Same preference as the "Repeat" button of DlgAutoDJ.
    Q_PROPERTY(bool autoDjRepeat READ autoDjRepeat WRITE setAutoDjRepeat NOTIFY
                    autoDjRepeatChanged)
    QML_ANONYMOUS
  public:
    enum Roles {
        /// URL of the source icon, suitable for a QML Image.
        IconUrlRole = Qt::UserRole + 100,
        /// False for an external library that is not installed on this
        /// machine. Browser.qml dims those rows and does not activate them.
        AvailableRole,
    };
    Q_ENUM(Roles);

    Q_DISABLE_COPY_MOVE(QmlSidebarModelProxy)
    explicit QmlSidebarModelProxy(QObject* parent = nullptr);
    ~QmlSidebarModelProxy() override;

    QmlLibraryTrackListModel* tracklist() const {
        return m_pTracklist;
    }

    QString searchText() const {
        return m_searchText;
    }
    void setSearchText(const QString& searchText);

    bool isAutoDjActive() const;
    int autoDjTransitionSeconds() const;
    void setAutoDjTransitionSeconds(int seconds);
    bool autoDjRepeat() const;
    void setAutoDjRepeat(bool repeat);

    /// Mirrors the visible features of the library's sidebar model. Call it
    /// once at startup; setSourceHidden() and showAllSources() rebuild the
    /// mirror later.
    void update();

    /// Every source of the library, hidden ones included. Each entry is a map
    /// with the keys `id`, `title`, `icon`, `available` and `hidden`.
    Q_INVOKABLE QVariantList sources() const;
    /// Hides or shows a source and remembers it in Config.
    Q_INVOKABLE void setSourceHidden(const QString& sourceId, bool hidden);
    /// Makes every source visible again.
    Q_INVOKABLE void showAllSources();

    /// Columns used for all track models created by this proxy.
    void setColumns(const QList<QmlLibraryTrackListColumn*>& columns);

    /// Column layout shared by every source. Changing it updates the cached
    /// models and every model created later.
    Q_INVOKABLE void moveColumn(int from, int to);
    Q_INVOKABLE void setColumnHidden(int column, bool hidden);
    /// Ordered list of maps with the keys `key` and `hidden`.
    Q_INVOKABLE QVariantList columnLayout() const;
    Q_INVOKABLE void applyColumnLayout(const QVariantList& layout);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex& index, int role) const override;

    Q_INVOKABLE QVariant get(int row) const;
    /// Activates the source/item at `index`, i.e. switches the track list.
    Q_INVOKABLE void activate(const QModelIndex& index);
    /// Populates the lazy children of `index`, e.g. playlists, crates or
    /// folders. Expanding/collapsing the visual tree is done by the view.
    Q_INVOKABLE void expand(const QModelIndex& index);
    /// Action that creates a new item for a source row, rendered as the hover
    /// "+" button by Browser.qml. Empty for rows without such an action.
    Q_INVOKABLE QString addAction(const QModelIndex& index) const;
    /// Parent of `index`. QML cannot call QModelIndex::parent() itself.
    Q_INVOKABLE QModelIndex parentIndex(const QModelIndex& index) const;
    /// Applies and remembers the search query for the current and all
    /// subsequent track lists.
    Q_INVOKABLE void search(const QString& searchText);

    /// Auto DJ queue editing. `rows` are rows of the queue as displayed, i.e.
    /// of the track list created for the Auto DJ playlist model.
    Q_INVOKABLE void clearAutoDjQueue();
    Q_INVOKABLE void removeAutoDjTracks(const QList<int>& rows);
    /// Moves the given rows one step up (`delta` < 0) or down (`delta` > 0).
    Q_INVOKABLE void moveAutoDjTracks(const QList<int>& rows, int delta);

    /// Context menu of a source-tree row, rendered by Browser.qml. Each node is
    /// a map with the keys `kind` ("action" or "separator"), `action` (the id
    /// to pass to invokeContextMenuAction()), `text` and `enabled`.
    Q_INVOKABLE QVariantList contextMenu(const QModelIndex& index) const;
    Q_INVOKABLE void invokeContextMenuAction(
            const QModelIndex& index, const QString& action);

    /// Drops `urls` onto the playlist, crate or source at `index`. Returns
    /// true if the feature accepted the drop.
    Q_INVOKABLE bool dropUrls(const QModelIndex& index, const QList<QUrl>& urls);

  signals:
    void tracklistChanged();
    void searchTextChanged();
    void autoDjActiveChanged();
    void autoDjTransitionSecondsChanged();
    void autoDjRepeatChanged();
    void columnLayoutChanged();
    /// The QML settings dialog should open, e.g. from "Sources..." in the
    /// source context menu.
    void sourceSettingsRequested();
    /// A source was hidden or shown, so `sources()` changed.
    void sourcesChanged();

  private slots:
    void slotShowTrackModel(QAbstractItemModel* pModel);
    void slotTrackListSearch(const QString& searchText);

  private:
    struct TrackListEntry {
        QPointer<QAbstractItemModel> pSourceModel;
        QmlLibraryTrackListModel* pTrackList;
    };

    QmlLibraryTrackListModel* trackListForModel(QAbstractItemModel* pModel);
    void activateDefaultSelection();
    void addFolderToLibrary();
    void applyColumnsToTrackLists();
    void invokeContextMenuActionNow(
            const QModelIndex& index, const QString& action);
    /// Rebuilds the mirrored feature list from the hidden sources in Config.
    void rebuild();

    std::vector<parented_ptr<QmlLibraryTrackListColumn>> m_columns;
    QList<TrackListEntry> m_trackLists;
    QmlLibraryTrackListModel* m_pTracklist;
    QString m_searchText;
    bool m_featuresUpdated;
    AutoDJFeature* m_pAutoDjFeature;
    PlaylistTableModel* m_pAutoDjQueueModel;
};

} // namespace qml
} // namespace mixxx
