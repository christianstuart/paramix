#pragma once

#include <QObject>
#include <array>
#include <memory>

#include "effects/defs.h"
#include "effects/padfx/padfxdefs.h"
#include "preferences/usersettings.h"

class ControlObject;
class ControlPotmeter;
class ControlProxy;
class ControlPushButton;
class EffectsManager;
class EffectSlot;
class PadFxPreset;

/// PadFxUnit owns the per-deck [ChannelN_PadFX] controls.
///
/// Every pad is momentary: the shared preset's effect is enabled while the pad
/// is held and released when it is let go. It is a control-thread translator,
/// not a DSP object.
class PadFxUnit : public QObject {
    Q_OBJECT
  public:
    PadFxUnit(const QString& deckGroup,
            UserSettingsPointer pConfig,
            EffectsManager* pEffectsManager,
            PadFxPreset* pPreset,
            QObject* parent = nullptr);
    ~PadFxUnit() override;

    const QString& deckGroup() const {
        return m_deckGroup;
    }

    void resetForTrackLoad();

  private slots:
    void slotBankChanged(double value);
    void slotPadChanged(int pad);
    void slotStemCountChanged(double value);
    void slotPresetPadChanged(int bankIndex, int pad);
    void slotPresetAllChanged();

  private:
    struct PadRuntime {
        bool down = false;
        bool active = false;
    };

    QString stemGroup(int stemIndex) const;
    EffectSlot* slotFor(int stemIndex, int pad) const;
    EffectSlot* deckSlotFor(int pad) const;
    EffectSlot* slotForTarget(int target, int pad) const;
    EffectManifestPointer manifestForId(const QString& effectId) const;
    int bankIndex() const;
    bool targetAvailable(int targetMask) const;
    void loadBank();
    void loadPad(int pad);
    void unloadAll();
    void unloadPad(int pad);
    void applyPadParameters(int pad);
    void setEffectActive(int pad, bool active);
    void setPadActiveControl(int pad, bool active);
    void stopAllPads();
    int stemCount() const;
    void loadBankConfig();
    void saveBankConfig();

    const QString m_deckGroup;
    const QString m_controlGroup;
    UserSettingsPointer m_pConfig;
    EffectsManager* m_pEffectsManager;
    PadFxPreset* m_pPreset;

    int m_bank = 1;
    std::array<PadRuntime, mixxx::padfx::kNumPadsPerBank> m_runtime;
    // Effect id currently loaded into (pad, stem).
    std::array<std::array<QString, mixxx::padfx::kNumStems>,
            mixxx::padfx::kNumPadsPerBank>
            m_loadedEffectIds;
    std::array<QString, mixxx::padfx::kNumPadsPerBank> m_loadedDeckEffectIds;

    std::unique_ptr<ControlPotmeter> m_pBank;
    std::array<std::unique_ptr<ControlPushButton>, mixxx::padfx::kNumPadsPerBank> m_pPads;
    std::array<std::unique_ptr<ControlObject>, mixxx::padfx::kNumPadsPerBank>
            m_pPadActive;

    std::unique_ptr<ControlProxy> m_pStemCount;

    int m_appliedStemCount = -1;
    bool m_syncingControls = false;
};
