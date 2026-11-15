#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QVariant>
#include <vector>

#include "library/libraryfeature.h"
#include "library/sidebarmodel.h"
#include "qmllibrarytracklistcolumn.h"
#include "qmllibrarytracklistmodel.h"
#include "util/parented_ptr.h"

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
    QML_ANONYMOUS
  public:
    enum Roles {
        /// URL of the source icon, suitable for a QML Image.
        IconUrlRole = Qt::UserRole + 100,
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

    /// Mirrors the features of the library's sidebar model. The features are
    /// created once during library construction, hence this is a one-shot
    /// operation.
    void update();

    /// Columns used for all track models created by this proxy.
    void setColumns(const QList<QmlLibraryTrackListColumn*>& columns);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex& index, int role) const override;

    Q_INVOKABLE QVariant get(int row) const;
    /// Activates the source/item at `index`, i.e. switches the track list.
    Q_INVOKABLE void activate(const QModelIndex& index);
    /// Populates the lazy children of `index`, e.g. playlists, crates or
    /// folders. Expanding/collapsing the visual tree is done by the view.
    Q_INVOKABLE void expand(const QModelIndex& index);
    /// Applies and remembers the search query for the current and all
    /// subsequent track lists.
    Q_INVOKABLE void search(const QString& searchText);

  signals:
    void tracklistChanged();
    void searchTextChanged();

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

    std::vector<parented_ptr<QmlLibraryTrackListColumn>> m_columns;
    QList<TrackListEntry> m_trackLists;
    QmlLibraryTrackListModel* m_pTracklist;
    QString m_searchText;
    bool m_featuresUpdated;
};

} // namespace qml
} // namespace mixxx
