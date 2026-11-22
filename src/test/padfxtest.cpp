#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <memory>

#include "control/controlobject.h"
#include "control/controlproxy.h"
#include "effects/effectchain.h"
#include "effects/effectparameter.h"
#include "effects/effectslot.h"
#include "effects/effectsmanager.h"
#include "effects/padfx/padfxpreset.h"
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

int effectIndex(const EffectsManager& effectsManager, const QString& id) {
    const auto manifests = effectsManager.getPadEffectManifests();
    for (int i = 0; i < manifests.size(); ++i) {
        if (manifests.at(i)->id() == id) {
            return i + 1;
        }
    }
    return 0;
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

        const ChannelHandleAndGroup deckGroup(
                m_pChannelHandleFactory->getOrCreateHandle("[Channel1]"),
                "[Channel1]");
        m_pEffectsManager->registerInputChannel(deckGroup);
        m_pEffectsManager->addDeck(deckGroup);

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
        m_pPreset = std::make_unique<PadFxPreset>(
                config(), m_pEffectsManager.get());
        m_pUnit = std::make_unique<PadFxUnit>(QStringLiteral("[Channel1]"),
                config(),
                m_pEffectsManager.get(),
                m_pPreset.get());
    }

    std::shared_ptr<ChannelHandleFactory> m_pChannelHandleFactory;
    std::shared_ptr<EffectsManager> m_pEffectsManager;
    std::unique_ptr<ControlObject> m_pStemCount;
    std::unique_ptr<PadFxPreset> m_pPreset;
    std::unique_ptr<PadFxUnit> m_pUnit;
};

TEST_F(PadFxTest, CreatesControlGroupAndDefaultBank) {
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[Channel1_PadFX]", "bank")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[Channel1_PadFX]", "pad1")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[Channel1_PadFX]", "pad8")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[PadFX]", "fx1_effect1")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[PadFX]", "fx1_target1")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[PadFX]", "fx2_effect8")));

    auto pChain = m_pEffectsManager->getPadEffectChain(stemGroup(0));
    ASSERT_NE(nullptr, pChain.data());
    ASSERT_NE(nullptr, pChain->getEffectSlot(0).data());
    EXPECT_EQ(QStringLiteral("org.paramix.effects.vinylbrake"),
            pChain->getEffectSlot(0)->getManifest()->id());
    EXPECT_EQ(QStringLiteral("org.paramix.effects.echoout"),
            pChain->getEffectSlot(2)->getManifest()->id());
}

TEST_F(PadFxTest, BrakeIsMomentaryAndOnlyTargetsVocals) {
    // Pad 2 is the second pad slot and defaults to the vocals target.
    auto pVocals = m_pEffectsManager->getPadEffectChain(stemGroup(3))->getEffectSlot(1);
    ASSERT_NE(nullptr, pVocals.data());

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad2"), 1.0);
    processEventsFor(50);
    EXPECT_NEAR(1.0, findParameterValue(pVocals, "active"), 1e-6);
    EXPECT_EQ(1.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad2_active")));
    EXPECT_FALSE(m_pEffectsManager->getPadEffectChain(stemGroup(0))
                    ->getEffectSlot(1)
                    ->isLoaded());

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad2"), 0.0);
    processEventsFor(50);
    EXPECT_NEAR(0.0, findParameterValue(pVocals, "active"), 1e-6);
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad2_active")));
}

TEST_F(PadFxTest, PadOnlyEffectSelectedThroughControlUsesActiveGate) {
    const int vinylIndex = effectIndex(*m_pEffectsManager,
            QStringLiteral("org.paramix.effects.vinylbrake"));
    ASSERT_GT(vinylIndex, 0);
    ControlObject::set(ConfigKey("[PadFX]", "fx1_effect1"), vinylIndex);
    processEventsFor(50);

    auto pBrake = m_pEffectsManager->getPadEffectChain(stemGroup(0))->getEffectSlot(0);
    ASSERT_NE(nullptr, pBrake.data());
    ASSERT_TRUE(pBrake->isLoaded());
    EXPECT_EQ(QStringLiteral("org.paramix.effects.vinylbrake"), pBrake->id());

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 1.0);
    processEventsFor(50);
    EXPECT_NEAR(1.0, findParameterValue(pBrake, "active"), 1e-6);

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 0.0);
    processEventsFor(50);
    EXPECT_NEAR(0.0, findParameterValue(pBrake, "active"), 1e-6);
}

TEST_F(PadFxTest, EchoOutIsMomentary) {
    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad3"), 1.0);
    processEventsFor(50);
    EXPECT_EQ(1.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad3_active")));

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad3"), 0.0);
    processEventsFor(50);
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad3_active")));
    auto pEcho = m_pEffectsManager->getPadEffectChain(stemGroup(0))->getEffectSlot(2);
    ASSERT_NE(nullptr, pEcho.data());
    EXPECT_NEAR(0.0, findParameterValue(pEcho, "active"), 1e-6);
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
    EXPECT_LT(SampleUtil::rms(buffer.data(), kSamples), baseline * 0.1);

    // Release the momentary pad; the brake winds the stem back up.
    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 0.0);
    EXPECT_NEAR(0.0, findParameterValue(pBrakeSlot, "active"), 1e-6);
    for (int i = 0; i < 800; ++i) {
        processBlock();
    }
    EXPECT_GT(SampleUtil::rms(buffer.data(), kSamples), baseline * 0.5);
}

TEST_F(PadFxTest, EchoOutReadsForwardThroughTheDelayLine) {
    processEventsFor(100);
    auto* pEngineEffects = m_pEffectsManager->getEngineEffectsManager();
    ASSERT_NE(nullptr, pEngineEffects);
    const ChannelHandle stemHandle =
            m_pChannelHandleFactory->handleForGroup(stemGroup(0));
    ASSERT_TRUE(stemHandle.valid());

    // A short delay keeps the test fast without needing a beatgrid.
    ControlObject::set(ConfigKey("[PadFX]", "fx1_pad3_param1"), 0.0);
    // Disable reverb for a clean single-tap direction check.
    ControlObject::set(ConfigKey("[PadFX]", "fx1_pad3_param4"), 0.0);
    processEventsFor(50);

    constexpr int kSamples = 128;
    std::vector<CSAMPLE> buffer(kSamples);
    double ramp = 0.0;
    auto processBlock = [&]() {
        for (int i = 0; i < kSamples; i += 2) {
            buffer[i] = static_cast<CSAMPLE>(ramp);
            buffer[i + 1] = static_cast<CSAMPLE>(ramp);
            ramp += 1e-5;
        }
        pEngineEffects->onCallbackStart();
        pEngineEffects->processPadFxInPlace(stemHandle,
                m_pEffectsManager->getMainHandle(),
                buffer.data(),
                kSamples,
                mixxx::audio::SampleRate::fromDouble(48000),
                GroupFeatureState{});
    };

    // Fill the delay line with a monotonically rising ramp.
    for (int i = 0; i < 200; ++i) {
        processBlock();
    }

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad3"), 1.0);
    processEventsFor(20);

    // Let the dry/wet crossfade settle, then check the delayed ramp itself.
    for (int block = 0; block < 10; ++block) {
        processBlock();
    }
    const CSAMPLE first = buffer[0];
    const CSAMPLE last = buffer[kSamples - 2];
    EXPECT_GT(std::abs(last - first), 1e-6f);
    // A forward delay reads the rising ramp in the same direction. Allow a
    // small tolerance for the reverb/feedback tail; a reversed buffer produced
    // a much larger falling slope.
    EXPECT_GT(last, first - 1e-3f);
}

TEST_F(PadFxTest, NonStemTrackIgnoresStemTargetButKeepsControls) {
    m_pStemCount->forceSet(0.0);
    processEventsFor(50);

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 1.0);
    processEventsFor(50);
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad1_active")));
    EXPECT_NE(nullptr, ControlObject::getControl(ConfigKey("[PadFX]", "fx1_effect1")));
}

TEST_F(PadFxTest, DeckTargetWorksWithoutStems) {
    m_pStemCount->forceSet(0.0);
    processEventsFor(50);

    const int filterIndex = effectIndex(*m_pEffectsManager,
            QStringLiteral("org.mixxx.effects.filter"));
    ASSERT_GT(filterIndex, 0);
    ControlObject::set(ConfigKey("[PadFX]", "fx1_effect1"), filterIndex);
    ControlObject::set(ConfigKey("[PadFX]", "fx1_target1"), 0.0);
    processEventsFor(50);

    auto pDeckChain = m_pEffectsManager->getPadEffectChain(QStringLiteral("[Channel1]"));
    ASSERT_NE(nullptr, pDeckChain.data());
    auto pSlot = pDeckChain->getEffectSlot(0);
    ASSERT_NE(nullptr, pSlot.data());
    ASSERT_TRUE(pSlot->isLoaded());
    EXPECT_EQ(QStringLiteral("org.mixxx.effects.filter"), pSlot->id());
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey(pSlot->getGroup(), "enabled")));

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 1.0);
    processEventsFor(50);
    EXPECT_EQ(1.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad1_active")));
    EXPECT_EQ(1.0, ControlObject::get(ConfigKey(pSlot->getGroup(), "enabled")));

    ControlObject::set(ConfigKey("[Channel1_PadFX]", "pad1"), 0.0);
    processEventsFor(50);
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "pad1_active")));
    EXPECT_EQ(0.0, ControlObject::get(ConfigKey(pSlot->getGroup(), "enabled")));
}

TEST_F(PadFxTest, PadAssignmentsAreSharedAndBanksStayPerDeck) {
    const QString deck2Group = QStringLiteral("[Channel2]");
    auto stem2Group = [](int stem) {
        return QStringLiteral("[Channel2_Stem%1]").arg(stem + 1);
    };
    const ChannelHandleAndGroup deck2(
            m_pChannelHandleFactory->getOrCreateHandle(deck2Group), deck2Group);
    m_pEffectsManager->registerInputChannel(deck2);
    m_pEffectsManager->addDeck(deck2);
    for (int stem = 0; stem < kStemCount; ++stem) {
        const ChannelHandleAndGroup group(
                m_pChannelHandleFactory->getOrCreateHandle(stem2Group(stem)),
                stem2Group(stem));
        m_pEffectsManager->registerInputChannel(group);
        m_pEffectsManager->addStem(group);
    }
    auto pStemCount2 = std::make_unique<ControlObject>(
            ConfigKey(deck2Group, "stem_count"));
    pStemCount2->setReadOnly();
    pStemCount2->forceSet(kStemCount);
    PadFxUnit unit2(deck2Group,
            config(),
            m_pEffectsManager.get(),
            m_pPreset.get());

    const int filterIndex = effectIndex(*m_pEffectsManager,
            QStringLiteral("org.mixxx.effects.filter"));
    ASSERT_GT(filterIndex, 0);

    ControlObject::set(ConfigKey("[PadFX]", "fx1_effect1"), filterIndex);
    processEventsFor(50);
    auto pStemChain1 = m_pEffectsManager->getPadEffectChain(stemGroup(0));
    auto pStemChain2 = m_pEffectsManager->getPadEffectChain(stem2Group(0));
    ASSERT_NE(nullptr, pStemChain1.data());
    ASSERT_NE(nullptr, pStemChain2.data());
    EXPECT_EQ(QStringLiteral("org.mixxx.effects.filter"),
            pStemChain1->getEffectSlot(0)->id());
    EXPECT_EQ(QStringLiteral("org.mixxx.effects.filter"),
            pStemChain2->getEffectSlot(0)->id());

    ControlObject::set(ConfigKey("[Channel2_PadFX]", "bank"), 2.0);
    processEventsFor(50);
    EXPECT_EQ(2.0, ControlObject::get(ConfigKey("[Channel2_PadFX]", "bank")));
    EXPECT_EQ(1.0, ControlObject::get(ConfigKey("[Channel1_PadFX]", "bank")));
}

class PadFxPresetMigrationTest : public MixxxTest {
  protected:
    void SetUp() override {
        MixxxTest::SetUp();
        m_pChannelHandleFactory = std::make_shared<ChannelHandleFactory>();
        m_pEffectsManager = std::make_shared<EffectsManager>(
                config(), m_pChannelHandleFactory);

        const ChannelHandleAndGroup master(
                m_pChannelHandleFactory->getOrCreateHandle("[Master]"), "[Master]");
        const ChannelHandleAndGroup masterOutput(
                m_pChannelHandleFactory->getOrCreateHandle("[MasterOutput]"),
                "[MasterOutput]");
        m_pEffectsManager->registerOutputChannel(master);
        m_pEffectsManager->registerInputChannel(masterOutput);
        m_pEffectsManager->setup();
    }

    std::shared_ptr<ChannelHandleFactory> m_pChannelHandleFactory;
    std::shared_ptr<EffectsManager> m_pEffectsManager;
};

TEST_F(PadFxPresetMigrationTest, MigratesDeckOneBanksAndIgnoresModes) {
    auto makePad = [](int effect, int target, int mode) {
        QJsonObject pad;
        pad.insert("effect", effect);
        pad.insert("target", target);
        // Old mode values are still present in old configs; they are ignored.
        pad.insert("mode", mode);
        pad.insert("params", QJsonArray{0.1, 0.2, 0.3, 0.4, 0.5});
        return pad;
    };

    QJsonArray banks;
    for (int oldBank = 0; oldBank < 4; ++oldBank) {
        QJsonArray pads;
        for (int oldPad = 0; oldPad < 4; ++oldPad) {
            int effect = 0;
            int target = 1;
            int mode = 1;
            if (oldPad == 0) {
                switch (oldBank) {
                case 0:
                    effect = 1;
                    target = 7;
                    mode = 0;
                    break;
                case 1:
                    effect = 2;
                    target = 8;
                    mode = 2;
                    break;
                case 2:
                    effect = 1;
                    target = 15;
                    mode = 1;
                    break;
                case 3:
                    effect = 2;
                    target = 4;
                    mode = 0;
                    break;
                }
            }
            pads.append(makePad(effect, target, mode));
        }
        banks.append(QJsonObject{{"pads", pads}});
    }
    const QJsonObject legacy{{"bank", 3}, {"banks", banks}};
    config()->setValue(ConfigKey("[PadFX]", "Deck1"),
            QString::fromUtf8(QJsonDocument(legacy).toJson(QJsonDocument::Compact)));

    PadFxPreset preset(config(), m_pEffectsManager.get());

    const QString saved = config()->getValue(
            ConfigKey("[PadFX]", "Preset"), QString());
    ASSERT_FALSE(saved.isEmpty());
    const QJsonObject root = QJsonDocument::fromJson(saved.toUtf8()).object();
    const QJsonArray newBanks = root.value("banks").toArray();
    ASSERT_EQ(2, newBanks.size());

    const QJsonArray fx1 = newBanks.at(0).toObject().value("pads").toArray();
    ASSERT_EQ(8, fx1.size());
    EXPECT_EQ(QStringLiteral("org.paramix.effects.vinylbrake"),
            fx1.at(0).toObject().value("effectId").toString());
    EXPECT_EQ(7, fx1.at(0).toObject().value("target").toInt());
    EXPECT_FALSE(fx1.at(0).toObject().contains("mode"));
    EXPECT_EQ(QStringLiteral("org.paramix.effects.echoout"),
            fx1.at(4).toObject().value("effectId").toString());
    EXPECT_EQ(8, fx1.at(4).toObject().value("target").toInt());

    const QJsonArray fx2 = newBanks.at(1).toObject().value("pads").toArray();
    ASSERT_EQ(8, fx2.size());
    EXPECT_EQ(QStringLiteral("org.paramix.effects.vinylbrake"),
            fx2.at(0).toObject().value("effectId").toString());
    EXPECT_EQ(15, fx2.at(0).toObject().value("target").toInt());
    EXPECT_EQ(QStringLiteral("org.paramix.effects.echoout"),
            fx2.at(4).toObject().value("effectId").toString());
    EXPECT_EQ(4, fx2.at(4).toObject().value("target").toInt());
    EXPECT_FALSE(fx2.at(4).toObject().contains("mode"));
}
