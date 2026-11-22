#include "qml/qmlvisibleeffectsmodel.h"

#include <QModelIndex>
#include <algorithm>
#include <cmath>

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
        {QmlVisibleEffectsModel::ParametersRole, "parameters"},
        {QmlVisibleEffectsModel::PadFxOnlyRole, "padFxOnly"},
        {QmlVisibleEffectsModel::BeatStepLabelsRole, "beatStepLabels"},
        {QmlVisibleEffectsModel::BeatStepValuesRole, "beatStepValues"},
        {QmlVisibleEffectsModel::BeatStepParameterIndexRole, "beatStepParameterIndex"},
};

QString beatStepLabel(double value, EffectManifest::BeatFxStepUnit unit) {
    if (unit == EffectManifest::BeatFxStepUnit::Percent) {
        return QString::number(qRound(value * 100.0)) + QLatin1Char('%');
    }
    if (std::abs(value - std::round(value)) < 1e-9) {
        return QString::number(qRound(value));
    }
    for (int denominator : {2, 3, 4, 8, 16}) {
        const double numerator = value * denominator;
        if (std::abs(numerator - std::round(numerator)) < 1e-9) {
            return QStringLiteral("%1/%2")
                    .arg(qRound(numerator))
                    .arg(denominator);
        }
    }
    return QString::number(value);
}

int knobParameterIndex(const EffectManifestPointer& pManifest, const QString& id) {
    int index = 0;
    for (const auto& pParameter : pManifest->parameters()) {
        if (pParameter->parameterType() !=
                        EffectManifestParameter::ParameterType::Knob ||
                pParameter->id() == QStringLiteral("active")) {
            continue;
        }
        if (pParameter->id() == id) {
            return index;
        }
        ++index;
    }
    return -1;
}

QStringList parameterNames(const EffectManifestPointer& pManifest) {
    QStringList names;
    for (const auto& pParameter : pManifest->parameters()) {
        if (pParameter->parameterType() !=
                        EffectManifestParameter::ParameterType::Knob ||
                pParameter->id() == QStringLiteral("active")) {
            continue;
        }
        names.append(pParameter->shortName().isEmpty() ? pParameter->name()
                                                       : pParameter->shortName());
    }
    return names;
}
} // namespace

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
    if (m_filter == Filter::PadFx) {
        m_visibleEffectManifests = m_pEffectsManager->getPadEffectManifests();
    } else {
        for (const auto& pManifest :
                m_pEffectsManager->getVisibleEffectsList()->getList()) {
            const bool isBeat = pManifest->isBeatFx();
            if (m_filter == Filter::All ||
                    (m_filter == Filter::Beat) == isBeat) {
                m_visibleEffectManifests.append(pManifest);
            }
        }
    }
    endResetModel();
}

QVariant QmlVisibleEffectsModel::data(const QModelIndex& index, int role) const {
    if (index.row() == 0) {
        switch (role) {
        case Qt::DisplayRole:
            return m_filter == Filter::PadFx ? tr("Off") : kNoEffectString;
        case Qt::ToolTipRole:
            return tr("No effect loaded.");
        case QmlVisibleEffectsModel::IsBeatFxRole:
            return false;
        case QmlVisibleEffectsModel::ParametersRole:
        case QmlVisibleEffectsModel::BeatStepLabelsRole:
        case QmlVisibleEffectsModel::BeatStepValuesRole:
            return QStringList();
        case QmlVisibleEffectsModel::PadFxOnlyRole:
            return false;
        case QmlVisibleEffectsModel::BeatStepParameterIndexRole:
            return -1;
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
        // Pad FX show the full names of their two pad-only effects; every other
        // entry keeps the short name used by the normal effect selectors.
        return m_filter == Filter::PadFx && pManifest->isPadFxOnly()
                ? pManifest->name()
                : pManifest->displayName();
    case Qt::ToolTipRole:
        return pManifest->description();
    case QmlVisibleEffectsModel::EffectIdRole:
        return pManifest->id();
    case QmlVisibleEffectsModel::IsBeatFxRole:
        return pManifest->isBeatFx();
    case QmlVisibleEffectsModel::ParametersRole:
        return parameterNames(pManifest);
    case QmlVisibleEffectsModel::PadFxOnlyRole:
        return pManifest->isPadFxOnly();
    case QmlVisibleEffectsModel::BeatStepLabelsRole: {
        QStringList labels;
        if (!pManifest->isBeatFx()) {
            return labels;
        }
        for (double step : pManifest->beatFxSteps()) {
            labels.append(beatStepLabel(step, pManifest->beatFxStepUnit()));
        }
        return labels;
    }
    case QmlVisibleEffectsModel::BeatStepValuesRole: {
        if (!pManifest->isBeatFx()) {
            return QVariantList();
        }
        QVariantList normalized;
        const QString stepId = pManifest->beatFxStepParameterId();
        EffectManifestParameterPointer pStepParameter;
        for (const auto& pParameter : pManifest->parameters()) {
            if (pParameter->id() == stepId) {
                pStepParameter = pParameter;
                break;
            }
        }
        if (!pStepParameter) {
            return normalized;
        }
        const double minimum = pStepParameter->getMinimum();
        const double maximum = pStepParameter->getMaximum();
        const double range = maximum - minimum;
        for (double step : pManifest->beatFxSteps()) {
            const double normalizedStep = range > 0.0
                    ? (step - minimum) / range
                    : 0.0;
            normalized.append(std::clamp(normalizedStep, 0.0, 1.0));
        }
        return normalized;
    }
    case QmlVisibleEffectsModel::BeatStepParameterIndexRole:
        return pManifest->isBeatFx()
                ? knobParameterIndex(pManifest,
                          pManifest->beatFxStepParameterId())
                : -1;
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
