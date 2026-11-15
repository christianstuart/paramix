#include "qml/qmltrackmenuproxy.h"

#include <QAction>
#include <QCheckBox>
#include <QKeySequence>
#include <QMenu>
#include <QMetaObject>
#include <QTimer>
#include <QWidget>
#include <QWidgetAction>

#include "library/library.h"
#include "library/trackmodel.h"
#include "moc_qmltrackmenuproxy.cpp"
#include "qml/qmlconfigproxy.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmllibrarytracklistmodel.h"
#include "util/color/rgbcolor.h"
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
    m_actions.clear();
    m_checkBoxes.clear();
    if (!pModel) {
        return {};
    }
    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel->sourceModel());
    if (!pTrackModel) {
        return {};
    }
    ensureMenu(pTrackModel);
    if (!m_pMenu) {
        return {};
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
        return {};
    }

    m_pMenu->loadTrackModelIndices(trackIndices);
    m_pMenu->updateMenus();
    forwardAboutToShow(m_pMenu.get());
    return serializeMenu(m_pMenu.get());
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
