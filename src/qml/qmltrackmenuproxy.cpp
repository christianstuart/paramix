#include "qml/qmltrackmenuproxy.h"

#include <QAction>
#include <QCheckBox>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QTimer>
#include <QWidget>
#include <QWidgetAction>
#include <algorithm>
#include <functional>
#include <utility>

#include "library/library.h"
#include "library/trackmodel.h"
#include "moc_qmltrackmenuproxy.cpp"
#include "qml/qmlconfigproxy.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmllibrarytracklistmodel.h"
#include "util/color/rgbcolor.h"
#include "util/dnd.h"
#include "widget/wcolorpickeraction.h"
#include "widget/wstarratingaction.h"
#include "widget/wtrackmenu.h"

namespace mixxx {
namespace qml {

namespace {

void forwardAboutToShow(QMenu* pMenu) {
    // The classic menu populates its dynamic submenus (playlists, crates,
    // search related tracks, find on web) lazily from aboutToShow. Emitting the
    // signal is the supported way to force that population without showing the
    // menu, and WTrackMenu::updateMenus() resets the "already loaded" guards.
    QMetaObject::invokeMethod(pMenu, "aboutToShow", Qt::DirectConnection);
    const auto actions = pMenu->actions();
    for (QAction* pAction : actions) {
        if (QMenu* pSubMenu = pAction->menu()) {
            forwardAboutToShow(pSubMenu);
        }
    }
}

} // anonymous namespace

QmlLibraryTrackMenu::QmlLibraryTrackMenu(QObject* pParent)
        : QObject(pParent),
          m_pTrackModel(nullptr) {
}

QmlLibraryTrackMenu::~QmlLibraryTrackMenu() = default;

// static
QmlLibraryTrackMenu* QmlLibraryTrackMenu::create(
        QQmlEngine* pQmlEngine, QJSEngine*) {
    Q_UNUSED(pQmlEngine);
    return new QmlLibraryTrackMenu();
}

void QmlLibraryTrackMenu::ensureMenu(TrackModel* pTrackModel) {
    if (m_pMenu && m_pTrackModel == pTrackModel) {
        return;
    }
    m_pMenu.reset();
    m_actions.clear();
    m_checkBoxes.clear();
    m_pTrackModel = pTrackModel;

    Library* pLibrary = QmlLibraryProxy::get();
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pLibrary || !pConfig || !pTrackModel) {
        return;
    }

    if (!m_pAnchorWidget) {
        // WTrackMenu is a QWidget and needs a parent to resolve the screen,
        // but it is never shown. Serialization replaces the native popup.
        m_pAnchorWidget = std::make_unique<QWidget>();
        m_pAnchorWidget->setAttribute(Qt::WA_DontShowOnScreen);
        m_pAnchorWidget->setObjectName(QStringLiteral("QmlLibraryTrackMenuAnchor"));
    }

    m_pMenu = std::make_unique<WTrackMenu>(m_pAnchorWidget.get(),
            pConfig,
            pLibrary,
            WTrackMenu::Feature::All,
            pTrackModel);

    // Load to would otherwise be a silent no-op in the QML UI.
    connect(m_pMenu.get(),
            &WTrackMenu::loadTrackToPlayer,
            pLibrary,
            &Library::slotLoadTrackToPlayer);
    connect(m_pMenu.get(),
            &WTrackMenu::restoreCurrentViewStateOrIndex,
            this,
            &QmlLibraryTrackMenu::restoreSelectionRequested);
}

QVariantList QmlLibraryTrackMenu::open(
        QmlLibraryTrackListModel* pModel, const QList<int>& rows) {
    if (!selectTracks(pModel, rows)) {
        return {};
    }
    m_pMenu->updateMenus();
    forwardAboutToShow(m_pMenu.get());
    return serializeMenu(m_pMenu.get());
}

bool QmlLibraryTrackMenu::selectTracks(
        QmlLibraryTrackListModel* pModel, const QList<int>& rows) {
    m_actions.clear();
    m_checkBoxes.clear();
    if (!pModel) {
        return false;
    }
    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel->sourceModel());
    if (!pTrackModel) {
        return false;
    }
    ensureMenu(pTrackModel);
    if (!m_pMenu) {
        return false;
    }

    QModelIndexList trackIndices;
    trackIndices.reserve(rows.size());
    for (int row : rows) {
        const QModelIndex proxyIndex = pModel->index(row, 0);
        if (!proxyIndex.isValid()) {
            continue;
        }
        const QModelIndex sourceIndex = pModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) {
            continue;
        }
        trackIndices.append(sourceIndex.sibling(sourceIndex.row(), 0));
    }
    if (trackIndices.isEmpty()) {
        return false;
    }

    m_pMenu->loadTrackModelIndices(trackIndices);
    return true;
}

QVariantList QmlLibraryTrackMenu::serializeMenu(QMenu* pMenu) const {
    QVariantList nodes;
    if (!pMenu) {
        return nodes;
    }
    const auto actions = pMenu->actions();
    for (QAction* pAction : actions) {
        if (!pAction || !pAction->isVisible()) {
            continue;
        }
        if (pAction->isSeparator()) {
            if (nodes.isEmpty()) {
                continue;
            }
            if (nodes.constLast().toMap().value(QStringLiteral("kind")).toString() ==
                    QStringLiteral("separator")) {
                continue;
            }
            QVariantMap node;
            node.insert(QStringLiteral("kind"), QStringLiteral("separator"));
            nodes.append(node);
            continue;
        }

        QVariantMap node;
        node.insert(QStringLiteral("text"), stripMnemonics(pAction->text()));
        node.insert(QStringLiteral("enabled"), pAction->isEnabled());
        node.insert(QStringLiteral("checkable"), pAction->isCheckable());
        node.insert(QStringLiteral("checked"), pAction->isChecked());
        node.insert(QStringLiteral("shortcut"),
                pAction->shortcut().toString(QKeySequence::NativeText));
        node.insert(QStringLiteral("index"), -1);
        node.insert(QStringLiteral("checkIndex"), -1);

        if (qobject_cast<WStarRatingAction*>(pAction)) {
            node.insert(QStringLiteral("kind"), QStringLiteral("rating"));
            node.insert(QStringLiteral("rating"), m_pMenu ? m_pMenu->commonTrackRating() : 0);
            node.insert(QStringLiteral("enabled"), pAction->isEnabled());
        } else if (qobject_cast<WColorPickerAction*>(pAction)) {
            node.insert(QStringLiteral("kind"), QStringLiteral("color"));
            QString color;
            if (m_pMenu) {
                const auto commonColor = m_pMenu->commonTrackColor();
                if (commonColor) {
                    color = RgbColor::toQString(*commonColor);
                }
            }
            node.insert(QStringLiteral("color"), color);
        } else if (auto* pWidgetAction = qobject_cast<QWidgetAction*>(pAction)) {
            auto* pCheckBox = qobject_cast<QCheckBox*>(pWidgetAction->defaultWidget());
            if (!pCheckBox) {
                // Other embedded widgets (stem hint labels) have no QML
                // equivalent and are not interactive.
                continue;
            }
            node.insert(QStringLiteral("kind"), QStringLiteral("check"));
            node.insert(QStringLiteral("checked"), pCheckBox->isChecked());
            node.insert(QStringLiteral("checkState"),
                    static_cast<int>(pCheckBox->checkState()));
            node.insert(QStringLiteral("enabled"),
                    pAction->isEnabled() && pCheckBox->isEnabled());
            node.insert(QStringLiteral("checkIndex"), m_checkBoxes.size());
            m_checkBoxes.append(pCheckBox);
        } else {
            node.insert(QStringLiteral("kind"), QStringLiteral("action"));
            node.insert(QStringLiteral("index"), m_actions.size());
            m_actions.append(pAction);
        }

        if (QMenu* pSubMenu = pAction->menu()) {
            const QVariantList children = serializeMenu(pSubMenu);
            if (children.isEmpty()) {
                // Match the classic menu: empty submenus are not selectable.
                node.insert(QStringLiteral("enabled"), false);
            }
            node.insert(QStringLiteral("children"), children);
        }

        nodes.append(node);
    }

    while (!nodes.isEmpty() &&
            nodes.constLast().toMap().value(QStringLiteral("kind")).toString() ==
                    QStringLiteral("separator")) {
        nodes.removeLast();
    }
    return nodes;
}

void QmlLibraryTrackMenu::invoke(int index) {
    if (index < 0 || index >= m_actions.size()) {
        return;
    }
    const QPointer<QAction> pAction = m_actions.at(index);
    if (!pAction || !pAction->isEnabled()) {
        return;
    }
    // Defer: the triggered action can reset the library model while QML is
    // still unwinding its own trigger handler.
    QTimer::singleShot(0, pAction, [pAction]() {
        if (pAction && pAction->isEnabled()) {
            pAction->trigger();
        }
    });
}

void QmlLibraryTrackMenu::toggleCheckItem(int checkIndex) {
    if (checkIndex < 0 || checkIndex >= m_checkBoxes.size()) {
        return;
    }
    const QPointer<QCheckBox> pCheckBox = m_checkBoxes.at(checkIndex);
    if (!pCheckBox || !pCheckBox->isEnabled()) {
        return;
    }
    // The menu's connection to checkStateChanged performs the add/remove.
    pCheckBox->setCheckState(pCheckBox->checkState() == Qt::Checked
                    ? Qt::Unchecked
                    : Qt::Checked);
}

void QmlLibraryTrackMenu::setRating(int rating) {
    if (m_pMenu) {
        m_pMenu->setSelectionRating(rating);
    }
}

void QmlLibraryTrackMenu::showProperties(
        QmlLibraryTrackListModel* pModel, const QList<int>& rows) {
    if (!selectTracks(pModel, rows)) {
        return;
    }
    // Match WTrackTableView: properties are only editable for metadata models.
    if (!m_pTrackModel ||
            !m_pTrackModel->hasCapabilities(
                    TrackModel::Capability::EditMetadata)) {
        return;
    }
    m_pMenu->slotShowDlgTrackInfo();
}

void QmlLibraryTrackMenu::setColor(const QString& hexColor) {
    if (!m_pMenu) {
        return;
    }
    if (hexColor.isEmpty()) {
        m_pMenu->setSelectionColor(RgbColor::nullopt());
    } else {
        m_pMenu->setSelectionColor(RgbColor::fromQString(hexColor));
    }
}

void QmlLibraryTrackMenu::removeSelectedRows(
        QmlLibraryTrackListModel* pModel, const QList<int>& rows) {
    if (!pModel || rows.isEmpty()) {
        return;
    }
    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel->sourceModel());
    if (!pTrackModel || pTrackModel->isLocked()) {
        return;
    }

    // Same capability order and confirmation as
    // WTrackTableView::hideOrRemoveSelectedTracks(): Remove wins for the Auto
    // DJ queue, then crates, playlists and finally hiding from the library.
    TrackModel::Capability capability = TrackModel::Capability::None;
    if (pTrackModel->hasCapabilities(TrackModel::Capability::Remove)) {
        capability = TrackModel::Capability::Remove;
    } else if (pTrackModel->hasCapabilities(
                       TrackModel::Capability::RemoveCrate)) {
        capability = TrackModel::Capability::RemoveCrate;
    } else if (pTrackModel->hasCapabilities(
                       TrackModel::Capability::RemovePlaylist)) {
        capability = TrackModel::Capability::RemovePlaylist;
    } else if (pTrackModel->hasCapabilities(TrackModel::Capability::Hide)) {
        capability = TrackModel::Capability::Hide;
    } else {
        return;
    }

    if (pTrackModel->getRequireConfirmationToHideRemoveTracks()) {
        QString title;
        QString message;
        if (capability == TrackModel::Capability::Hide) {
            title = tr("Confirm track hide");
            message = tr("Are you sure you want to hide the selected tracks?");
        } else {
            title = tr("Confirm track removal");
            if (capability == TrackModel::Capability::Remove) {
                message = tr("Are you sure you want to remove the selected "
                             "tracks from AutoDJ queue?");
            } else if (capability == TrackModel::Capability::RemoveCrate) {
                message = tr("Are you sure you want to remove the selected "
                             "tracks from this crate?");
            } else {
                message = tr("Are you sure you want to remove the selected "
                             "tracks from this playlist?");
            }
        }

        QMessageBox msg;
        msg.setIcon(QMessageBox::Question);
        msg.setWindowTitle(title);
        msg.setText(message);
        QCheckBox notAgainCB(tr("Don't ask again during this session"));
        notAgainCB.setCheckState(Qt::Unchecked);
        msg.setCheckBox(&notAgainCB);
        msg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msg.setDefaultButton(QMessageBox::Cancel);
        if (msg.exec() != QMessageBox::Ok) {
            return;
        }
        if (notAgainCB.isChecked()) {
            pTrackModel->setRequireConfirmationToHideRemoveTracks(false);
        }
    }

    QModelIndexList indices;
    indices.reserve(rows.size());
    for (int row : rows) {
        const QModelIndex sourceIndex = pModel->mapToSource(pModel->index(row, 0));
        if (sourceIndex.isValid()) {
            indices.append(sourceIndex.sibling(sourceIndex.row(), 0));
        }
    }
    if (indices.isEmpty()) {
        return;
    }
    if (capability == TrackModel::Capability::Hide) {
        pTrackModel->hideTracks(indices);
    } else {
        pTrackModel->removeTracks(indices);
    }
}

void QmlLibraryTrackMenu::moveSelectedRows(
        QmlLibraryTrackListModel* pModel,
        const QList<int>& rows,
        int destinationRow) {
    if (!pModel) {
        return;
    }
    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel->sourceModel());
    if (!pTrackModel || pTrackModel->isLocked() ||
            !pTrackModel->hasCapabilities(TrackModel::Capability::Reorder)) {
        return;
    }

    const int rowCount = pModel->rowCount();
    QList<int> selectedRows;
    selectedRows.reserve(rows.size());
    for (int row : rows) {
        if (row >= 0 && row < rowCount) {
            selectedRows.append(row);
        }
    }
    if (selectedRows.isEmpty()) {
        return;
    }
    std::sort(selectedRows.begin(), selectedRows.end());

    // Same block semantics as WTrackTableView::moveRows(): a drop inside the
    // block is a no-op, and rows are moved one by one so the row numbers stay
    // valid while the model shifts.
    int destination = destinationRow < 0 ? rowCount : destinationRow;
    destination = std::min(destination, rowCount);
    int firstSelRow = selectedRows.first();
    const int lastSelRow = selectedRows.last();
    if (destination == firstSelRow && selectedRows.size() == 1) {
        return;
    }
    if (destination >= firstSelRow && destination <= lastSelRow) {
        if (selectedRows.size() == lastSelRow - firstSelRow + 1) {
            return;
        }
        if (destination != firstSelRow) {
            return;
        }
        // Non-contiguous selection dropped on its first row: use the first gap.
        while (!selectedRows.isEmpty() && destination == firstSelRow) {
            selectedRows.removeFirst();
            if (selectedRows.isEmpty()) {
                return;
            }
            firstSelRow = selectedRows.first();
            ++destination;
        }
    } else if (destination < firstSelRow) {
        // Move up: process the rows in reverse so the earliest keeps its slot.
        std::sort(selectedRows.begin(), selectedRows.end(), std::greater<int>());
    }

    while (!selectedRows.isEmpty()) {
        const int movedRow = selectedRows.takeFirst();
        pTrackModel->moveTrack(
                pModel->mapToSource(pModel->index(movedRow, 0)),
                pModel->mapToSource(pModel->index(destination, 0)));
        for (int i = 0; i < selectedRows.size(); ++i) {
            if (selectedRows[i] > movedRow && destination > selectedRows[i]) {
                --selectedRows[i];
            } else if (selectedRows[i] < movedRow &&
                    destination < selectedRows[i]) {
                ++selectedRows[i];
            }
        }
    }
}

int QmlLibraryTrackMenu::importDroppedUrls(
        QmlLibraryTrackListModel* pModel,
        const QList<QUrl>& urls,
        int destinationRow) {
    if (!pModel || urls.isEmpty()) {
        return 0;
    }
    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel->sourceModel());
    if (!pTrackModel || pTrackModel->isLocked() ||
            !pTrackModel->hasCapabilities(TrackModel::Capability::ReceiveDrops)) {
        return 0;
    }
    // Expands folders and playlist files and filters out unsupported files,
    // the same way a drop on the classic track table does.
    const QList<mixxx::FileInfo> fileInfos =
            DragAndDropHelper::supportedTracksFromUrls(urls, false, true);
    if (fileInfos.isEmpty()) {
        return 0;
    }
    QList<QString> locations;
    locations.reserve(fileInfos.size());
    for (const auto& fileInfo : std::as_const(fileInfos)) {
        locations.append(fileInfo.location());
    }
    // An invalid destination index appends, which is also what dropping below
    // the last row (destinationRow == rowCount) ends up as.
    const QModelIndex destination = destinationRow < 0
            ? QModelIndex()
            : pModel->mapToSource(pModel->index(destinationRow, 0));
    return pTrackModel->addTracks(destination, locations);
}

// static
QString QmlLibraryTrackMenu::stripMnemonics(const QString& text) {
    QString result;
    result.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('&')) {
            if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('&')) {
                result.append(QLatin1Char('&'));
                ++i;
            }
            // Single '&' is a mnemonic marker and is dropped.
            continue;
        }
        result.append(c);
    }
    return result;
}

// static
void QmlLibraryTrackMenu::populateLazySubmenus(QMenu* pMenu) {
    forwardAboutToShow(pMenu);
}

} // namespace qml
} // namespace mixxx
