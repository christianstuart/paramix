#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <memory>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "effects/effectchain.h"
#include "effects/effectparameter.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "effects/padfx/padfxunit.h"
#include "engine/channelhandle.h"
#include "engine/effects/engineeffectsmanager.h"
#include "engine/effects/groupfeaturestate.h"
#include "mixxxtest.h"
#include "util/sample.h"

namespace {
constexpr int kStemCount = 4;

QString stemGroup(int stem) {
    return QStringLiteral("[Channel1_Stem%1]").arg(stem + 1);
}

void processEventsFor(int milliseconds) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
}

double findParameterValue(const EffectSlotPointer& pSlot, const QString& id) {
    const auto parameters = pSlot->getLoadedParameters();
    for (const auto& list : parameters) {
        for (const auto& pParameter : list) {
            if (pParameter && pParameter->manifest()->id() == id) {
                return pParameter->getValue();
            }
        }
    }
    return -1.0;
}
} // namespace

class PadFxTest : public MixxxTest {
  protected:
    void SetUp() override {
        MixxxTest::SetUp();
        m_pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
        m_pEffectsManager = std::make_shared<EffectsManager>(config(), m_pChannelHandleFactory);

        const ChannelHandleAndGroup master(
                m_pChannelHandleFactory->getOrCreateHandle("[Master]"), "[Master]");
        const ChannelHandleAndGroup masterOutput(
                m_pChannelHandleFactory->getOrCreateHandle("[MasterOutput]"),
                "[MasterOutput]");
        m_pEffectsManager->registerOutputChannel(master);
        m_pEffectsManager->registerInputChannel(masterOutput);
        m_pEffectsManager->setup();

        for (int stem = 0; stem < kStemCount; ++stem) {
            const ChannelHandleAndGroup group(
                    m_pChannelHandleFactory->getOrCreateHandle(stemGroup(stem)),
                    stemGroup(stem));
            m_pEffectsManager->registerInputChannel(group);
            m_pEffectsManager->addStem(group);
        }

        // The real engine creates these controls in EngineDeck/BasetrackPlayer.
        m_pStemCount = std::make_unique<ControlObject>(
                ConfigKey("[Channel1]", "stem_count"));
        m_pStemCount->setReadOnly();
        m_pStemCount->forceSet(kStemCount);
        m_pBpm = std::make_unique<ControlObject>(ConfigKey("[Channel1]", "bpm"));
        m_pBpm->forceSet(120.0);
        for (int stem = 0; stem < kStemCount; ++stem) {
            m_pMutes[stem] = std::make_unique<ControlObject>(
                    ConfigKey(stemGroup(stem), "mute"));
        }

        m_pUnit = std::make_unique<PadFxUnit>(
                QStringLiteral("[Channel1]"), config(), m_pEffectsManager.get());
    }

    std::shared_ptr<ChannelHandleFactory> m_pChannelHandleFactory;
    std::shared_ptr<EffectsManager> m_pEffectsManager;
    std::unique_ptr<ControlObject> m_pStemCount;
    std::unique_ptr<ControlObject> m_pBpm;
    std::array<std::unique_ptr<ControlObject>, kStemCount> m_pMutes;
    std::unique_ptr<PadFxUnit> m_pUnit;
};

TEST_F(PadFxTest, CreatesControlGroupAndDefaultBank) {
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[Channel1_PadFX]", "bank")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[Channel1_PadFX]", "pad1")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[Channel1_PadFX]", "effect1")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[Channel1_PadFX]", "target1")));

    auto pChain = m_pEffectsManager->getPadEffectChain(stemGroup(0));
    ASSERT_NE(nullptr, pChain.data());
    ASSERT_NE(nullptr, pChain->getEffectSlot(0).data());
    EXPECT_EQ(QStringLiteral("org.paramix.effects.vinylbrake"),
            pChain->getEffectSlot(0)->getManifest()->id());
    EXPECT_EQ(QStringLiteral("org.paramix.effects.echoout"),
            pChain->getEffectSlot(2)->getManifest()->id());
}

TEST_F(PadFxTest, BrakeToggleActivatesOnlyTargetStemsAndMutesAfterRamp) {
    // Pad 2 is the second pad slot and defaults to the vocals target.
    auto pVocals = m_pEffectsManager->getPadEffectChain(stemGroup(3))->getEffectSlot(1);
    ASSERT_NE(nullptr, pVocals.data());

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad2"), 1.0);
    processEventsFor(50);

    EXPECT_NEAR(1.0, findParameterValue(pVocals, "active"), 1e-6);
    EXPECT_FALSE(m_pEffectsManager->getPadEffectChain(stemGroup(0))
                    ->getEffectSlot(1)
                    ->isLoaded());

    // Default brake time is one second. Wait for the pad to latch the native
    // stem mute, then toggle it off again.
    processEventsFor(1400);
    EXPECT_EQ(1.0, m_pMutes[3]->get());

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad2"), 0.0);
    processEventsFor(20);
    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad2"), 1.0);
    processEventsFor(50);

    EXPECT_EQ(0.0, m_pMutes[3]->get());
    EXPECT_NEAR(0.0, findParameterValue(pVocals, "active"), 1e-6);
}

TEST_F(PadFxTest, EchoOutLeavesTargetStemMutedAfterTail) {
    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad3"), 1.0);
    processEventsFor(50);

    EXPECT_EQ(1.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad3_active")));

    // Default echo tail is four beats at 120 BPM.
    processEventsFor(2500);
    EXPECT_EQ(1.0, m_pMutes[0]->get());
    EXPECT_EQ(1.0, m_pMutes[1]->get());
    EXPECT_EQ(1.0, m_pMutes[2]->get());
    EXPECT_EQ(0.0, m_pMutes[3]->get());
}

TEST_F(PadFxTest, BrakeSilencesTheStemBufferAndReleasingRestoresIt) {
    processEventsFor(100);
    auto* pEngineEffects = m_pEffectsManager->getEngineEffectsManager();
    ASSERT_NE(nullptr, pEngineEffects);
    const ChannelHandle stemHandle =
            m_pChannelHandleFactory->handleForGroup(stemGroup(0));
    ASSERT_TRUE(stemHandle.valid());

    constexpr int kSamples = 128;
    std::vector<CSAMPLE> buffer(kSamples);
    double phase = 0.0;

    auto processBlock = [&]() {
        for (int i = 0; i < kSamples; i += 2) {
            const CSAMPLE sample = static_cast<CSAMPLE>(0.2 * std::sin(phase));
            buffer[i] = sample;
            buffer[i + 1] = sample;
            phase += 2.0 * M_PI * 440.0 / 48000.0;
            if (phase > 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
        }
        pEngineEffects->onCallbackStart();
        pEngineEffects->processPadFxInPlace(stemHandle,
                m_pEffectsManager->getMainHandle(),
                buffer.data(),
                kSamples,
                mixxx::audio::SampleRate::fromDouble(48000),
                GroupFeatureState{});
    };

    processBlock();
    for (int i = 0; i < 20; ++i) {
        processBlock();
    }
    const double baseline = SampleUtil::rms(buffer.data(), kSamples);
    ASSERT_GT(baseline, 0.0);

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 1.0);
    processBlock();
    auto pBrakeSlot = m_pEffectsManager->getPadEffectChain(stemGroup(0))->getEffectSlot(0);
    EXPECT_NEAR(1.0, findParameterValue(pBrakeSlot, "active"), 1e-6);
    for (int i = 0; i < 800; ++i) {
        processBlock();
        if ((i % 50) == 0) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
    }
    // Let the one-second brake timer fire and latch the native mute.
    processEventsFor(1300);
    EXPECT_LT(SampleUtil::rms(buffer.data(), kSamples), baseline * 0.1);

    // Release and toggle the brake off; the dry path comes back.
    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 0.0);
    processBlock();
    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 1.0);
    EXPECT_NEAR(0.0, findParameterValue(pBrakeSlot, "active"), 1e-6);
    for (int i = 0; i < 100; ++i) {
        processBlock();
    }
    EXPECT_GT(SampleUtil::rms(buffer.data(), kSamples), baseline * 0.5);
}

TEST_F(PadFxTest, NonStemTrackDisablesPads) {
    m_pStemCount->forceSet(0.0);
    processEventsFor(50);

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 1.0);
    processEventsFor(50);
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad1_active")));
}
