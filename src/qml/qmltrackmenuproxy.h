#pragma once

#include <qqmlintegration.h>

#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>
#include <memory>

#include "qml/qmllibrarytracklistmodel.h"
#include "util/color/rgbcolor.h"

class QAction;
class QCheckBox;
class QMenu;
class QWidget;
class TrackModel;
class WTrackMenu;

namespace mixxx {
namespace qml {

/// QML bridge for the classic WTrackMenu.
///
/// Instead of duplicating the ~40 actions of the classic track context menu in
/// QML, this proxy builds the real WTrackMenu for the current selection and
/// serializes its QAction tree into a nested list of plain objects. QML renders
/// that tree with its own styled Menu components and calls invoke() for the
/// selected entry, so behaviour, enablement, dialogs and database updates are
/// exactly the ones of the classic UI.
class QmlLibraryTrackMenu : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryTrackMenu)
    QML_SINGLETON

  public:
    explicit QmlLibraryTrackMenu(QObject* pParent = nullptr);
    ~QmlLibraryTrackMenu() override;

    static QmlLibraryTrackMenu* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    /// Prepare the menu for the given rows of the model and return the action
    /// tree. Each node is a map with the keys:
    ///   kind:      "action" | "separator" | "rating" | "color" | "check"
    ///   text:      label with mnemonic markers removed
    ///   shortcut:  native shortcut text (may be empty)
    ///   enabled, checkable, checked: QAction state
    ///   index:     index to pass to invoke() for kind == "action"
    ///   checkIndex: index to pass to toggleCheckItem() for kind == "check"
    ///   checkState: Qt::CheckState (0/1/2) for kind == "check"
    ///   rating:    current common rating for kind == "rating"
    ///   color:     #RRGGBB of the common color for kind == "color" ("" if none)
    ///   children:  nested nodes for submenus
    Q_INVOKABLE QVariantList open(QmlLibraryTrackListModel* pModel,
            const QList<int>& rows);

    Q_INVOKABLE void invoke(int index);
    Q_INVOKABLE void toggleCheckItem(int checkIndex);
    Q_INVOKABLE void setRating(int rating);
    /// Empty string clears the color.
    Q_INVOKABLE void setColor(const QString& hexColor);
    /// Opens the classic Track Properties dialog for the rows (Ctrl+Enter).
    Q_INVOKABLE void showProperties(QmlLibraryTrackListModel* pModel,
            const QList<int>& rows);
    /// Removes (or hides, for the main library) the rows with the same
    /// confirmation the classic track table uses.
    Q_INVOKABLE void removeSelectedRows(QmlLibraryTrackListModel* pModel,
            const QList<int>& rows);
    /// Moves the rows to `destinationRow` (drag and drop reordering of a
    /// playlist). Ignored by sources without the reorder capability.
    Q_INVOKABLE void moveSelectedRows(QmlLibraryTrackListModel* pModel,
            const QList<int>& rows,
            int destinationRow);
    /// Imports the supported audio files and folders below `urls` into the
    /// track list's source (and the library). `destinationRow` inserts before
    /// that row; a negative row or one past the end appends. Returns the
    /// number of added tracks; 0 means nothing usable was dropped.
    Q_INVOKABLE int importDroppedUrls(QmlLibraryTrackListModel* pModel,
            const QList<QUrl>& urls,
            int destinationRow = -1);

  signals:
    /// Emitted when an action changed the underlying model and the caller
    /// should restore a usable selection.
    void restoreSelectionRequested();

  private:
    void ensureMenu(TrackModel* pTrackModel);
    /// Loads the source indexes of `rows` into the menu. Returns false if
    /// no usable row was left.
    bool selectTracks(QmlLibraryTrackListModel* pModel, const QList<int>& rows);
    QVariantList serializeMenu(QMenu* pMenu) const;
    static void populateLazySubmenus(QMenu* pMenu);
    static QString stripMnemonics(const QString& text);

    std::unique_ptr<QWidget> m_pAnchorWidget;
    std::unique_ptr<WTrackMenu> m_pMenu;
    // Not a QObject, so it cannot be tracked by a QPointer. The model is owned
    // by the library feature and outlives the menu.
    TrackModel* m_pTrackModel;

    // QML-safe handles into the current menu. Rebuilt on every open().
    mutable QList<QPointer<QAction>> m_actions;
    mutable QList<QPointer<QCheckBox>> m_checkBoxes;
};

} // namespace qml
} // namespace mixxx
