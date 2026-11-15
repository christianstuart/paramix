#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QVariant>

#include "qmllibrarytracklistcolumn.h"
#include "qmlsidebarmodelproxy.h"
#include "util/parented_ptr.h"

namespace mixxx {
namespace qml {

/// Exposes the library sidebar, i.e. all sources of the real library
/// (tracks, playlists, crates, AutoDJ, browse, recordings, ...), to QML.
///
/// The set of sources is not configured in QML: it mirrors the features of the
/// library so that the QML library pane and the legacy QWidget library always
/// offer the same views. Only the track table columns are configured in QML.
class QmlLibrarySourceTree : public QQuickItem {
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QQmlListProperty<mixxx::qml::QmlLibraryTrackListColumn> defaultColumns READ
                    defaultColumns CONSTANT)
    QML_NAMED_ELEMENT(LibrarySourceTree)

  public:
    Q_DISABLE_COPY_MOVE(QmlLibrarySourceTree)
    explicit QmlLibrarySourceTree(QQuickItem* parent = nullptr);
    ~QmlLibrarySourceTree() override;

    void componentComplete() override;

    QQmlListProperty<mixxx::qml::QmlLibraryTrackListColumn> defaultColumns() {
        return {this, &m_defaultColumns};
    }

    Q_INVOKABLE mixxx::qml::QmlSidebarModelProxy* sidebar() const {
        return m_model.get();
    }
    Q_INVOKABLE mixxx::qml::QmlLibraryTrackListModel* allTracks() const;

  private:
    parented_ptr<QmlSidebarModelProxy> m_model;
    QList<QmlLibraryTrackListColumn*> m_defaultColumns;
};

} // namespace qml
} // namespace mixxx
