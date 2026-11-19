#pragma once

#include <QObject>
#include <array>
#include <memory>

#include "effects/padfx/padfxdefs.h"
#include "preferences/usersettings.h"

class ControlObject;
class ControlPotmeter;
class ControlProxy;
class ControlPushButton;
class EffectsManager;
class EffectSlot;
class QTimer;

/// PadFxUnit owns the [ChannelN_PadFX] controls for one deck.
///
/// It is a control-thread translator, not a DSP object. It loads the selected
/// bank's four effects into the per-stem PadEffectChains and translates pad
/// presses into effect `active` gates and, after the audible gesture, native
/// stem mutes.
class PadFxUnit : public QObject {
    Q_OBJECT
  public:
    PadFxUnit(const QString& deckGroup,
            UserSettingsPointer pConfig,
            EffectsManager* pEffectsManager,
            QObject* parent = nullptr);
    ~PadFxUnit() override;

    const QString& deckGroup() const {
        return m_deckGroup;
    }

    void resetForTrackLoad();

  private slots:
    void slotBankChanged(double value);
    void slotPadChanged(int pad);
    void slotEffectChanged(int pad);
    void slotTargetChanged(int pad);
    void slotModeChanged(int pad);
    void slotParamChanged(int pad, int param);
    void slotStemCountChanged(double value);
    void finishPad(int pad);

  private:
    struct PadRuntime {
        bool down = false;
        bool gestureActive = false;
        bool muteOwned = false;
        std::array<int, mixxx::padfx::kNumStems> savedMute{0, 0, 0, 0};
    };

    QString stemGroup(int stemIndex) const;
    EffectSlot* slotFor(int stemIndex, int pad) const;
    void loadConfig();
    void saveConfig();
    void syncControlsFromConfig();
    void unloadAll();
    void unloadPad(int pad);
    void loadPad(int pad);
    void applyBank();
    void applyPadParameters(int pad);
    void setEffectActive(int pad, bool active);
    void setPadActiveControl(int pad, bool active);
    void restoreMuteOwned(int pad);
    void stopPad(int pad);
    int stemCount() const;
    double padDurationSeconds(int pad) const;
    double bpm() const;

    const QString m_deckGroup;
    const QString m_controlGroup;
    UserSettingsPointer m_pConfig;
    EffectsManager* m_pEffectsManager;

    mixxx::padfx::DeckConfig m_config;
    std::array<PadRuntime, mixxx::padfx::kNumPadsPerBank> m_runtime;
    // Effect id currently loaded into (pad, stem).
    std::array<std::array<QString, mixxx::padfx::kNumStems>,
            mixxx::padfx::kNumPadsPerBank>
            m_loadedEffectIds;

    std::unique_ptr<ControlPotmeter> m_pBank;
    std::array<std::unique_ptr<ControlPushButton>, mixxx::padfx::kNumPadsPerBank> m_pPads;
    std::array<std::unique_ptr<ControlObject>, mixxx::padfx::kNumPadsPerBank>
            m_pPadActive;
    std::array<std::unique_ptr<ControlObject>, mixxx::padfx::kNumPadsPerBank>
            m_pPadMuted;
    std::array<std::unique_ptr<ControlPotmeter>, mixxx::padfx::kNumPadsPerBank>
            m_pEffects;
    std::array<std::unique_ptr<ControlPotmeter>, mixxx::padfx::kNumPadsPerBank>
            m_pTargets;
    std::array<std::unique_ptr<ControlPotmeter>, mixxx::padfx::kNumPadsPerBank>
            m_pModes;
    std::array<std::array<std::unique_ptr<ControlPotmeter>, 5>,
            mixxx::padfx::kNumPadsPerBank>
            m_pParams;

    std::unique_ptr<ControlProxy> m_pStemCount;
    std::unique_ptr<ControlProxy> m_pBpm;
    std::array<std::unique_ptr<ControlProxy>, mixxx::padfx::kNumStems> m_pStemMute;
    std::array<QTimer*, mixxx::padfx::kNumPadsPerBank> m_padTimers{nullptr, nullptr, nullptr, nullptr};

    int m_appliedStemCount = -1;
    bool m_syncingControls = false;
};
