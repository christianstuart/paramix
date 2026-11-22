#include "qml/qmlvisibleeffectsmodel.h"

#include <QModelIndex>

#include "effects/backends/effectmanifest.h"
#include "effects/defs.h"
#include "effects/effectsmanager.h"
#include "effects/visibleeffectslist.h"
#include "moc_qmlvisibleeffectsmodel.cpp"

namespace mixxx {
namespace qml {
namespace {
const QHash<int, QByteArray> kRoleNames = {
        {Qt::DisplayRole, "display"},
        {Qt::ToolTipRole, "tooltip"},
        {QmlVisibleEffectsModel::EffectIdRole, "effectId"},
        {QmlVisibleEffectsModel::IsBeatFxRole, "isBeatFx"},
};
}

QmlVisibleEffectsModel::QmlVisibleEffectsModel(
        std::shared_ptr<EffectsManager> pEffectsManager,
        Filter filter,
        QObject* parent)
        : QAbstractListModel(parent),
          m_pEffectsManager(pEffectsManager),
          m_filter(filter) {
    slotVisibleEffectsUpdated();
    connect(m_pEffectsManager->getVisibleEffectsList().get(),
            &VisibleEffectsList::visibleEffectsListChanged,
            this,
            &QmlVisibleEffectsModel::slotVisibleEffectsUpdated);
}

void QmlVisibleEffectsModel::slotVisibleEffectsUpdated() {
    beginResetModel();
    m_visibleEffectManifests.clear();
    for (const auto& pManifest :
            m_pEffectsManager->getVisibleEffectsList()->getList()) {
        const bool isBeat = pManifest->isBeatFx();
        if (m_filter == Filter::All ||
                (m_filter == Filter::Beat) == isBeat) {
            m_visibleEffectManifests.append(pManifest);
        }
    }
    endResetModel();
}

QVariant QmlVisibleEffectsModel::data(const QModelIndex& index, int role) const {
    if (index.row() == 0) {
        switch (role) {
        case Qt::DisplayRole:
            return kNoEffectString;
        case Qt::ToolTipRole:
            return tr("No effect loaded.");
        case QmlVisibleEffectsModel::IsBeatFxRole:
            return false;
        default:
            return QVariant();
        }
    }

    if (index.row() > m_visibleEffectManifests.size()) {
        return QVariant();
    }

    const EffectManifestPointer pManifest = m_visibleEffectManifests.at(index.row() - 1);
    switch (role) {
    case Qt::DisplayRole:
        return pManifest->displayName();
    case Qt::ToolTipRole:
        return pManifest->description();
    case QmlVisibleEffectsModel::EffectIdRole:
        return pManifest->id();
    case QmlVisibleEffectsModel::IsBeatFxRole:
        return pManifest->isBeatFx();
    default:
        return QVariant();
    }
}

int QmlVisibleEffectsModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }

    // Add +1 because we also include "no effect" in the model
    return m_visibleEffectManifests.size() + 1;
}

QHash<int, QByteArray> QmlVisibleEffectsModel::roleNames() const {
    return kRoleNames;
}

QVariant QmlVisibleEffectsModel::get(int row) const {
    QModelIndex idx = index(row, 0);
    QVariantMap dataMap;
    for (auto it = kRoleNames.constBegin(); it != kRoleNames.constEnd(); it++) {
        dataMap.insert(it.value(), data(idx, it.key()));
    }
    return dataMap;
}

} // namespace qml
} // namespace mixxx
