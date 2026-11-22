#include "effects/padfx/padfxpreset.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>

#include "control/controlpotmeter.h"
#include "effects/backends/effectmanifest.h"
#include "effects/backends/effectmanifestparameter.h"
#include "effects/effectsmanager.h"
#include "effects/visibleeffectslist.h"
#include "moc_padfxpreset.cpp"
#include "util/assert.h"

namespace {
using mixxx::padfx::kDeckTarget;
using mixxx::padfx::kMaxPadParams;
using mixxx::padfx::kNumBanks;
using mixxx::padfx::kNumPadsPerBank;

constexpr const char* kConfigGroup = "[PadFX]";
constexpr const char* kPresetKey = "Preset";
constexpr const char* kLegacyDeck1Key = "Deck1";

int clampInt(int value, int low, int high) {
    return std::clamp(value, low, high);
}

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}
} // namespace

PadFxPreset::PadFxPreset(UserSettingsPointer pConfig,
        EffectsManager* pEffectsManager,
        QObject* parent)
        : QObject(parent),
          m_pConfig(std::move(pConfig)),
          m_pEffectsManager(pEffectsManager) {
    DEBUG_ASSERT(m_pEffectsManager != nullptr);

    updateCatalog();
    loadConfig();
    createControls();
    syncControls();

    connect(m_pEffectsManager->getVisibleEffectsList().get(),
            &VisibleEffectsList::visibleEffectsListChanged,
            this,
            &PadFxPreset::slotVisibleEffectsChanged);
}

PadFxPreset::~PadFxPreset() = default;

const mixxx::padfx::PadConfig& PadFxPreset::padConfig(int bankIndex, int pad) const {
    static const mixxx::padfx::PadConfig kEmpty;
    if (bankIndex < 0 || bankIndex >= kNumBanks ||
            pad < 0 || pad >= kNumPadsPerBank) {
        return kEmpty;
    }
    return m_config.banks[bankIndex].pads[pad];
}

QString PadFxPreset::effectKey(int bankIndex, int pad) {
    return QStringLiteral("fx%1_effect%2").arg(bankIndex + 1).arg(pad + 1);
}

QString PadFxPreset::targetKey(int bankIndex, int pad) {
    return QStringLiteral("fx%1_target%2").arg(bankIndex + 1).arg(pad + 1);
}

QString PadFxPreset::paramKey(int bankIndex, int pad, int param) {
    return QStringLiteral("fx%1_pad%2_param%3")
            .arg(bankIndex + 1)
            .arg(pad + 1)
            .arg(param + 1);
}

void PadFxPreset::createControls() {
    for (int bank = 0; bank < kNumBanks; ++bank) {
        auto& controls = m_controls[bank];
        for (int pad = 0; pad < kNumPadsPerBank; ++pad) {
            controls.effects[pad] = std::make_unique<ControlPotmeter>(
                    ConfigKey(kConfigGroup, effectKey(bank, pad)),
                    0.0,
                    255.0,
                    false,
                    true,
                    false,
                    false,
                    0.0);
            controls.effects[pad]->setStepCount(
                    std::max(0, static_cast<int>(m_effects.size()) - 1));
            controls.targets[pad] = std::make_unique<ControlPotmeter>(
                    ConfigKey(kConfigGroup, targetKey(bank, pad)),
                    0.0,
                    15.0,
                    false,
                    true,
                    false,
                    false,
                    kDeckTarget);
            controls.targets[pad]->setStepCount(15);
            for (int param = 0; param < kMaxPadParams; ++param) {
                controls.params[pad][param] = std::make_unique<ControlPotmeter>(
                        ConfigKey(kConfigGroup, paramKey(bank, pad, param)),
                        0.0,
                        1.0,
                        false,
                        true,
                        false,
                        false,
                        0.5);
            }

            connect(controls.effects[pad].get(),
                    &ControlObject::valueChanged,
                    this,
                    [this, bank, pad](double) {
                        slotEffectChanged(bank, pad);
                    });
            connect(controls.targets[pad].get(),
                    &ControlObject::valueChanged,
                    this,
                    [this, bank, pad](double) {
                        slotTargetChanged(bank, pad);
                    });
            for (int param = 0; param < kMaxPadParams; ++param) {
                connect(controls.params[pad][param].get(),
                        &ControlObject::valueChanged,
                        this,
                        [this, bank, pad, param](double) {
                            slotParamChanged(bank, pad, param);
                        });
            }
        }
    }
}

void PadFxPreset::setDefaults() {
    m_config = mixxx::padfx::PresetConfig{};
    for (auto& bank : m_config.banks) {
        for (auto& pad : bank.pads) {
            pad.effectId.clear();
            pad.targetMask = kDeckTarget;
            pad.params = mixxx::padfx::defaultPadParams(QString());
        }
    }

    auto& fx1 = m_config.banks[0].pads;
    fx1[0].effectId = mixxx::padfx::kVinylBrakeId;
    fx1[0].targetMask = 7;
    fx1[0].params = mixxx::padfx::defaultPadParams(fx1[0].effectId);
    fx1[1].effectId = mixxx::padfx::kVinylBrakeId;
    fx1[1].targetMask = 8;
    fx1[1].params = mixxx::padfx::defaultPadParams(fx1[1].effectId);
    fx1[2].effectId = mixxx::padfx::kEchoOutId;
    fx1[2].targetMask = 7;
    fx1[2].params = mixxx::padfx::defaultPadParams(fx1[2].effectId);
    fx1[3].effectId = mixxx::padfx::kEchoOutId;
    fx1[3].targetMask = 8;
    fx1[3].params = mixxx::padfx::defaultPadParams(fx1[3].effectId);
}

void PadFxPreset::migrateLegacy(const QJsonObject& legacy, bool useLegacyBank) {
    Q_UNUSED(useLegacyBank);
    setDefaults();
    const QJsonArray banks = legacy.value("banks").toArray();
    const bool legacyLayout = banks.size() == 4 &&
            banks.at(0).toObject().value("pads").toArray().size() == 4;
    if (!legacyLayout) {
        return;
    }

    // Four old banks of four pads become two banks of eight:
    //   A + B -> FX 1, C + D -> FX 2.
    for (int newBank = 0; newBank < kNumBanks; ++newBank) {
        for (int oldSlot = 0; oldSlot < 2; ++oldSlot) {
            const QJsonArray oldPads = banks.at(newBank * 2 + oldSlot)
                                               .toObject()
                                               .value("pads")
                                               .toArray();
            for (int oldPad = 0; oldPad < 4; ++oldPad) {
                const QJsonObject obj = oldPads.at(oldPad).toObject();
                auto& cfg = m_config.banks[newBank].pads[oldSlot * 4 + oldPad];
                cfg.effectId = mixxx::padfx::legacyEffectId(
                        clampInt(obj.value("effect").toInt(0), 0, 2));
                cfg.targetMask = clampInt(obj.value("target").toInt(1), 0, 15);
                cfg.params = mixxx::padfx::defaultPadParams(cfg.effectId);
                const QJsonArray params = obj.value("params").toArray();
                for (int param = 0;
                        param < params.size() && param < kMaxPadParams;
                        ++param) {
                    cfg.params[param] = clamp01(params.at(param).toDouble(0.5));
                }
            }
        }
    }

    if (useLegacyBank) {
        // The old active bank only mattered for the deck that owned the config.
        // The decks read their own legacy bank when they are created.
    }
}

void PadFxPreset::loadConfig() {
    const QString json = m_pConfig->getValue(
            ConfigKey(kConfigGroup, kPresetKey), QString());
    if (!json.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        if (doc.isObject()) {
            const QJsonObject root = doc.object();
            const QJsonArray banks = root.value("banks").toArray();
            const bool newLayout = root.value("version").toInt(0) >= 3 ||
                    (banks.size() == kNumBanks &&
                            banks.at(0).toObject().value("pads").toArray().size() ==
                                    kNumPadsPerBank);
            if (newLayout) {
                setDefaults();
                for (int bank = 0;
                        bank < banks.size() && bank < kNumBanks;
                        ++bank) {
                    const QJsonArray pads = banks.at(bank)
                                                    .toObject()
                                                    .value("pads")
                                                    .toArray();
                    for (int pad = 0;
                            pad < pads.size() && pad < kNumPadsPerBank;
                            ++pad) {
                        const QJsonObject obj = pads.at(pad).toObject();
                        auto& cfg = m_config.banks[bank].pads[pad];
                        cfg.effectId = obj.value("effectId").toString();
                        if (cfg.effectId.isEmpty() && obj.contains("effect")) {
                            cfg.effectId = mixxx::padfx::legacyEffectId(
                                    clampInt(obj.value("effect").toInt(0), 0, 2));
                        }
                        cfg.targetMask = clampInt(
                                obj.value("target").toInt(kDeckTarget), 0, 15);
                        cfg.params = mixxx::padfx::defaultPadParams(cfg.effectId);
                        const QJsonArray params = obj.value("params").toArray();
                        for (int param = 0;
                                param < params.size() && param < kMaxPadParams;
                                ++param) {
                            cfg.params[param] =
                                    clamp01(params.at(param).toDouble(0.5));
                        }
                    }
                }
                return;
            }
            migrateLegacy(root, true);
            saveConfig();
            return;
        }
    }

    // First run after the per-deck banks: Deck 1 becomes the shared preset.
    const QString legacyJson = m_pConfig->getValue(
            ConfigKey(kConfigGroup, kLegacyDeck1Key), QString());
    if (!legacyJson.isEmpty()) {
        const QJsonDocument legacyDoc =
                QJsonDocument::fromJson(legacyJson.toUtf8());
        if (legacyDoc.isObject()) {
            migrateLegacy(legacyDoc.object(), true);
            saveConfig();
            return;
        }
    }

    setDefaults();
}

void PadFxPreset::saveConfig() {
    QJsonObject root;
    root.insert("version", 3);
    QJsonArray banks;
    for (const auto& bank : m_config.banks) {
        QJsonObject bankObject;
        QJsonArray pads;
        for (const auto& pad : bank.pads) {
            QJsonObject padObject;
            padObject.insert("effectId", pad.effectId);
            padObject.insert("target", pad.targetMask);
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
    m_pConfig->setValue(ConfigKey(kConfigGroup, kPresetKey),
            QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

void PadFxPreset::updateCatalog() {
    m_effects = m_pEffectsManager->getPadEffectManifests();
    for (auto& controls : m_controls) {
        for (auto& pEffect : controls.effects) {
            if (pEffect) {
                pEffect->setStepCount(
                        std::max(0, static_cast<int>(m_effects.size()) - 1));
            }
        }
    }
}

int PadFxPreset::effectIndexForId(const QString& effectId) const {
    if (effectId.isEmpty()) {
        return 0;
    }
    for (int i = 0; i < m_effects.size(); ++i) {
        const auto& pManifest = m_effects.at(i);
        if (pManifest->uniqueId() == effectId ||
                pManifest->id() == effectId) {
            return i + 1;
        }
    }
    return 0;
}

void PadFxPreset::syncPadControls(int bankIndex, int pad) {
    if (bankIndex < 0 || bankIndex >= kNumBanks ||
            pad < 0 || pad >= kNumPadsPerBank) {
        return;
    }
    const auto& cfg = m_config.banks[bankIndex].pads[pad];
    auto& controls = m_controls[bankIndex];
    m_syncingControls = true;
    controls.effects[pad]->set(effectIndexForId(cfg.effectId));
    controls.targets[pad]->set(cfg.targetMask);
    for (int param = 0; param < kMaxPadParams; ++param) {
        controls.params[pad][param]->set(cfg.params[param]);
    }
    m_syncingControls = false;
}

void PadFxPreset::syncControls() {
    for (int bank = 0; bank < kNumBanks; ++bank) {
        for (int pad = 0; pad < kNumPadsPerBank; ++pad) {
            syncPadControls(bank, pad);
        }
    }
}

void PadFxPreset::setEffectDefaults(int bankIndex, int pad) {
    auto& cfg = m_config.banks[bankIndex].pads[pad];
    cfg.params = mixxx::padfx::defaultPadParams(cfg.effectId);
    if (mixxx::padfx::isPadFxOnly(cfg.effectId)) {
        return;
    }
    for (const auto& pManifest : m_effects) {
        if (pManifest->id() != cfg.effectId &&
                pManifest->uniqueId() != cfg.effectId) {
            continue;
        }
        int index = 0;
        for (const auto& pParameter : pManifest->parameters()) {
            if (index >= kMaxPadParams) {
                break;
            }
            if (pParameter->parameterType() !=
                            EffectManifestParameter::ParameterType::Knob ||
                    pParameter->id() == QStringLiteral("active")) {
                continue;
            }
            const double minimum = pParameter->getMinimum();
            const double maximum = pParameter->getMaximum();
            const double range = maximum - minimum;
            cfg.params[index] = range > 0.0
                    ? clamp01((pParameter->getDefault() - minimum) / range)
                    : 0.0;
            ++index;
        }
        return;
    }
}

void PadFxPreset::slotEffectChanged(int bankIndex, int pad) {
    if (m_syncingControls) {
        return;
    }
    const double rawIndex =
            m_controls[bankIndex].effects[pad]->get();
    const int index = clampInt(static_cast<int>(std::lround(rawIndex)),
            0,
            std::max(0, static_cast<int>(m_effects.size())));
    auto& cfg = m_config.banks[bankIndex].pads[pad];
    const QString newId = index <= 0 ? QString()
                                     : m_effects.at(index - 1)->uniqueId();
    if (cfg.effectId == newId) {
        if (std::abs(rawIndex - index) > 1e-9) {
            syncPadControls(bankIndex, pad);
        }
        return;
    }
    cfg.effectId = newId;
    setEffectDefaults(bankIndex, pad);
    syncPadControls(bankIndex, pad);
    saveConfig();
    emit padChanged(bankIndex, pad);
}

void PadFxPreset::slotTargetChanged(int bankIndex, int pad) {
    if (m_syncingControls) {
        return;
    }
    auto& cfg = m_config.banks[bankIndex].pads[pad];
    const int target = clampInt(
            static_cast<int>(std::lround(
                    m_controls[bankIndex].targets[pad]->get())),
            0,
            15);
    if (cfg.targetMask == target) {
        return;
    }
    cfg.targetMask = target;
    saveConfig();
    emit padChanged(bankIndex, pad);
}

void PadFxPreset::slotParamChanged(int bankIndex, int pad, int param) {
    if (m_syncingControls) {
        return;
    }
    auto& cfg = m_config.banks[bankIndex].pads[pad];
    const double value = clamp01(
            m_controls[bankIndex].params[pad][param]->get());
    if (cfg.params[param] == value) {
        return;
    }
    cfg.params[param] = value;
    saveConfig();
    emit padChanged(bankIndex, pad);
}

void PadFxPreset::slotVisibleEffectsChanged() {
    updateCatalog();
    syncControls();
    emit allPadsChanged();
}
