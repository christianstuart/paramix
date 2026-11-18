#pragma once

#include <QHash>
#include <QObject>
#include <memory>

#include "preferences/usersettings.h"

class EffectsManager;
class PadFxUnit;

/// PadFxManager owns one PadFxUnit per primary deck. It is intentionally
/// independent of the controller layer: mappings only need the stable
/// [ChannelN_PadFX] controls.
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
    QHash<QString, PadFxUnit*> m_units;
};
