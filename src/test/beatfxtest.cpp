#include <gtest/gtest.h>

#include <memory>

#include "control/controlobject.h"
#include "effects/backends/builtin/echoeffect.h"
#include "effects/backends/builtin/filtereffect.h"
#include "effects/effectchain.h"
#include "effects/effectparameter.h"
#include "effects/effectparameterslotbase.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "effects/presets/effectchainpreset.h"
#include "engine/channelhandle.h"
#include "qml/qmleffectslotproxy.h"
#include "qml/qmleffectsmanagerproxy.h"
#include "qml/qmlvisibleeffectsmodel.h"
#include "test/mixxxtest.h"

namespace {

constexpr int kMaxParameterSlots = 16;

double parameterValue(const EffectSlotPointer& pSlot, const QString& parameterId) {
    const ParameterMap maps[] = {
            pSlot->getLoadedParameters(), pSlot->getHiddenParameters()};
    for (const auto& parameterMap : maps) {
        for (const auto& parameterList : parameterMap) {
            for (const auto& pParameter : parameterList) {
                if (pParameter->manifest()->id() == parameterId) {
                    return pParameter->getValue();
                }
            }
        }
    }
    return -1.0;
}

int parameterSlotNumber(const EffectSlotPointer& pSlot, const QString& parameterId) {
    for (int i = 0; i < kMaxParameterSlots; ++i) {
        const auto pParameterSlot =
                pSlot->getEffectParameterSlot(EffectParameterType::Knob, i);
        if (pParameterSlot && pParameterSlot->isLoaded() &&
                pParameterSlot->getManifest()->id() == parameterId) {
            return i + 1;
        }
    }
    return -1;
}

void press(const QString& group, const QString& key) {
    // A push button only emits when its value changes.
    ControlObject::set(ConfigKey(group, key), 0.0);
    ControlObject::set(ConfigKey(group, key), 1.0);
}

double controlValue(const QString& group, const QString& key) {
    return ControlObject::get(ConfigKey(group, key));
}

} // namespace

class BeatFxTest : public MixxxTest {
  protected:
    void SetUp() override {
        MixxxTest::SetUp();
        m_pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
        m_pEffectsManager = std::make_shared<EffectsManager>(
                config(), m_pChannelHandleFactory);
        const QString mainOutputGroup = QStringLiteral("[MasterOutput]");
        m_pEffectsManager->registerInputChannel(ChannelHandleAndGroup(
                m_pChannelHandleFactory->getOrCreateHandle(mainOutputGroup),
                mainOutputGroup));
        m_pEffectsManager->setup();
        m_pProxy = std::make_unique<mixxx::qml::QmlEffectsManagerProxy>(
                m_pEffectsManager);
        m_pEffectSlot = m_pEffectsManager->getStandardEffectChain(0)->getEffectSlot(0);
        m_group = m_pEffectSlot->getGroup();
    }

    void TearDown() override {
        m_pProxy.reset();
        for (int unitIndex = 0; unitIndex < kNumStandardEffectUnits; ++unitIndex) {
            const auto pChain = m_pEffectsManager->getStandardEffectChain(unitIndex);
            auto pEmptyPreset = EffectChainPresetPointer::create();
            pEmptyPreset->setName(QString());
            pChain->loadChainPreset(pEmptyPreset);
        }
        m_pEffectsManager.reset();
    }

    void loadEffect(const QString& effectId) {
        m_pEffectSlot->loadEffectWithDefaults(
                m_pEffectsManager->getBackendManager()->getManifestFromUniqueId(
                        effectId));
    }

    std::shared_ptr<ChannelHandleFactory> m_pChannelHandleFactory;
    std::shared_ptr<EffectsManager> m_pEffectsManager;
    std::unique_ptr<mixxx::qml::QmlEffectsManagerProxy> m_pProxy;
    EffectSlotPointer m_pEffectSlot;
    QString m_group;
};

TEST_F(BeatFxTest, EchoIsBeatFxWithSevenSteps) {
    loadEffect(EchoEffect::getId());

    EXPECT_EQ(1.0, controlValue(m_group, QStringLiteral("beat_fx")));
    EXPECT_EQ(7.0, controlValue(m_group, QStringLiteral("beat_num_steps")));
    EXPECT_EQ(2.0, controlValue(m_group, QStringLiteral("beat_step")));

    press(m_group, QStringLiteral("beat_up"));
    EXPECT_DOUBLE_EQ(0.75, parameterValue(m_pEffectSlot, QStringLiteral("delay_time")));
    EXPECT_EQ(3.0, controlValue(m_group, QStringLiteral("beat_step")));

    press(m_group, QStringLiteral("beat_up"));
    EXPECT_DOUBLE_EQ(1.0, parameterValue(m_pEffectSlot, QStringLiteral("delay_time")));
    EXPECT_EQ(4.0, controlValue(m_group, QStringLiteral("beat_step")));

    press(m_group, QStringLiteral("beat_down"));
    EXPECT_DOUBLE_EQ(0.75, parameterValue(m_pEffectSlot, QStringLiteral("delay_time")));

    press(m_group, QStringLiteral("beat_down"));
    EXPECT_DOUBLE_EQ(0.5, parameterValue(m_pEffectSlot, QStringLiteral("delay_time")));
    EXPECT_EQ(2.0, controlValue(m_group, QStringLiteral("beat_step")));
}

TEST_F(BeatFxTest, BeatKeysStopAtTheEnds) {
    loadEffect(EchoEffect::getId());

    const int slotNumber =
            parameterSlotNumber(m_pEffectSlot, QStringLiteral("delay_time"));
    ASSERT_GT(slotNumber, 0);
    const ConfigKey parameterKey(m_group,
            QStringLiteral("parameter%1").arg(slotNumber));

    ControlObject::set(parameterKey, 1 / 8.0);
    press(m_group, QStringLiteral("beat_down"));
    EXPECT_DOUBLE_EQ(1 / 8.0, parameterValue(m_pEffectSlot, QStringLiteral("delay_time")));

    ControlObject::set(parameterKey, 2.0);
    press(m_group, QStringLiteral("beat_up"));
    EXPECT_DOUBLE_EQ(2.0, parameterValue(m_pEffectSlot, QStringLiteral("delay_time")));
}

TEST_F(BeatFxTest, BeatStepFollowsDirectParameterChange) {
    loadEffect(EchoEffect::getId());

    const int slotNumber =
            parameterSlotNumber(m_pEffectSlot, QStringLiteral("delay_time"));
    ASSERT_GT(slotNumber, 0);

    ControlObject::set(ConfigKey(m_group,
                               QStringLiteral("parameter%1").arg(slotNumber)),
            1.5);
    EXPECT_EQ(5.0, controlValue(m_group, QStringLiteral("beat_step")));
}

TEST_F(BeatFxTest, BeatLevelMapsToParameterRangeAndFollows) {
    loadEffect(EchoEffect::getId());

    ControlObject::set(ConfigKey(m_group, QStringLiteral("beat_level")), 0.0);
    EXPECT_DOUBLE_EQ(0.0, parameterValue(m_pEffectSlot, QStringLiteral("send_amount")));

    ControlObject::set(ConfigKey(m_group, QStringLiteral("beat_level")), 1.0);
    EXPECT_DOUBLE_EQ(1.0, parameterValue(m_pEffectSlot, QStringLiteral("send_amount")));

    // Change the level parameter by its own control and check that
    // beat_level follows.
    const int slotNumber =
            parameterSlotNumber(m_pEffectSlot, QStringLiteral("send_amount"));
    if (slotNumber > 0) {
        ControlObject::set(ConfigKey(m_group,
                                   QStringLiteral("parameter%1").arg(slotNumber)),
                0.25);
        EXPECT_NEAR(0.25, controlValue(m_group, QStringLiteral("beat_level")), 1e-9);
    }
    EXPECT_NEAR(parameterValue(m_pEffectSlot, QStringLiteral("send_amount")),
            controlValue(m_group, QStringLiteral("beat_level")),
            1e-9);
}

TEST_F(BeatFxTest, ColorFxHasNoBeatControls) {
    loadEffect(FilterEffect::getId());

    EXPECT_EQ(0.0, controlValue(m_group, QStringLiteral("beat_fx")));
    EXPECT_EQ(-1.0, controlValue(m_group, QStringLiteral("beat_step")));
    EXPECT_EQ(0.0, controlValue(m_group, QStringLiteral("beat_num_steps")));

    // These must not change anything.
    press(m_group, QStringLiteral("beat_up"));
    press(m_group, QStringLiteral("beat_down"));
    ControlObject::set(ConfigKey(m_group, QStringLiteral("beat_level")), 1.0);
    EXPECT_EQ(-1.0, controlValue(m_group, QStringLiteral("beat_step")));
}

TEST_F(BeatFxTest, UnloadingResetsBeatControls) {
    loadEffect(EchoEffect::getId());
    ASSERT_EQ(1.0, controlValue(m_group, QStringLiteral("beat_fx")));

    m_pEffectSlot->slotClear(1.0);

    EXPECT_EQ(0.0, controlValue(m_group, QStringLiteral("beat_fx")));
    EXPECT_EQ(-1.0, controlValue(m_group, QStringLiteral("beat_step")));
    EXPECT_EQ(0.0, controlValue(m_group, QStringLiteral("beat_num_steps")));
    EXPECT_EQ(0.0, controlValue(m_group, QStringLiteral("beat_level")));
}

TEST_F(BeatFxTest, ProxyExposesBeatFxMetadata) {
    std::unique_ptr<mixxx::qml::QmlEffectSlotProxy> pSlotProxy(
            m_pProxy->getEffectSlot(1, 1));
    ASSERT_NE(nullptr, pSlotProxy);

    pSlotProxy->setEffectId(EchoEffect::getId());
    EXPECT_TRUE(pSlotProxy->isBeatFx());
    EXPECT_EQ(QStringList({QStringLiteral("1/8"),
                      QStringLiteral("1/4"),
                      QStringLiteral("1/2"),
                      QStringLiteral("3/4"),
                      QStringLiteral("1"),
                      QStringLiteral("3/2"),
                      QStringLiteral("2")}),
            pSlotProxy->getBeatStepLabels());
    EXPECT_EQ(QStringLiteral("beats"), pSlotProxy->getBeatStepUnit());
    EXPECT_EQ(QStringLiteral("Send"), pSlotProxy->getBeatLevelName());
}

TEST_F(BeatFxTest, PadModelExposesBeatStepMetadata) {
    auto* pPadModel = m_pProxy->property("padEffectsModel")
                              .value<mixxx::qml::QmlVisibleEffectsModel*>();
    ASSERT_NE(nullptr, pPadModel);

    int echoRow = -1;
    int filterRow = -1;
    for (int row = 1; row < pPadModel->rowCount({}); ++row) {
        const QString id = pPadModel->get(row).toMap().value("effectId").toString();
        if (id == EchoEffect::getId()) {
            echoRow = row;
        } else if (id == FilterEffect::getId()) {
            filterRow = row;
        }
    }
    ASSERT_GT(echoRow, 0);
    ASSERT_GT(filterRow, 0);

    const QVariantMap echo = pPadModel->get(echoRow).toMap();
    EXPECT_TRUE(echo.value("isBeatFx").toBool());
    EXPECT_EQ(7, echo.value("beatStepLabels").toStringList().size());
    EXPECT_EQ(echo.value("beatStepLabels").toStringList().size(),
            echo.value("beatStepValues").toList().size());
    EXPECT_GE(echo.value("beatStepParameterIndex").toInt(), 0);

    const QVariantMap filter = pPadModel->get(filterRow).toMap();
    EXPECT_FALSE(filter.value("isBeatFx").toBool());
    EXPECT_TRUE(filter.value("beatStepLabels").toStringList().isEmpty());
    EXPECT_EQ(-1, filter.value("beatStepParameterIndex").toInt());
}

TEST_F(BeatFxTest, FilteredModelsSeparateBeatAndColorFx) {
    auto* pBeatModel = m_pProxy->property("beatEffectsModel")
                               .value<mixxx::qml::QmlVisibleEffectsModel*>();
    auto* pColorModel = m_pProxy->property("colorEffectsModel")
                                .value<mixxx::qml::QmlVisibleEffectsModel*>();
    ASSERT_NE(nullptr, pBeatModel);
    ASSERT_NE(nullptr, pColorModel);

    EXPECT_GE(pBeatModel->rowCount({}), 2);
    EXPECT_GE(pColorModel->rowCount({}), 2);
    EXPECT_FALSE(pBeatModel->get(0).toMap().value("isBeatFx").toBool());
    EXPECT_FALSE(pColorModel->get(0).toMap().value("isBeatFx").toBool());
    EXPECT_EQ(kNoEffectString,
            pBeatModel->get(0).toMap().value("display").toString());

    bool echoInBeat = false;
    bool echoInColor = false;
    bool filterInBeat = false;
    bool filterInColor = false;
    for (int row = 1; row < pBeatModel->rowCount({}); ++row) {
        const QString id = pBeatModel->get(row).toMap().value("effectId").toString();
        echoInBeat |= id == EchoEffect::getId();
        filterInBeat |= id == FilterEffect::getId();
    }
    for (int row = 1; row < pColorModel->rowCount({}); ++row) {
        const QString id = pColorModel->get(row).toMap().value("effectId").toString();
        echoInColor |= id == EchoEffect::getId();
        filterInColor |= id == FilterEffect::getId();
    }

    EXPECT_TRUE(echoInBeat);
    EXPECT_FALSE(echoInColor);
    EXPECT_FALSE(filterInBeat);
    EXPECT_TRUE(filterInColor);
}
