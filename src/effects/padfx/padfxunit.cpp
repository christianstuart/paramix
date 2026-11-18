#include "effects/padfx/padfxunit.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <algorithm>
#include <cmath>

#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "effects/backends/effectmanifest.h"
#include "effects/backends/effectsbackendmanager.h"
#include "effects/effectchain.h"
#include "effects/effectparameter.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "moc_padfxunit.cpp"
#include "util/assert.h"

namespace {
using mixxx::padfx::PadEffect;
using mixxx::padfx::PadMode;

constexpr const char* kConfigGroup = "[PadFX]";

QString configKeyForDeck(const QString& deckGroup) {
    QString digits;
    for (const QChar c : deckGroup) {
        if (c.isDigit()) {
            digits.append(c);
        }
    }
    return QStringLiteral("Deck%1").arg(digits.isEmpty() ? deckGroup : digits);
}

int clampInt(int value, int low, int high) {
    return std::clamp(value, low, high);
}

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}
} // namespace

PadFxUnit::PadFxUnit(const QString& deckGroup,
        UserSettingsPointer pConfig,
        EffectsManager* pEffectsManager,
        QObject* parent)
        : QObject(parent),
          m_deckGroup(deckGroup),
          m_controlGroup(deckGroup.chopped(1) + QStringLiteral("_PadFX]")),
          m_pConfig(pConfig),
          m_pEffectsManager(pEffectsManager) {
    DEBUG_ASSERT(m_pEffectsManager != nullptr);

    m_pBank = std::make_unique<ControlPotmeter>(
            ConfigKey(m_controlGroup, QStringLiteral("bank")), 1.0, 4.0, false, true, false, false, 1.0);
    m_pBank->setStepCount(3);
    m_pBank->set(1.0);

    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        const QString padPrefix = QStringLiteral("pad%1").arg(pad + 1);
        m_pPads[pad] = std::make_unique<ControlPushButton>(
                ConfigKey(m_controlGroup, padPrefix));
        m_pPads[pad]->setButtonMode(mixxx::control::ButtonMode::Push);

        m_pPadActive[pad] = std::make_unique<ControlObject>(
                ConfigKey(m_controlGroup, padPrefix + QStringLiteral("_active")));
        m_pPadActive[pad]->setReadOnly();
        m_pPadMuted[pad] = std::make_unique<ControlObject>(
                ConfigKey(m_controlGroup, padPrefix + QStringLiteral("_muted")));
        m_pPadMuted[pad]->setReadOnly();

        m_pEffects[pad] = std::make_unique<ControlPotmeter>(
                ConfigKey(m_controlGroup, QStringLiteral("effect%1").arg(pad + 1)),
                0.0,
                2.0,
                false,
                true,
                false,
                false,
                0.0);
        m_pEffects[pad]->setStepCount(2);
        m_pTargets[pad] = std::make_unique<ControlPotmeter>(
                ConfigKey(m_controlGroup, QStringLiteral("target%1").arg(pad + 1)),
                1.0,
                15.0,
                false,
                true,
                false,
                false,
                1.0);
        m_pModes[pad] = std::make_unique<ControlPotmeter>(
                ConfigKey(m_controlGroup, QStringLiteral("mode%1").arg(pad + 1)),
                0.0,
                2.0,
                false,
                true,
                false,
                false,
                1.0);
        m_pModes[pad]->setStepCount(2);
        for (int param = 0; param < 5; ++param) {
            m_pParams[pad][param] = std::make_unique<ControlPotmeter>(
                    ConfigKey(m_controlGroup,
                            QStringLiteral("pad%1_param%2").arg(pad + 1).arg(param + 1)),
                    0.0,
                    1.0,
                    false,
                    true,
                    false,
                    false,
                    0.5);
        }

        m_padTimers[pad] = new QTimer(this);
        m_padTimers[pad]->setSingleShot(true);
        connect(m_padTimers[pad], &QTimer::timeout, this, [this, pad]() {
            finishPad(pad);
        });
    }

    m_pStemCount = std::make_unique<ControlProxy>(
            ConfigKey(m_deckGroup, QStringLiteral("stem_count")), this);
    m_pBpm = std::make_unique<ControlProxy>(
            ConfigKey(m_deckGroup, QStringLiteral("bpm")), this);
    for (int stem = 0; stem < mixxx::padfx::kNumStems; ++stem) {
        m_pStemMute[stem] = std::make_unique<ControlProxy>(
                ConfigKey(stemGroup(stem), QStringLiteral("mute")), this);
    }

    loadConfig();
    syncControlsFromConfig();

    connect(m_pBank.get(), &ControlObject::valueChanged, this, [this](double value) {
        slotBankChanged(value);
    });
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        connect(m_pPads[pad].get(), &ControlObject::valueChanged, this, [this, pad](double) {
            slotPadChanged(pad);
        });
        connect(m_pEffects[pad].get(), &ControlObject::valueChanged, this, [this, pad](double) {
            slotEffectChanged(pad);
        });
        connect(m_pTargets[pad].get(), &ControlObject::valueChanged, this, [this, pad](double) {
            slotTargetChanged(pad);
        });
        connect(m_pModes[pad].get(), &ControlObject::valueChanged, this, [this, pad](double) {
            slotModeChanged(pad);
        });
        for (int param = 0; param < 5; ++param) {
            connect(m_pParams[pad][param].get(),
                    &ControlObject::valueChanged,
                    this,
                    [this, pad, param](double) {
                        slotParamChanged(pad, param);
                    });
        }
    }
    m_pStemCount->connectValueChanged(this, &PadFxUnit::slotStemCountChanged);
    slotStemCountChanged(m_pStemCount->get());
}

QString PadFxUnit::stemGroup(int stemIndex) const {
    return m_deckGroup.chopped(1) + QStringLiteral("_Stem") +
            QChar('1' + stemIndex) + QChar(']');
}

EffectSlot* PadFxUnit::slotFor(int stemIndex, int pad) const {
    if (stemIndex < 0 || stemIndex >= mixxx::padfx::kNumStems) {
        return nullptr;
    }
    const EffectChainPointer pChain =
            m_pEffectsManager->getPadEffectChain(stemGroup(stemIndex));
    if (!pChain) {
        return nullptr;
    }
    return pChain->getEffectSlot(pad).data();
}

int PadFxUnit::stemCount() const {
    return clampInt(static_cast<int>(std::lround(m_pStemCount->get())), 0, mixxx::padfx::kNumStems);
}

double PadFxUnit::bpm() const {
    const double value = m_pBpm->get();
    return value > 0.0 ? value : 120.0;
}

void PadFxUnit::loadConfig() {
    const QString json = m_pConfig->getValue(
            ConfigKey(kConfigGroup, configKeyForDeck(m_deckGroup)), QString());
    if (json.isEmpty()) {
        // Bank A: brake on instrumental/vocals, echo out on instrumental/vocals.
        auto& pads = m_config.banks[0].pads;
        pads[0].effect = static_cast<int>(PadEffect::VinylBrake);
        pads[0].targetMask = 7;
        pads[0].mode = static_cast<int>(PadMode::Toggle);
        pads[1].effect = static_cast<int>(PadEffect::VinylBrake);
        pads[1].targetMask = 8;
        pads[1].mode = static_cast<int>(PadMode::Toggle);
        pads[2].effect = static_cast<int>(PadEffect::EchoOut);
        pads[2].targetMask = 7;
        pads[2].mode = static_cast<int>(PadMode::OneShot);
        pads[3].effect = static_cast<int>(PadEffect::EchoOut);
        pads[3].targetMask = 8;
        pads[3].mode = static_cast<int>(PadMode::OneShot);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject root = doc.object();
    m_config.bank = clampInt(root.value("bank").toInt(1), 1, mixxx::padfx::kNumBanks);
    const QJsonArray banks = root.value("banks").toArray();
    for (int bank = 0; bank < banks.size() && bank < mixxx::padfx::kNumBanks; ++bank) {
        const QJsonArray pads = banks.at(bank).toObject().value("pads").toArray();
        for (int pad = 0; pad < pads.size() && pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
            const QJsonObject obj = pads.at(pad).toObject();
            auto& cfg = m_config.banks[bank].pads[pad];
            cfg.effect = clampInt(obj.value("effect").toInt(0), 0, 2);
            cfg.targetMask = clampInt(obj.value("target").toInt(1), 1, 15);
            cfg.mode = clampInt(obj.value("mode").toInt(1), 0, 2);
            const QJsonArray params = obj.value("params").toArray();
            for (int param = 0; param < params.size() && param < 5; ++param) {
                cfg.params[param] = clamp01(params.at(param).toDouble(0.5));
            }
        }
    }
}

void PadFxUnit::saveConfig() {
    QJsonObject root;
    root.insert("bank", m_config.bank);
    QJsonArray banks;
    for (const auto& bank : m_config.banks) {
        QJsonObject bankObject;
        QJsonArray pads;
        for (const auto& pad : bank.pads) {
            QJsonObject padObject;
            padObject.insert("effect", pad.effect);
            padObject.insert("target", pad.targetMask);
            padObject.insert("mode", pad.mode);
            QJsonArray params;
            for (double param : pad.params) {
                params.append(param);
            }
            padObject.insert("params", params);
            pads.append(padObject);
        }
        bankObject.insert("pads", pads);
        banks.append(bankObject);
    }
    root.insert("banks", banks);
    m_pConfig->setValue(ConfigKey(kConfigGroup, configKeyForDeck(m_deckGroup)),
            QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

void PadFxUnit::syncControlsFromConfig() {
    m_syncingControls = true;
    m_pBank->set(m_config.bank);
    const auto& bank = m_config.banks.at(m_config.bank - 1);
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        const auto& cfg = bank.pads[pad];
        m_pEffects[pad]->set(cfg.effect);
        m_pTargets[pad]->set(cfg.targetMask);
        m_pModes[pad]->set(cfg.mode);
        for (int param = 0; param < 5; ++param) {
            m_pParams[pad][param]->set(cfg.params[param]);
        }
    }
    m_syncingControls = false;
}

void PadFxUnit::unloadPad(int pad) {
    for (int stem = 0; stem < mixxx::padfx::kNumStems; ++stem) {
        EffectSlot* pSlot = slotFor(stem, pad);
        if (pSlot) {
            pSlot->setEnabled(false);
            pSlot->loadEffectWithDefaults(EffectManifestPointer());
        }
        m_loadedEffectIds[pad][stem].clear();
    }
}

void PadFxUnit::unloadAll() {
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        unloadPad(pad);
    }
}

void PadFxUnit::applyPadParameters(int pad) {
    const auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    for (int stem = 0; stem < stemCount(); ++stem) {
        if ((cfg.targetMask & (1 << stem)) == 0) {
            continue;
        }
        EffectSlot* pSlot = slotFor(stem, pad);
        if (!pSlot) {
            continue;
        }
        const auto loaded = pSlot->getLoadedParameters();
        for (const auto& list : loaded) {
            for (const auto& pParameter : list) {
                if (!pParameter) {
                    continue;
                }
                const QString id = pParameter->manifest()->id();
                if (id == QStringLiteral("brake_time")) {
                    pParameter->setValue(0.1 + clamp01(cfg.params[0]) * 1.9);
                } else if (id == QStringLiteral("curve")) {
                    pParameter->setValue(clamp01(cfg.params[1]));
                } else if (id == QStringLiteral("wind_up_time")) {
                    pParameter->setValue(0.05 + clamp01(cfg.params[2]) * 0.95);
                } else if (id == QStringLiteral("beats")) {
                    pParameter->setValue(0.125 + clamp01(cfg.params[0]) * 1.875);
                } else if (id == QStringLiteral("feedback")) {
                    pParameter->setValue(clamp01(cfg.params[1]) * 0.95);
                } else if (id == QStringLiteral("reverb")) {
                    pParameter->setValue(clamp01(cfg.params[3]) * 0.85);
                } else if (id == QStringLiteral("reverb_decay")) {
                    pParameter->setValue(0.15 + clamp01(cfg.params[4]) * 0.85);
                }
            }
        }
    }
}

void PadFxUnit::loadPad(int pad) {
    const auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    const auto effect = static_cast<PadEffect>(cfg.effect);
    const QString id = mixxx::padfx::effectId(effect);
    const int count = stemCount();

    for (int stem = 0; stem < mixxx::padfx::kNumStems; ++stem) {
        EffectSlot* pSlot = slotFor(stem, pad);
        if (!pSlot) {
            continue;
        }
        const bool wanted = !id.isEmpty() &&
                stem < count &&
                (cfg.targetMask & (1 << stem)) != 0;
        if (!wanted) {
            pSlot->setEnabled(false);
            if (!m_loadedEffectIds[pad][stem].isEmpty()) {
                pSlot->loadEffectWithDefaults(EffectManifestPointer());
                m_loadedEffectIds[pad][stem].clear();
            }
            continue;
        }

        if (m_loadedEffectIds[pad][stem] != id) {
            const EffectManifestPointer pManifest =
                    m_pEffectsManager->getBackendManager()->getManifest(
                            id, EffectBackendType::BuiltIn);
            if (!pManifest) {
                continue;
            }
            pSlot->loadEffectWithDefaults(pManifest);
            m_loadedEffectIds[pad][stem] = id;
        }
        pSlot->setEnabled(true);
        // ControlObject::set from the main thread is the same path used by
        // QML and mappings. It also guarantees the slot's valueChanged signal
        // is delivered when the slot was created immediately before this call.
        ControlObject::set(ConfigKey(pSlot->getGroup(), QStringLiteral("enabled")), 1.0);
    }
    applyPadParameters(pad);
    setEffectActive(pad, false);
}

void PadFxUnit::applyBank() {
    unloadAll();
    if (stemCount() <= 0) {
        m_appliedStemCount = stemCount();
        return;
    }
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        loadPad(pad);
    }
    m_appliedStemCount = stemCount();
}

void PadFxUnit::slotBankChanged(double value) {
    if (m_syncingControls) {
        return;
    }
    const int newBank = clampInt(static_cast<int>(std::lround(value)), 1, mixxx::padfx::kNumBanks);
    if (newBank == m_config.bank) {
        return;
    }
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        if (m_runtime[pad].muteOwned) {
            restoreMuteOwned(pad);
        }
        m_padTimers[pad]->stop();
        m_runtime[pad] = PadRuntime{};
        setPadActiveControl(pad, false);
    }
    m_config.bank = newBank;
    syncControlsFromConfig();
    applyBank();
    saveConfig();
}

void PadFxUnit::slotPadChanged(int pad) {
    const bool down = m_pPads[pad]->toBool();
    if (down == m_runtime[pad].down) {
        return;
    }
    m_runtime[pad].down = down;

    const auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    const auto mode = static_cast<PadMode>(cfg.mode);
    if (down) {
        if (stemCount() <= 0 || cfg.effect == static_cast<int>(PadEffect::Off)) {
            return;
        }
        if (m_runtime[pad].muteOwned || m_runtime[pad].gestureActive) {
            // A second click always toggles the pad off, even if the brake or
            // echo gesture is still running. Previously those clicks were
            // ignored until the gesture timer expired.
            stopPad(pad);
            return;
        }

        for (int stem = 0; stem < mixxx::padfx::kNumStems; ++stem) {
            m_runtime[pad].savedMute[stem] = m_pStemMute[stem]->get() > 0.5 ? 1 : 0;
        }
        setEffectActive(pad, true);
        m_runtime[pad].gestureActive = true;
        setPadActiveControl(pad, true);

        if (mode != PadMode::Momentary) {
            const int durationMs = std::max(20,
                    static_cast<int>(padDurationSeconds(pad) * 1000.0));
            m_padTimers[pad]->start(durationMs);
        }
    } else {
        if (mode == PadMode::Momentary && m_runtime[pad].gestureActive) {
            m_padTimers[pad]->stop();
            setEffectActive(pad, false);
            m_runtime[pad].gestureActive = false;
            if (m_runtime[pad].muteOwned) {
                restoreMuteOwned(pad);
            }
            setPadActiveControl(pad, false);
        }
    }
}

void PadFxUnit::finishPad(int pad) {
    if (!m_runtime[pad].gestureActive) {
        return;
    }
    // The audible gesture is over. Hold the stems with the native mute before
    // dropping the effect gate, otherwise the dry signal can burst through for
    // one callback while the two control changes cross the engine boundary.
    const auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    for (int stem = 0; stem < mixxx::padfx::kNumStems; ++stem) {
        if ((cfg.targetMask & (1 << stem)) != 0) {
            m_pStemMute[stem]->set(1.0);
        }
    }
    // Keep the brake gate engaged while the pad is latched. The wind-up is
    // triggered when the pad is released, after the native mute is cleared, so
    // the spin-up is audible. Echo Out releases immediately.
    if (cfg.effect != static_cast<int>(PadEffect::VinylBrake)) {
        setEffectActive(pad, false);
    }
    m_runtime[pad].gestureActive = false;
    m_runtime[pad].muteOwned = true;
    m_pPadMuted[pad]->forceSet(1.0);

    const auto mode = static_cast<PadMode>(
            m_config.banks.at(m_config.bank - 1).pads[pad].mode);
    setPadActiveControl(pad, mode == PadMode::Toggle);
}

void PadFxUnit::stopPad(int pad) {
    m_padTimers[pad]->stop();
    if (m_runtime[pad].muteOwned) {
        restoreMuteOwned(pad);
    } else {
        setEffectActive(pad, false);
        m_runtime[pad].gestureActive = false;
        setPadActiveControl(pad, false);
        m_pPadMuted[pad]->forceSet(0.0);
    }
    m_runtime[pad].down = false;
}

void PadFxUnit::restoreMuteOwned(int pad) {
    for (int stem = 0; stem < mixxx::padfx::kNumStems; ++stem) {
        if ((m_config.banks.at(m_config.bank - 1).pads[pad].targetMask & (1 << stem)) == 0) {
            continue;
        }
        const double current = m_pStemMute[stem]->get();
        if (current > 0.5) {
            m_pStemMute[stem]->set(m_runtime[pad].savedMute[stem] ? 1.0 : 0.0);
        }
    }
    m_runtime[pad].muteOwned = false;
    m_runtime[pad].gestureActive = false;
    setEffectActive(pad, false);
    m_pPadMuted[pad]->forceSet(0.0);
    setPadActiveControl(pad, false);
}

void PadFxUnit::slotEffectChanged(int pad) {
    if (m_syncingControls) {
        return;
    }
    auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    const int newEffect = clampInt(static_cast<int>(std::lround(m_pEffects[pad]->get())), 0, 2);
    if (cfg.effect == newEffect) {
        return;
    }
    if (m_runtime[pad].muteOwned) {
        restoreMuteOwned(pad);
    }
    m_padTimers[pad]->stop();
    m_runtime[pad] = PadRuntime{};
    cfg.effect = newEffect;
    unloadPad(pad);
    applyPadParameters(pad);
    loadPad(pad);
    saveConfig();
}

void PadFxUnit::slotTargetChanged(int pad) {
    if (m_syncingControls) {
        return;
    }
    auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    const int newTarget = clampInt(static_cast<int>(std::lround(m_pTargets[pad]->get())), 1, 15);
    if (cfg.targetMask == newTarget) {
        return;
    }
    if (m_runtime[pad].muteOwned) {
        restoreMuteOwned(pad);
    }
    m_padTimers[pad]->stop();
    m_runtime[pad] = PadRuntime{};
    cfg.targetMask = newTarget;
    unloadPad(pad);
    loadPad(pad);
    saveConfig();
}

void PadFxUnit::slotModeChanged(int pad) {
    if (m_syncingControls) {
        return;
    }
    auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    cfg.mode = clampInt(static_cast<int>(std::lround(m_pModes[pad]->get())), 0, 2);
    saveConfig();
}

void PadFxUnit::slotParamChanged(int pad, int param) {
    if (m_syncingControls) {
        return;
    }
    auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    cfg.params[param] = clamp01(m_pParams[pad][param]->get());
    applyPadParameters(pad);
    saveConfig();
}

void PadFxUnit::slotStemCountChanged(double value) {
    const int count = clampInt(static_cast<int>(std::lround(value)), 0, mixxx::padfx::kNumStems);
    if (count == m_appliedStemCount) {
        return;
    }
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        if (m_runtime[pad].muteOwned) {
            restoreMuteOwned(pad);
        }
        m_padTimers[pad]->stop();
        m_runtime[pad] = PadRuntime{};
        setPadActiveControl(pad, false);
    }
    if (count <= 0) {
        unloadAll();
        m_appliedStemCount = 0;
        return;
    }
    applyBank();
}

void PadFxUnit::resetForTrackLoad() {
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        m_padTimers[pad]->stop();
        if (m_runtime[pad].muteOwned) {
            restoreMuteOwned(pad);
        }
        m_runtime[pad] = PadRuntime{};
        setPadActiveControl(pad, false);
    }
    unloadAll();
    applyBank();
}

void PadFxUnit::setPadActiveControl(int pad, bool active) {
    m_pPadActive[pad]->forceSet(active ? 1.0 : 0.0);
}

void PadFxUnit::setEffectActive(int pad, bool active) {
    const auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    for (int stem = 0; stem < stemCount(); ++stem) {
        if ((cfg.targetMask & (1 << stem)) == 0) {
            continue;
        }
        EffectSlot* pSlot = slotFor(stem, pad);
        if (!pSlot || !pSlot->isLoaded()) {
            continue;
        }
        bool found = false;
        const auto loaded = pSlot->getLoadedParameters();
        for (const auto& list : loaded) {
            for (const auto& pParameter : list) {
                if (pParameter && pParameter->manifest()->id() == QStringLiteral("active")) {
                    pParameter->setValue(active ? 1.0 : 0.0);
                    found = true;
                }
            }
        }
        if (!found) {
            pSlot->setEnabled(active);
        }
    }
}

double PadFxUnit::padDurationSeconds(int pad) const {
    const auto& cfg = m_config.banks.at(m_config.bank - 1).pads[pad];
    const auto effect = static_cast<PadEffect>(cfg.effect);
    if (effect == PadEffect::VinylBrake) {
        return 0.1 + clamp01(cfg.params[0]) * 1.9 + 0.05;
    }
    if (effect == PadEffect::EchoOut) {
        const double beats = 0.5 + clamp01(cfg.params[2]) * 7.5;
        return std::clamp(beats * 60.0 / bpm(), 0.05, 15.0);
    }
    return 0.0;
}
