#pragma once
#include <QColor>
#include <QIdentityProxyModel>
#include <QPointer>
#include <QQmlEngine>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "library/trackmodel.h"
#include "qml/qmllibrarytracklistcolumn.h"
#include "qml/qmltrackproxy.h"
#include "track/trackid.h"

namespace mixxx {
namespace qml {

class QmlLibraryTrackListModel : public QIdentityProxyModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryTrackListModel)
    Q_PROPERTY(QQmlListProperty<mixxx::qml::QmlLibraryTrackListColumn> columns READ columns FINAL)
    /// Monotonic counter bumped whenever the model's data changes. The cover
    /// column binds to this so its invokable-based bindings re-evaluate when a
    /// track's colour or cover changes (a plain function call has no model
    /// dependency).
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged FINAL)
    QML_UNCREATABLE("Only accessible via Mixxx.Library")

  public:
    enum Roles {
        Track = Qt::UserRole,
        FileURL,
        CoverArt,
        Delegate,
        HasStems,
    };
    Q_ENUM(Roles);

    // FIXME Remove the enum duplication with the `Capability` in `trackmodel.h`
    enum class Capability {
        None = 0u,
        Reorder = 1u << 0u,
        ReceiveDrops = 1u << 1u,
        AddToTrackSet = 1u << 2u,
        AddToAutoDJ = 1u << 3u,
        Locked = 1u << 4u,
        EditMetadata = 1u << 5u,
        LoadToDeck = 1u << 6u,
        LoadToSampler = 1u << 7u,
        LoadToPreviewDeck = 1u << 8u,
        Remove = 1u << 9u,
        ResetPlayed = 1u << 10u,
        Hide = 1u << 11u,
        Unhide = 1u << 12u,
        Purge = 1u << 13u,
        RemovePlaylist = 1u << 14u,
        RemoveCrate = 1u << 15u,
        RemoveFromDisk = 1u << 16u,
        Analyze = 1u << 17u,
        Properties = 1u << 18u,
        Sorting = 1u << 19u,
    };
    Q_ENUM(Capability)

    QmlLibraryTrackListModel(const QList<QmlLibraryTrackListColumn*>& librarySource,
            QAbstractItemModel* pModel,
            QObject* pParent = nullptr);
    ~QmlLibraryTrackListModel() = default;

    QQmlListProperty<mixxx::qml::QmlLibraryTrackListColumn> columns() {
        return {this,
                &m_columns,
                parent_qlist_append,
                parent_qlist_count,
                parent_qlist_at,
                parent_qlist_clear};
    }

    int revision() const {
        return m_revision;
    }

    QVariant data(const QModelIndex& index, int role) const override;
    int columnCount(const QModelIndex& index = QModelIndex()) const override;
    Q_INVOKABLE QUrl getUrl(int row) const;
    Q_INVOKABLE QUrl getCoverArtUrl(int row) const;
    Q_INVOKABLE QColor getDecorationColor(int row) const;
    Q_INVOKABLE mixxx::qml::QmlTrackProxy* getTrack(int row) const;
    Q_INVOKABLE TrackModel::Capabilities getCapabilities() const;
    Q_INVOKABLE bool hasCapabilities(TrackModel::Capabilities caps) const;
    Q_INVOKABLE void search(const QString& searchText);
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariant headerData(int section,
            Qt::Orientation orientation,
            int role = Qt::DisplayRole) const override;
    Q_INVOKABLE void sort(int column, Qt::SortOrder order) override;
    /// Applies a column order and visibility shared by every source. Keys that
    /// are missing from `orderKeys` keep their relative order at the end, so a
    /// saved layout from an older version does not hide newly added columns.
    void applyColumnLayout(const QStringList& orderKeys,
            const QStringList& hiddenKeys);

  signals:
    /// Emitted after a search was applied to the underlying track model so
    /// that owners can remember the query and restore it later.
    void searchRequested(const QString& searchText);
    void revisionChanged();

  private:
    void bumpRevision();
    /// Connects to StemService::instance() once it exists. Start-up may create
    /// the service after this model, so a bounded retry timer is used.
    void ensureStemServiceConnected();

    /// Re-emits the HasStems role for every row of `trackId`. When
    /// `populateStemInfo` is true the cached stem metadata is attached to the
    /// track first, mirroring what opening a cached stem source would do.
    void refreshHasStems(TrackId trackId, bool populateStemInfo);

    /// Returns a shared QmlTrackProxy for a track. QML keeps the returned
    /// object alive (JavaScript ownership); the model only caches weak
    /// references so reusing the same track in several cells does not create a
    /// new proxy (and its cue/beat/stem models) per cell.
    QmlTrackProxy* trackProxyForTrack(const TrackPointer& pTrack) const;

    /// Track ids with an exported stem set. Used for the has_stems role so the
    /// badge also shows before the stem metadata is loaded into the track.
    QSet<TrackId> m_stemTrackIds;

    mutable QHash<TrackId, QPointer<QmlTrackProxy>> m_trackProxies;
    mutable int m_trackProxySweepCountdown = 0;

    std::vector<parented_ptr<QmlLibraryTrackListColumn>> m_columns;
    int m_revision = 0;

    QTimer m_stemServiceRetryTimer;
    bool m_stemServiceConnected = false;
    int m_stemServiceRetryCount = 0;

    static void parent_qlist_append(
            QQmlListProperty<QmlLibraryTrackListColumn>* p,
            QmlLibraryTrackListColumn* v) {
        reinterpret_cast<std::vector<parented_ptr<QmlLibraryTrackListColumn>>*>(
                p->data)
                ->emplace_back(v);
    }
    static qsizetype parent_qlist_count(QQmlListProperty<QmlLibraryTrackListColumn>* p) {
        return reinterpret_cast<
                std::vector<parented_ptr<QmlLibraryTrackListColumn>>*>(p->data)
                ->size();
    }
    static QmlLibraryTrackListColumn* parent_qlist_at(
            QQmlListProperty<QmlLibraryTrackListColumn>* p, qsizetype idx) {
        return reinterpret_cast<
                std::vector<parented_ptr<QmlLibraryTrackListColumn>>*>(p->data)
                ->at(idx)
                .get();
    }
    static void parent_qlist_clear(QQmlListProperty<QmlLibraryTrackListColumn>* p) {
        return reinterpret_cast<
                std::vector<parented_ptr<QmlLibraryTrackListColumn>>*>(p->data)
                ->clear();
    }
};

} // namespace qml
} // namespace mixxx

Q_DECLARE_METATYPE(mixxx::qml::QmlLibraryTrackListModel*)
