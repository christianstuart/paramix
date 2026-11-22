#include "effects/effectslot.h"

#include <QDebug>
#include <cmath>

#include "control/controlencoder.h"
#include "control/controlpushbutton.h"
#include "effects/backends/effectmanifest.h"
#include "effects/defs.h"
#include "effects/effectbuttonparameterslot.h"
#include "effects/effectchain.h"
#include "effects/effectknobparameterslot.h"
#include "effects/effectparameter.h"
#include "effects/effectsmanager.h"
#include "effects/effectsmessenger.h"
#include "effects/presets/effectpreset.h"
#include "effects/presets/effectpresetmanager.h"
#include "effects/visibleeffectslist.h"
#include "engine/effects/engineeffect.h"
#include "moc_effectslot.cpp"
#include "util/math.h"

// The maximum number of effect parameters we're going to support.
constexpr unsigned int kDefaultMaxParameters = 16;

EffectSlot::EffectSlot(const QString& group,
        EffectsManager* pEffectsManager,
        EffectsMessengerPointer pEffectsMessenger,
        const unsigned int iEffectnumber,
        EffectChain* pChainSlot,
        EngineEffectChain* pEngineEffectChain)
        : m_iEffectNumber(iEffectnumber),
          m_group(group),
          m_pEffectsManager(pEffectsManager),
          m_pPresetManager(pEffectsManager->getEffectPresetManager()),
          m_pBackendManager(pEffectsManager->getBackendManager()),
          m_pMessenger(pEffectsMessenger),
          m_pVisibleEffects(m_pEffectsManager->getVisibleEffectsList()),
          m_pChain(pChainSlot),
          m_pEngineEffectChain(pEngineEffectChain),
          m_pEngineEffect(nullptr),
          m_beatFxUpdateInProgress(false) {
    VERIFY_OR_DEBUG_ASSERT(m_pEngineEffectChain) {
        return;
    }

    m_pControlLoaded = std::make_unique<ControlObject>(ConfigKey(m_group, "loaded"));
    m_pControlLoaded->setReadOnly();

    m_pControlNumParameters.insert(EffectParameterType::Knob,
            QSharedPointer<ControlObject>(
                    new ControlObject(ConfigKey(m_group, "num_parameters"))));
    m_pControlNumParameters.insert(EffectParameterType::Button,
            QSharedPointer<ControlObject>(
                    new ControlObject(ConfigKey(m_group, "num_button_parameters"))));
    for (const auto& pControlNumParameters : std::as_const(m_pControlNumParameters)) {
        pControlNumParameters->setReadOnly();
    }

    m_pControlNumParameterSlots.insert(EffectParameterType::Knob,
            QSharedPointer<ControlObject>(
                    new ControlObject(ConfigKey(m_group, "num_parameterslots"))));
    m_pControlNumParameterSlots.insert(EffectParameterType::Button,
            QSharedPointer<ControlObject>(
                    new ControlObject(ConfigKey(m_group, "num_button_parameterslots"))));
    for (const auto& pControlNumParameterSlots : std::as_const(m_pControlNumParameterSlots)) {
        pControlNumParameterSlots->setReadOnly();
    }

    // Default to disabled to prevent accidental activation of effects
    // at the beginning of a set.
    m_pControlEnabled = std::make_unique<ControlPushButton>(ConfigKey(m_group, "enabled"));
    m_pControlEnabled->setButtonMode(mixxx::control::ButtonMode::PowerWindow);
    connect(m_pControlEnabled.get(),
            &ControlObject::valueChanged,
            this,
            &EffectSlot::updateEngineState);

    m_pControlNextEffect = std::make_unique<ControlPushButton>(
            ConfigKey(m_group, "next_effect"));
    connect(m_pControlNextEffect.get(),
            &ControlObject::valueChanged,
            this,
            &EffectSlot::slotNextEffect);

    m_pControlPrevEffect = std::make_unique<ControlPushButton>(
            ConfigKey(m_group, "prev_effect"));
    connect(m_pControlPrevEffect.get(),
            &ControlObject::valueChanged,
            this,
            &EffectSlot::slotPrevEffect);

    m_pControlLoadedEffect = std::make_unique<ControlObject>(
            ConfigKey(m_group, "loaded_effect"));
    m_pControlLoadedEffect->connectValueChangeRequest(
            this,
            &EffectSlot::slotLoadedEffectRequest);

    connect(m_pVisibleEffects.get(),
            &VisibleEffectsList::visibleEffectsListChanged,
            this,
            &EffectSlot::visibleEffectsListChanged);

    // Ignoring no-ops is important since this is for +/- tickers.
    m_pControlEffectSelector = std::make_unique<ControlEncoder>(
            ConfigKey(m_group, "effect_selector"), false);
    connect(m_pControlEffectSelector.get(),
            &ControlObject::valueChanged,
            this,
            &EffectSlot::slotEffectSelector);

    m_pControlClear =
            std::make_unique<ControlPushButton>(ConfigKey(m_group, "clear"));
    connect(m_pControlClear.get(),
            &ControlObject::valueChanged,
            this,
            &EffectSlot::slotClear);

    m_pControlShowPresetList =
            std::make_unique<ControlPushButton>(ConfigKey(m_group, "show_preset_list"));
    m_pControlShowPresetList->connectValueChangeRequest(
            this,
            [this](double value) {
                emit presetListShowRequest(value > 0);
            });
    // All WEffectSelector of this slot receive this. If one of them is visible,
    // it'll return a 'confirm' signal which we handle in slotPresetListVisibleChanged
    // and set the control accordingly.

    for (unsigned int i = 0; i < kDefaultMaxParameters; ++i) {
        addEffectParameterSlot(EffectParameterType::Knob);
        addEffectParameterSlot(EffectParameterType::Button);
    }

    m_pControlMetaParameter = std::make_unique<ControlPotmeter>(
            ConfigKey(m_group, "meta"), 0.0, 1.0);
    // QObject::connect cannot connect to slots with optional parameters using function
    // pointer syntax if the slot has more parameters than the signal, so use a lambda
    // to hack around this limitation.
    connect(m_pControlMetaParameter.get(),
            &ControlObject::valueChanged,
            this,
            [=, this](double value) { slotEffectMetaParameter(value); });
    m_pControlMetaParameter->set(0.0);
    m_pControlMetaParameter->setDefaultValue(0.0);

    m_pControlBeatFx = std::make_unique<ControlObject>(ConfigKey(m_group, "beat_fx"));
    m_pControlBeatFx->setReadOnly();
    m_pControlBeatStep = std::make_unique<ControlObject>(ConfigKey(m_group, "beat_step"));
    m_pControlBeatStep->setReadOnly();
    m_pControlBeatNumSteps = std::make_unique<ControlObject>(
            ConfigKey(m_group, "beat_num_steps"));
    m_pControlBeatNumSteps->setReadOnly();
    m_pControlBeatUp = std::make_unique<ControlPushButton>(
            ConfigKey(m_group, "beat_up"));
    connect(m_pControlBeatUp.get(),
            &ControlObject::valueChanged,
            this,
            &EffectSlot::slotBeatFxStepUp);
    m_pControlBeatDown = std::make_unique<ControlPushButton>(
            ConfigKey(m_group, "beat_down"));
    connect(m_pControlBeatDown.get(),
            &ControlObject::valueChanged,
            this,
            &EffectSlot::slotBeatFxStepDown);
    m_pControlBeatLevel = std::make_unique<ControlPotmeter>(
            ConfigKey(m_group, "beat_level"), 0.0, 1.0);
    connect(m_pControlBeatLevel.get(),
            &ControlObject::valueChanged,
            this,
            &EffectSlot::slotBeatFxLevel);

    for (const auto& slotList : std::as_const(m_parameterSlots)) {
        for (const auto& pSlot : std::as_const(slotList)) {
            // A parameter can be changed by its knob, the meta knob or a
            // controller, all of which end up in the parameter slot.
            connect(pSlot.get(),
                    &EffectParameterSlotBase::valueChanged,
                    this,
                    [this](double) { updateBeatFxControls(); });
        }
    }

    m_beatFxUpdateInProgress = false;
    updateBeatFxControls();

    m_pControlLoaded->forceSet(0.0);
}

EffectSlot::~EffectSlot() {
    // qDebug() << debugString() << "destroyed";
    unloadEffect();
}

void EffectSlot::addToEngine() {
    VERIFY_OR_DEBUG_ASSERT(!isLoaded()) {
        return;
    }

    VERIFY_OR_DEBUG_ASSERT(!m_pEngineEffect) {
        return;
    }

    m_pEngineEffect = new EngineEffect(
            m_pManifest,
            m_pBackendManager,
            m_pChain->getActiveChannels(),
            m_pEffectsManager->registeredInputChannels(),
            m_pChain->engineOutputChannels());

    EffectsRequest* request = new EffectsRequest();
    request->type = EffectsRequest::ADD_EFFECT_TO_CHAIN;
    request->pTargetChain = m_pEngineEffectChain;
    request->AddEffectToChain.pEffect = m_pEngineEffect;
    request->AddEffectToChain.iIndex = m_iEffectNumber;
    m_pMessenger->writeRequest(request);
}

void EffectSlot::removeFromEngine() {
    VERIFY_OR_DEBUG_ASSERT(isLoaded()) {
        return;
    }

    EffectsRequest* request = new EffectsRequest();
    request->type = EffectsRequest::REMOVE_EFFECT_FROM_CHAIN;
    request->pTargetChain = m_pEngineEffectChain;
    request->RemoveEffectFromChain.pEffect = m_pEngineEffect;
    request->RemoveEffectFromChain.iIndex = m_iEffectNumber;
    m_pMessenger->writeRequest(request);

    m_pEngineEffect = nullptr;
}

void EffectSlot::updateEngineState() {
    if (!m_pEngineEffect) {
        return;
    }

    EffectsRequest* pRequest = new EffectsRequest();
    pRequest->type = EffectsRequest::SET_EFFECT_PARAMETERS;
    pRequest->pTargetEffect = m_pEngineEffect;
    pRequest->SetEffectParameters.enabled = m_pControlEnabled->toBool();
    m_pMessenger->writeRequest(pRequest);

    for (const auto& parameterList : std::as_const(m_allParameters)) {
        for (auto const& pParameter : parameterList) {
            pParameter->updateEngineState();
        }
    }
}

void EffectSlot::initializeInputChannel(ChannelHandle inputChannel) {
    if (!m_pEngineEffect) {
        return;
    }
    m_pEngineEffect->initializeInputChannel(inputChannel);
};

EffectManifestPointer EffectSlot::getManifest() const {
    return m_pManifest;
}

void EffectSlot::addEffectParameterSlot(EffectParameterType parameterType) {
    EffectParameterSlotBasePointer pParameterSlot =
            EffectParameterSlotBasePointer();
    if (parameterType == EffectParameterType::Knob) {
        pParameterSlot = QSharedPointer<EffectKnobParameterSlot>::create(
                m_group, m_iNumParameterSlots[parameterType]);
    } else if (parameterType == EffectParameterType::Button) {
        pParameterSlot = QSharedPointer<EffectButtonParameterSlot>::create(
                m_group, m_iNumParameterSlots[parameterType]);
    }
    ++m_iNumParameterSlots[parameterType];
    const auto pCONumParameterSlots = m_pControlNumParameterSlots[parameterType];
    pCONumParameterSlots->forceSet(pCONumParameterSlots->get() + 1);
    VERIFY_OR_DEBUG_ASSERT(m_iNumParameterSlots[parameterType] == pCONumParameterSlots->get()) {
        return;
    }
    m_parameterSlots[parameterType].append(pParameterSlot);
}

unsigned int EffectSlot::numParameters(
        EffectParameterType parameterType) const {
    return m_allParameters.value(parameterType).size();
}

void EffectSlot::setEnabled(bool enabled) {
    m_pControlEnabled->set(enabled);
}

EffectParameterSlotBasePointer EffectSlot::getEffectParameterSlot(
        EffectParameterType parameterType, unsigned int slotNumber) {
    VERIFY_OR_DEBUG_ASSERT(slotNumber <=
            (unsigned)m_parameterSlots.value(parameterType).size()) {
        return nullptr;
    }
    return m_parameterSlots.value(parameterType).at(slotNumber);
}

void EffectSlot::loadEffectFromPreset(const EffectPresetPointer pPreset) {
    EffectManifestPointer pManifest;
    if (pPreset && !pPreset->isEmpty()) {
        pManifest = m_pBackendManager->getManifest(pPreset);
    }
    if (!pManifest) {
        loadEffectInner(nullptr, nullptr, true);
        return;
    }
    loadEffectInner(pManifest, pPreset, true);
}

void EffectSlot::loadEffectWithDefaults(const EffectManifestPointer pManifest) {
    EffectPresetPointer pPreset = m_pPresetManager->getDefaultPreset(pManifest);
    loadEffectInner(pManifest, pPreset, false);
}

void EffectSlot::loadEffectInner(const EffectManifestPointer pManifest,
        EffectPresetPointer pEffectPreset,
        bool adoptMetaknobFromPreset) {
    if (kEffectDebugOutput) {
        if (pManifest) {
            qDebug() << this << m_group << "loading effect" << pManifest->id();
        } else {
            qDebug() << this << m_group << "unloading effect";
        }
    }
    unloadEffect();
    DEBUG_ASSERT(!m_pManifest);

    // The function shall be called only with both pointers set or both null.
    DEBUG_ASSERT(pManifest.isNull() == pEffectPreset.isNull());
    if (!pManifest || !pEffectPreset) {
        // No new effect to load; just unload the old effect and return.
        emit effectChanged();
        return;
    }

    // Don't load an effect into the '---' preset. The preset would remain
    // selected in WEffectChainPresetSelector and WEffectChainPresetButton and
    // therefore couldn't be used to clear the chain.
    // Instead, load an empty, nameless preset, then load the desired effect.
    if (m_pChain->isEmptyPlaceholderPresetLoaded()) {
        m_pChain->loadEmptyNamelessPreset();
    }

    m_pManifest = pManifest;
    addToEngine();

    // Create EffectParameters. Every parameter listed in the manifest must have
    // an EffectParameter created, regardless of whether it is loaded in a slot.
    for (const auto& pManifestParameter : m_pManifest->parameters()) {
        // match the manifest parameter to the preset parameter
        EffectParameterPreset parameterPreset;
        if (pEffectPreset) {
            for (const auto& p : pEffectPreset->getParameterPresets()) {
                if (p.id() == pManifestParameter->id()) {
                    parameterPreset = p;
                    break;
                }
            }
        }
        EffectParameterPointer pParameter(new EffectParameter(
                m_pEngineEffect,
                m_pMessenger,
                pManifestParameter,
                parameterPreset));
        m_allParameters[pManifestParameter->parameterType()].append(pParameter);
    }

    // Map the parameter slots to the EffectParameters.
    // The slot order is determined by the order parameters are listed in the preset.
    int numTypes = static_cast<int>(EffectParameterType::NumTypes);
    for (int parameterTypeId = 0; parameterTypeId < numTypes;
            ++parameterTypeId) {
        const EffectParameterType parameterType =
                static_cast<EffectParameterType>(parameterTypeId);

        if (pEffectPreset && !pEffectPreset.isNull()) {
            m_loadedParameters[parameterType].clear();
            for (const auto& parameterPreset :
                    pEffectPreset->getParameterPresets()) {
                if (parameterPreset.hidden() || parameterPreset.isNull()) {
                    continue;
                }

                const auto& allParameters = m_allParameters.value(parameterType);
                for (const auto& pParameter : allParameters) {
                    if (pParameter->manifest()->id() == parameterPreset.id()) {
                        m_loadedParameters[parameterType].append(pParameter);
                        break;
                    }
                }
            }
        }
    }

    loadParameters();

    m_pControlMetaParameter->setDefaultValue(pManifest->metaknobDefault());

    m_pControlLoaded->forceSet(1.0);

    if (m_pEffectsManager->isAdoptMetaknobSettingEnabled()) {
        if (adoptMetaknobFromPreset) {
            // Update the ControlObject value, but do not sync the parameters
            // with slotEffectMetaParameter. This allows presets to intentionally
            // save parameters in a state inconsistent with the metaknob.
            m_pControlMetaParameter->set(pEffectPreset->metaParameter());
        } else {
            slotEffectMetaParameter(m_pControlMetaParameter->get(), true);
        }
    } else {
        m_pControlMetaParameter->set(pEffectPreset->metaParameter());
        slotEffectMetaParameter(pEffectPreset->metaParameter(), true);
    }

    // ControlObjects are 1-indexed
    m_pControlLoadedEffect->setAndConfirm(m_pVisibleEffects->indexOf(pManifest) + 1);

    if (m_pManifest->isBeatFx() && usableBeatFxSteps().size() < 2) {
        qWarning() << debugString() << "Beat FX" << m_pManifest->id()
                   << "has fewer than two usable steps; treating it as Color FX";
    }
    updateBeatFxControls();

    emit effectChanged();
    updateEngineState();
}

void EffectSlot::unloadEffect() {
    if (!isLoaded()) {
        return;
    }

    m_pControlLoaded->forceSet(0.0);
    m_pControlLoadedEffect->setAndConfirm(0.0);
    for (const auto& pControlNumParameters : std::as_const(m_pControlNumParameters)) {
        pControlNumParameters->forceSet(0.0);
    }

    for (const auto& slotList : std::as_const(m_parameterSlots)) {
        // Do not delete the slots; clear the parameters from the slots
        // The parameter slots are used by the next effect, but the EffectParameters
        // are deleted below.
        for (auto pSlot : std::as_const(slotList)) {
            pSlot->clear();
        }
    }
    for (auto& parameterList : m_allParameters) {
        parameterList.clear();
    }
    for (auto& parameterList : m_loadedParameters) {
        parameterList.clear();
    }
    for (auto& parameterList : m_hiddenParameters) {
        parameterList.clear();
    }

    m_pControlMetaParameter->setDefaultValue(0.0);

    m_pManifest.clear();

    removeFromEngine();

    updateBeatFxControls();
}

void EffectSlot::loadParameters() {
    // qDebug() << this << m_group << "loading parameters";
    int numTypes = static_cast<int>(EffectParameterType::NumTypes);
    for (int parameterTypeId = 0; parameterTypeId < numTypes;
            ++parameterTypeId) {
        const EffectParameterType parameterType =
                static_cast<EffectParameterType>(parameterTypeId);

        m_pControlNumParameters[parameterType]->forceSet(
                numParameters(parameterType));

        int slot = 0;
        const auto& loadedParameters = m_loadedParameters.value(parameterType);
        for (const auto& pParameter : loadedParameters) {
            // LV2 effects may have more parameters than there are slots available
            if ((unsigned)slot >= kDefaultMaxParameters) {
                return;
            }
            VERIFY_OR_DEBUG_ASSERT(
                    slot <= m_parameterSlots.value(parameterType).size()) {
                break;
            }
            m_parameterSlots.value(parameterType)
                    .at(slot)
                    ->loadParameter(pParameter);
            slot++;
        }

        // Clear any EffectParameterSlots that still have a loaded parameter from before
        // but the loop above did not load a new parameter into them.
        for (; slot < m_parameterSlots.value(parameterType).size(); slot++) {
            m_parameterSlots.value(parameterType).at(slot)->clear();
        }

        m_hiddenParameters[parameterType].clear();
        const auto& allParameters = m_allParameters.value(parameterType);
        for (const auto& pParameter : allParameters) {
            if (!m_loadedParameters.value(parameterType).contains(pParameter)) {
                m_hiddenParameters[parameterType].append(pParameter);
            }
        }
    }
}

void EffectSlot::hideParameter(EffectParameterPointer pParameter) {
    auto parameterType = pParameter->manifest()->parameterType();
    VERIFY_OR_DEBUG_ASSERT(
            m_allParameters.value(parameterType).contains(pParameter)) {
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(
            !m_hiddenParameters.value(parameterType).contains(pParameter)) {
        return;
    }
    m_loadedParameters[parameterType].removeAll(pParameter);
    loadParameters();
    emit parametersChanged();
}

void EffectSlot::showParameter(EffectParameterPointer pParameter) {
    auto parameterType = pParameter->manifest()->parameterType();
    VERIFY_OR_DEBUG_ASSERT(
            m_allParameters.value(parameterType).contains(pParameter)) {
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(
            !m_loadedParameters.value(parameterType).contains(pParameter)) {
        return;
    }
    m_loadedParameters[parameterType].append(pParameter);
    loadParameters();
    emit parametersChanged();
}

void EffectSlot::swapParameters(EffectParameterType type, int index1, int index2) {
    if (index1 == index2) {
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(m_loadedParameters[type].size() > index1) {
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(m_loadedParameters[type].size() > index2) {
        return;
    }
    m_loadedParameters[type].swapItemsAt(index1, index2);
    loadParameters();
    emit parametersChanged();
}

void EffectSlot::slotPrevEffect(double v) {
    if (v > 0) {
        loadEffectWithDefaults(m_pVisibleEffects->previous(m_pManifest));
    }
}

void EffectSlot::slotNextEffect(double v) {
    if (v > 0) {
        loadEffectWithDefaults(m_pVisibleEffects->next(m_pManifest));
    }
}

void EffectSlot::slotLoadedEffectRequest(double value) {
    // ControlObjects are 1-indexed
    int index = static_cast<int>(value) - 1;
    if (index < 0 || index >= m_pVisibleEffects->getList().size()) {
        return;
    }
    // loadEffectInner calls setAndConfirm
    loadEffectWithDefaults(m_pVisibleEffects->at(index));
}

void EffectSlot::visibleEffectsListChanged() {
    if (isLoaded()) {
        // ControlObjects are 1-indexed
        m_pControlLoadedEffect->setAndConfirm(
                m_pVisibleEffects->indexOf(m_pManifest) + 1);
    }
}

void EffectSlot::slotEffectSelector(double v) {
    if (v > 0) {
        loadEffectWithDefaults(m_pVisibleEffects->next(m_pManifest));
    } else if (v < 0) {
        loadEffectWithDefaults(m_pVisibleEffects->previous(m_pManifest));
    }
}

void EffectSlot::slotClear(double v) {
    if (v > 0) {
        unloadEffect();
        emit effectChanged();
    }
}

void EffectSlot::syncSofttakeover() {
    for (const auto& parameterSlotList : std::as_const(m_parameterSlots)) {
        for (const auto& pParameterSlot : std::as_const(parameterSlotList)) {
            if (pParameterSlot->parameterType() == EffectParameterType::Knob) {
                pParameterSlot->syncSofttakeover();
            }
        }
    }
}

double EffectSlot::getMetaParameter() const {
    return m_pControlMetaParameter->get();
}

// This function is for the superknob to update individual effects' meta knobs
// slotEffectMetaParameter does not need to update m_pControlMetaParameter's value
void EffectSlot::setMetaParameter(double v, bool force) {
    if (!m_metaknobSoftTakeover.ignore(*m_pControlMetaParameter, v) ||
            !m_pControlEnabled->toBool() || force) {
        m_pControlMetaParameter->set(v);
        slotEffectMetaParameter(v, force);
    }
}

void EffectSlot::slotEffectMetaParameter(double v, bool force) {
    // Clamp to [0.0, 1.0]
    if (v < 0.0 || v > 1.0) {
        qWarning() << debugString() << "value out of limits";
        v = math_clamp(v, 0.0, 1.0);
        m_pControlMetaParameter->set(v);
    }
    if (!m_pControlEnabled->toBool()) {
        force = true;
    }

    // Only knobs are linked to the metaknob; not buttons
    const auto& knobParameters = m_parameterSlots.value(EffectParameterType::Knob);
    for (const auto& pParameterSlot : std::as_const(knobParameters)) {
        if (pParameterSlot->parameterType() == EffectParameterType::Knob) {
            pParameterSlot->onEffectMetaParameterChanged(v, force);
        }
    }
}

void EffectSlot::slotPresetListVisibleChanged(bool visible) {
    m_pControlShowPresetList->setAndConfirm(visible ? 1.0 : 0.0);
}

EffectManifestParameterPointer EffectSlot::manifestParameterById(const QString& id) const {
    if (!m_pManifest) {
        return EffectManifestParameterPointer();
    }
    for (const auto& pParameter : m_pManifest->parameters()) {
        if (pParameter->id() == id) {
            return pParameter;
        }
    }
    return EffectManifestParameterPointer();
}

EffectParameterPointer EffectSlot::effectParameterById(const QString& id) const {
    for (const auto& parameterList : m_allParameters) {
        for (const auto& pParameter : parameterList) {
            if (pParameter->manifest()->id() == id) {
                return pParameter;
            }
        }
    }
    return EffectParameterPointer();
}

EffectParameterSlotBasePointer EffectSlot::loadedParameterSlot(
        const EffectParameterPointer& pParameter) const {
    if (!pParameter) {
        return EffectParameterSlotBasePointer();
    }
    for (const auto& slotList : m_parameterSlots) {
        for (const auto& pSlot : slotList) {
            if (pSlot->isLoaded() &&
                    pSlot->getManifest()->id() == pParameter->manifest()->id()) {
                return pSlot;
            }
        }
    }
    return EffectParameterSlotBasePointer();
}

void EffectSlot::setEffectParameterValue(
        const EffectParameterPointer& pParameter, double value) {
    if (!pParameter) {
        return;
    }
    const auto pSlot = loadedParameterSlot(pParameter);
    if (pSlot) {
        // Sets the parameterN control, which updates the EffectParameter and
        // the engine exactly like a controller knob does.
        pSlot->setParameterValue(value);
    } else {
        pParameter->setValue(value);
    }
}

QList<double> EffectSlot::usableBeatFxSteps() const {
    if (!m_pManifest || !m_pManifest->isBeatFx()) {
        return QList<double>();
    }
    const auto pStepParameter =
            manifestParameterById(m_pManifest->beatFxStepParameterId());
    if (!pStepParameter) {
        return QList<double>();
    }
    QList<double> steps;
    for (double step : m_pManifest->beatFxSteps()) {
        if (step >= pStepParameter->getMinimum() &&
                step <= pStepParameter->getMaximum()) {
            steps.append(step);
        }
    }
    return steps;
}

bool EffectSlot::isBeatFx() const {
    return usableBeatFxSteps().size() >= 2;
}

void EffectSlot::stepBeatFx(int direction) {
    if (direction == 0 || !isBeatFx()) {
        return;
    }
    const auto pStepParameter = effectParameterById(m_pManifest->beatFxStepParameterId());
    if (!pStepParameter) {
        return;
    }
    const QList<double> steps = usableBeatFxSteps();
    const double current = pStepParameter->getValue();
    // Small epsilon so an exact match is not selected again.
    constexpr double kEpsilon = 1e-6;

    bool found = false;
    double target = 0.0;
    if (direction > 0) {
        for (double step : steps) {
            if (step > current + kEpsilon) {
                target = step;
                found = true;
                break;
            }
        }
    } else {
        for (double step : steps) {
            if (step < current - kEpsilon) {
                target = step;
                found = true;
            }
        }
    }

    if (found) {
        setEffectParameterValue(pStepParameter, target);
        updateBeatFxControls();
    }
}

void EffectSlot::slotBeatFxStepUp(double v) {
    if (v > 0) {
        stepBeatFx(1);
    }
}

void EffectSlot::slotBeatFxStepDown(double v) {
    if (v > 0) {
        stepBeatFx(-1);
    }
}

void EffectSlot::slotBeatFxLevel(double v) {
    if (m_beatFxUpdateInProgress || !isBeatFx()) {
        return;
    }
    if (m_pManifest->beatFxLevelParameterId().isEmpty()) {
        return;
    }
    const auto pLevelParameter =
            effectParameterById(m_pManifest->beatFxLevelParameterId());
    if (!pLevelParameter) {
        return;
    }
    const auto pManifestParameter = pLevelParameter->manifest();
    const double minimum = pManifestParameter->getMinimum();
    const double maximum = pManifestParameter->getMaximum();
    const double value = minimum + math_clamp(v, 0.0, 1.0) * (maximum - minimum);
    setEffectParameterValue(pLevelParameter, value);
    updateBeatFxControls();
}

void EffectSlot::updateBeatFxControls() {
    if (!m_pControlBeatFx) {
        return;
    }
    const bool wasUpdating = m_beatFxUpdateInProgress;
    m_beatFxUpdateInProgress = true;

    if (!isBeatFx()) {
        m_pControlBeatFx->forceSet(0.0);
        m_pControlBeatStep->forceSet(-1.0);
        m_pControlBeatNumSteps->forceSet(0.0);
        m_pControlBeatLevel->set(0.0);
        m_beatFxUpdateInProgress = wasUpdating;
        return;
    }

    const QList<double> steps = usableBeatFxSteps();
    m_pControlBeatFx->forceSet(1.0);
    m_pControlBeatNumSteps->forceSet(static_cast<double>(steps.size()));

    const auto pStepParameter = effectParameterById(m_pManifest->beatFxStepParameterId());
    int nearestIndex = -1;
    if (pStepParameter) {
        const double current = pStepParameter->getValue();
        double nearestDistance = 0.0;
        for (int i = 0; i < steps.size(); ++i) {
            const double distance = std::abs(steps.at(i) - current);
            if (nearestIndex == -1 || distance < nearestDistance) {
                nearestIndex = i;
                nearestDistance = distance;
            }
        }
    }
    m_pControlBeatStep->forceSet(static_cast<double>(nearestIndex));

    if (!m_pManifest->beatFxLevelParameterId().isEmpty()) {
        const auto pLevelParameter =
                effectParameterById(m_pManifest->beatFxLevelParameterId());
        if (pLevelParameter) {
            const double minimum = pLevelParameter->manifest()->getMinimum();
            const double maximum = pLevelParameter->manifest()->getMaximum();
            const double range = maximum - minimum;
            m_pControlBeatLevel->set(range > 0.0
                            ? (pLevelParameter->getValue() - minimum) / range
                            : 0.0);
        } else {
            m_pControlBeatLevel->set(0.0);
        }
    } else {
        m_pControlBeatLevel->set(0.0);
    }

    m_beatFxUpdateInProgress = wasUpdating;
}
