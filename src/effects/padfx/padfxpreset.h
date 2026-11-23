#pragma once

#include <QList>
#include <QObject>
#include <array>
#include <memory>

#include "effects/defs.h"
#include "effects/padfx/padfxdefs.h"
#include "preferences/usersettings.h"

class ControlPotmeter;
class ControlPushButton;
class EffectsManager;
class QJsonObject;

/// PadFxPreset owns the Pad FX assignments shared by every deck.
///
/// The [PadFX] controls hold two banks of eight pads. Each deck keeps its own
/// active bank and pad triggers; changing a pad here changes it everywhere.
class PadFxPreset : public QObject {
    Q_OBJECT
  public:
    PadFxPreset(UserSettingsPointer pConfig,
            EffectsManager* pEffectsManager,
            QObject* parent = nullptr);
    ~PadFxPreset() override;

    const mixxx::padfx::PadConfig& padConfig(int bankIndex, int pad) const;
    QList<EffectManifestPointer> effectManifests() const {
        return m_effects;
    }

  signals:
    void padChanged(int bankIndex, int pad);
    /// Only parameter values changed; a held or latched pad keeps playing.
    void padParamsChanged(int bankIndex, int pad);
    void allPadsChanged();

  private slots:
    void slotVisibleEffectsChanged();
    void slotEffectChanged(int bankIndex, int pad);
    void slotTargetChanged(int bankIndex, int pad);
    void slotModeChanged(int bankIndex, int pad);
    void slotParamChanged(int bankIndex, int pad, int param);

  private:
    struct BankControls {
        std::array<std::unique_ptr<ControlPotmeter>,
                mixxx::padfx::kNumPadsPerBank>
                effects;
        std::array<std::unique_ptr<ControlPotmeter>,
                mixxx::padfx::kNumPadsPerBank>
                targets;
        std::array<std::unique_ptr<ControlPushButton>,
                mixxx::padfx::kNumPadsPerBank>
                modes;
        std::array<std::array<std::unique_ptr<ControlPotmeter>,
                           mixxx::padfx::kMaxPadParams>,
                mixxx::padfx::kNumPadsPerBank>
                params;
    };

    static QString effectKey(int bankIndex, int pad);
    static QString targetKey(int bankIndex, int pad);
    static QString modeKey(int bankIndex, int pad);
    static QString paramKey(int bankIndex, int pad, int param);

    void createControls();
    void loadConfig();
    void saveConfig();
    void setDefaults();
    void migrateLegacy(const QJsonObject& legacy, bool useLegacyBank);
    void updateCatalog();
    void syncControls();
    void syncPadControls(int bankIndex, int pad);
    void setEffectDefaults(int bankIndex, int pad);
    int effectIndexForId(const QString& effectId) const;

    UserSettingsPointer m_pConfig;
    EffectsManager* m_pEffectsManager;
    mixxx::padfx::PresetConfig m_config;
    QList<EffectManifestPointer> m_effects;
    std::array<BankControls, mixxx::padfx::kNumBanks> m_controls;
    bool m_syncingControls = false;
};
