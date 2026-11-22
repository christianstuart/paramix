#include "effects/padfx/padfxunit.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>

#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "control/controlproxy.h"
#include "control/controlpushbutton.h"
#include "effects/backends/effectmanifest.h"
#include "effects/backends/effectmanifestparameter.h"
#include "effects/backends/effectsbackendmanager.h"
#include "effects/effectchain.h"
#include "effects/effectparameter.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "effects/padfx/padfxpreset.h"
#include "moc_padfxunit.cpp"
#include "util/assert.h"

namespace {
using mixxx::padfx::kDeckTarget;

constexpr const char* kConfigGroup = "[PadFX]";

QString configKeyForDeck(const QString& deckGroup, const char* suffix) {
    QString digits;
    for (const QChar c : deckGroup) {
        if (c.isDigit()) {
            digits.append(c);
        }
    }
    return QStringLiteral("Deck%1%2")
            .arg(digits.isEmpty() ? deckGroup : digits)
            .arg(QString::fromLatin1(suffix));
}

int clampInt(int value, int low, int high) {
    return std::clamp(value, low, high);
}

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

EffectParameterPointer findParameter(const EffectSlot* pSlot, const QString& id) {
    if (!pSlot) {
        return {};
    }
    const auto loaded = pSlot->getLoadedParameters();
    for (const auto& list : loaded) {
        for (const auto& pParameter : list) {
            if (pParameter && pParameter->manifest()->id() == id) {
                return pParameter;
            }
        }
    }
    const auto hidden = pSlot->getHiddenParameters();
    for (const auto& list : hidden) {
        for (const auto& pParameter : list) {
            if (pParameter && pParameter->manifest()->id() == id) {
                return pParameter;
            }
        }
    }
    return {};
}

void applyGenericParameters(const EffectSlot* pSlot,
        const std::array<double, mixxx::padfx::kMaxPadParams>& params) {
    const auto pManifest = pSlot->getManifest();
    if (!pManifest) {
        return;
    }
    int index = 0;
    for (const auto& pManifestParameter : pManifest->parameters()) {
        if (index >= mixxx::padfx::kMaxPadParams) {
            break;
        }
        if (pManifestParameter->parameterType() !=
                        EffectManifestParameter::ParameterType::Knob ||
                pManifestParameter->id() == QStringLiteral("active")) {
            continue;
        }
        const EffectParameterPointer pParameter =
                findParameter(pSlot, pManifestParameter->id());
        if (pParameter) {
            const double minimum = pManifestParameter->getMinimum();
            const double maximum = pManifestParameter->getMaximum();
            pParameter->setValue(minimum + clamp01(params[index]) * (maximum - minimum));
        }
        ++index;
    }
}

void applyPadOnlyParameters(const EffectSlot* pSlot,
        const QString& effectId,
        const std::array<double, mixxx::padfx::kMaxPadParams>& params) {
    for (const auto& list : {pSlot->getLoadedParameters(),
                 pSlot->getHiddenParameters()}) {
        for (const auto& parameters : list) {
            for (const auto& pParameter : parameters) {
                if (!pParameter) {
                    continue;
                }
                const QString id = pParameter->manifest()->id();
                if (mixxx::padfx::baseEffectId(effectId) ==
                        mixxx::padfx::kVinylBrakeId) {
                    if (id == QStringLiteral("brake_time")) {
                        pParameter->setValue(0.1 + clamp01(params[0]) * 1.9);
                    } else if (id == QStringLiteral("curve")) {
                        pParameter->setValue(clamp01(params[1]));
                    } else if (id == QStringLiteral("wind_up_time")) {
                        pParameter->setValue(0.05 + clamp01(params[2]) * 0.95);
                    }
                } else if (mixxx::padfx::baseEffectId(effectId) ==
                        mixxx::padfx::kEchoOutId) {
                    if (id == QStringLiteral("beats")) {
                        pParameter->setValue(0.125 + clamp01(params[0]) * 1.875);
                    } else if (id == QStringLiteral("feedback")) {
                        pParameter->setValue(clamp01(params[1]) * 0.95);
                    } else if (id == QStringLiteral("reverb")) {
                        pParameter->setValue(clamp01(params[3]) * 0.85);
                    } else if (id == QStringLiteral("reverb_decay")) {
                        pParameter->setValue(0.15 + clamp01(params[4]) * 0.85);
                    }
                }
            }
        }
    }
}
} // namespace

PadFxUnit::~PadFxUnit() = default;

PadFxUnit::PadFxUnit(const QString& deckGroup,
        UserSettingsPointer pConfig,
        EffectsManager* pEffectsManager,
        PadFxPreset* pPreset,
        QObject* parent)
        : QObject(parent),
          m_deckGroup(deckGroup),
          m_controlGroup(deckGroup.chopped(1) + QStringLiteral("_PadFX]")),
          m_pConfig(pConfig),
          m_pEffectsManager(pEffectsManager),
          m_pPreset(pPreset) {
    DEBUG_ASSERT(m_pEffectsManager != nullptr);
    DEBUG_ASSERT(m_pPreset != nullptr);

    m_pBank = std::make_unique<ControlPotmeter>(
            ConfigKey(m_controlGroup, QStringLiteral("bank")), 1.0, 2.0, false, true, false, false, 1.0);
    m_pBank->setStepCount(1);
    m_pBank->set(1.0);

    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        const QString padPrefix = QStringLiteral("pad%1").arg(pad + 1);
        m_pPads[pad] = std::make_unique<ControlPushButton>(
                ConfigKey(m_controlGroup, padPrefix));
        m_pPads[pad]->setButtonMode(mixxx::control::ButtonMode::Push);

        m_pPadActive[pad] = std::make_unique<ControlObject>(
                ConfigKey(m_controlGroup, padPrefix + QStringLiteral("_active")));
        m_pPadActive[pad]->setReadOnly();
    }

    m_pStemCount = std::make_unique<ControlProxy>(
            ConfigKey(m_deckGroup, QStringLiteral("stem_count")), this);

    loadBankConfig();
    m_syncingControls = true;
    m_pBank->set(m_bank);
    m_syncingControls = false;

    connect(m_pBank.get(), &ControlObject::valueChanged, this, [this](double value) {
        slotBankChanged(value);
    });
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        connect(m_pPads[pad].get(), &ControlObject::valueChanged, this, [this, pad](double) {
            slotPadChanged(pad);
        });
    }
    connect(m_pPreset, &PadFxPreset::padChanged, this, &PadFxUnit::slotPresetPadChanged);
    connect(m_pPreset, &PadFxPreset::allPadsChanged, this, &PadFxUnit::slotPresetAllChanged);
    m_pStemCount->connectValueChanged(this, &PadFxUnit::slotStemCountChanged);
    slotStemCountChanged(m_pStemCount->get());
}

QString PadFxUnit::stemGroup(int stemIndex) const {
    return m_deckGroup.chopped(1) + QStringLiteral("_Stem") +
            QChar('1' + stemIndex) + QChar(']');
}

int PadFxUnit::bankIndex() const {
    return clampInt(m_bank - 1, 0, mixxx::padfx::kNumBanks - 1);
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

EffectSlot* PadFxUnit::deckSlotFor(int pad) const {
    const EffectChainPointer pChain =
            m_pEffectsManager->getPadEffectChain(m_deckGroup);
    if (!pChain) {
        return nullptr;
    }
    return pChain->getEffectSlot(pad).data();
}

EffectSlot* PadFxUnit::slotForTarget(int target, int pad) const {
    return target == kDeckTarget ? deckSlotFor(pad) : slotFor(target, pad);
}

EffectManifestPointer PadFxUnit::manifestForId(const QString& effectId) const {
    if (effectId.isEmpty()) {
        return {};
    }
    // Resolve against the same catalogue the preset uses. This avoids parsing
    // a unique ID when the effect comes from a non-built-in backend.
    for (const auto& pManifest : m_pPreset->effectManifests()) {
        if (pManifest->uniqueId() == effectId ||
                pManifest->id() == effectId) {
            return pManifest;
        }
    }
    return m_pEffectsManager->getBackendManager()->getManifestFromUniqueId(effectId);
}

bool PadFxUnit::targetAvailable(int targetMask) const {
    return targetMask == kDeckTarget || stemCount() > 0;
}

int PadFxUnit::stemCount() const {
    return clampInt(static_cast<int>(std::lround(m_pStemCount->get())), 0, mixxx::padfx::kNumStems);
}

void PadFxUnit::loadBankConfig() {
    const QString key = configKeyForDeck(m_deckGroup, "Bank");
    const QString stored = m_pConfig->getValue(
            ConfigKey(kConfigGroup, key), QString());
    if (!stored.isEmpty()) {
        m_bank = clampInt(stored.toInt(), 1, 2);
        return;
    }

    // Old per-deck JSON stored an A-D bank. A/B become FX 1, C/D become FX 2.
    const QString legacyKey = configKeyForDeck(m_deckGroup, "");
    const QString json = m_pConfig->getValue(ConfigKey(kConfigGroup, legacyKey), QString());
    if (json.isEmpty()) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        return;
    }
    const int oldBank = clampInt(doc.object().value("bank").toInt(1), 1, 4);
    m_bank = oldBank <= 2 ? 1 : 2;
}

void PadFxUnit::saveBankConfig() {
    m_pConfig->setValue(
            ConfigKey(kConfigGroup, configKeyForDeck(m_deckGroup, "Bank")),
            QString::number(m_bank));
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
    EffectSlot* pDeckSlot = deckSlotFor(pad);
    if (pDeckSlot) {
        pDeckSlot->setEnabled(false);
        pDeckSlot->loadEffectWithDefaults(EffectManifestPointer());
    }
    m_loadedDeckEffectIds[pad].clear();
}

void PadFxUnit::unloadAll() {
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        unloadPad(pad);
    }
}

void PadFxUnit::applyPadParameters(int pad) {
    const auto& cfg = m_pPreset->padConfig(bankIndex(), pad);
    if (cfg.effectId.isEmpty()) {
        return;
    }
    if (cfg.targetMask == kDeckTarget) {
        EffectSlot* pSlot = deckSlotFor(pad);
        if (!pSlot) {
            return;
        }
        if (mixxx::padfx::isPadFxOnly(cfg.effectId)) {
            applyPadOnlyParameters(pSlot, cfg.effectId, cfg.params);
        } else {
            applyGenericParameters(pSlot, cfg.params);
        }
        return;
    }
    for (int stem = 0; stem < stemCount(); ++stem) {
        if ((cfg.targetMask & (1 << stem)) == 0) {
            continue;
        }
        EffectSlot* pSlot = slotFor(stem, pad);
        if (!pSlot) {
            continue;
        }
        if (mixxx::padfx::isPadFxOnly(cfg.effectId)) {
            applyPadOnlyParameters(pSlot, cfg.effectId, cfg.params);
        } else {
            applyGenericParameters(pSlot, cfg.params);
        }
    }
}

void PadFxUnit::loadPad(int pad) {
    const auto& cfg = m_pPreset->padConfig(bankIndex(), pad);
    const QString id = cfg.effectId;
    if (id.isEmpty()) {
        unloadPad(pad);
        return;
    }
    const int count = stemCount();
    const bool deckTarget = cfg.targetMask == kDeckTarget;

    for (int stem = 0; stem < mixxx::padfx::kNumStems; ++stem) {
        EffectSlot* pSlot = slotFor(stem, pad);
        if (!pSlot) {
            continue;
        }
        const bool wanted = !deckTarget && stem < count &&
                (cfg.targetMask & (1 << stem)) != 0;
        if (!wanted) {
            if (!m_loadedEffectIds[pad][stem].isEmpty()) {
                pSlot->setEnabled(false);
                pSlot->loadEffectWithDefaults(EffectManifestPointer());
                m_loadedEffectIds[pad][stem].clear();
            }
            continue;
        }
        if (m_loadedEffectIds[pad][stem] != id) {
            const EffectManifestPointer pManifest = manifestForId(id);
            if (!pManifest) {
                continue;
            }
            pSlot->loadEffectWithDefaults(pManifest);
            m_loadedEffectIds[pad][stem] = id;
        }
        pSlot->setEnabled(true);
        ControlObject::set(ConfigKey(pSlot->getGroup(), QStringLiteral("enabled")), 1.0);
    }

    EffectSlot* pDeckSlot = deckSlotFor(pad);
    if (pDeckSlot) {
        if (!deckTarget) {
            if (!m_loadedDeckEffectIds[pad].isEmpty()) {
                pDeckSlot->setEnabled(false);
                pDeckSlot->loadEffectWithDefaults(EffectManifestPointer());
                m_loadedDeckEffectIds[pad].clear();
            }
        } else {
            if (m_loadedDeckEffectIds[pad] != id) {
                const EffectManifestPointer pManifest = manifestForId(id);
                if (!pManifest) {
                    return;
                }
                pDeckSlot->loadEffectWithDefaults(pManifest);
                m_loadedDeckEffectIds[pad] = id;
            }
            pDeckSlot->setEnabled(true);
            ControlObject::set(ConfigKey(pDeckSlot->getGroup(), QStringLiteral("enabled")), 1.0);
        }
    }

    applyPadParameters(pad);
    setEffectActive(pad, false);
}

void PadFxUnit::loadBank() {
    unloadAll();
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
    if (newBank == m_bank) {
        return;
    }
    stopAllPads();
    m_bank = newBank;
    saveBankConfig();
    loadBank();
}

void PadFxUnit::slotPadChanged(int pad) {
    const bool down = m_pPads[pad]->toBool();
    if (down == m_runtime[pad].down) {
        return;
    }
    m_runtime[pad].down = down;

    const auto& cfg = m_pPreset->padConfig(bankIndex(), pad);
    if (cfg.effectId.isEmpty() || !targetAvailable(cfg.targetMask)) {
        return;
    }

    if (down) {
        setEffectActive(pad, true);
        m_runtime[pad].active = true;
        setPadActiveControl(pad, true);
    } else if (m_runtime[pad].active) {
        setEffectActive(pad, false);
        m_runtime[pad].active = false;
        setPadActiveControl(pad, false);
    }
}

void PadFxUnit::stopAllPads() {
    for (int pad = 0; pad < mixxx::padfx::kNumPadsPerBank; ++pad) {
        if (m_runtime[pad].active) {
            setEffectActive(pad, false);
        }
        m_runtime[pad] = PadRuntime{};
        setPadActiveControl(pad, false);
    }
}

void PadFxUnit::slotStemCountChanged(double value) {
    const int count = clampInt(static_cast<int>(std::lround(value)), 0, mixxx::padfx::kNumStems);
    if (count == m_appliedStemCount) {
        return;
    }
    stopAllPads();
    loadBank();
}

void PadFxUnit::slotPresetPadChanged(int bankIndex, int pad) {
    if (bankIndex != this->bankIndex()) {
        return;
    }
    if (m_runtime[pad].active) {
        setEffectActive(pad, false);
    }
    m_runtime[pad] = PadRuntime{};
    setPadActiveControl(pad, false);
    unloadPad(pad);
    loadPad(pad);
}

void PadFxUnit::slotPresetAllChanged() {
    stopAllPads();
    loadBank();
}

void PadFxUnit::resetForTrackLoad() {
    stopAllPads();
    unloadAll();
    loadBank();
}

void PadFxUnit::setPadActiveControl(int pad, bool active) {
    m_pPadActive[pad]->forceSet(active ? 1.0 : 0.0);
}

void PadFxUnit::setEffectActive(int pad, bool active) {
    const auto& cfg = m_pPreset->padConfig(bankIndex(), pad);
    auto setSlotActive = [active](EffectSlot* pSlot) {
        if (!pSlot || !pSlot->isLoaded()) {
            return;
        }
        bool found = false;
        for (const auto& list : {pSlot->getLoadedParameters(),
                     pSlot->getHiddenParameters()}) {
            for (const auto& parameters : list) {
                for (const auto& pParameter : parameters) {
                    if (pParameter &&
                            pParameter->manifest()->id() == QStringLiteral("active")) {
                        pParameter->setValue(active ? 1.0 : 0.0);
                        found = true;
                    }
                }
            }
        }
        if (!found) {
            pSlot->setEnabled(active);
        }
    };

    if (cfg.targetMask == kDeckTarget) {
        setSlotActive(deckSlotFor(pad));
        return;
    }
    for (int stem = 0; stem < mixxx::padfx::kNumStems; ++stem) {
        if ((cfg.targetMask & (1 << stem)) != 0) {
            setSlotActive(slotFor(stem, pad));
        }
    }
}
