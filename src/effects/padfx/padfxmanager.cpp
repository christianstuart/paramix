#include "effects/padfx/padfxmanager.h"

#include "effects/padfx/padfxunit.h"
#include "moc_padfxmanager.cpp"
#include "util/assert.h"

PadFxManager::PadFxManager(UserSettingsPointer pConfig,
        EffectsManager* pEffectsManager,
        QObject* parent)
        : QObject(parent),
          m_pConfig(std::move(pConfig)),
          m_pEffectsManager(pEffectsManager) {
    DEBUG_ASSERT(m_pEffectsManager != nullptr);
}

PadFxManager::~PadFxManager() {
    qDeleteAll(m_units);
    m_units.clear();
}

void PadFxManager::addDeck(const QString& deckGroup) {
    if (m_units.contains(deckGroup)) {
        return;
    }
    auto* pUnit = new PadFxUnit(deckGroup, m_pConfig, m_pEffectsManager, this);
    m_units.insert(deckGroup, pUnit);
}

void PadFxManager::resetDeck(const QString& deckGroup) {
    PadFxUnit* pUnit = m_units.value(deckGroup);
    if (pUnit) {
        pUnit->resetForTrackLoad();
    }
}
