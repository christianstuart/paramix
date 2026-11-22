#pragma once

#include <QHash>
#include <QObject>
#include <memory>

#include "preferences/usersettings.h"

class EffectsManager;
class PadFxPreset;
class PadFxUnit;

/// PadFxManager owns the shared PadFxPreset and one PadFxUnit per primary
/// deck. It is intentionally independent of the controller layer: mappings use
/// the shared [PadFX] assignment controls and the per-deck [ChannelN_PadFX]
/// triggers.
class PadFxManager : public QObject {
    Q_OBJECT
  public:
    PadFxManager(UserSettingsPointer pConfig,
            EffectsManager* pEffectsManager,
            QObject* parent = nullptr);
    ~PadFxManager() override;

    void addDeck(const QString& deckGroup);
    void resetDeck(const QString& deckGroup);

  private:
    UserSettingsPointer m_pConfig;
    EffectsManager* m_pEffectsManager;
    std::unique_ptr<PadFxPreset> m_pPreset;
    QHash<QString, PadFxUnit*> m_units;
};
