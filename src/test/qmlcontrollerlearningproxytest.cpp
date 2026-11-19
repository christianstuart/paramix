#include <gtest/gtest.h>

#include <QDir>
#include <QFileInfo>
#include <initializer_list>
#include <memory>
#include <optional>

#include "control/controlpushbutton.h"
#include "controllers/controlpickermenu.h"
#include "controllers/defs_controllers.h"
#include "controllers/legacycontrollermappingfilehandler.h"
#include "controllers/midi/legacymidicontrollermapping.h"
#include "controllers/midi/midicontroller.h"
#include "controllers/midi/midiutils.h"
#include "qml/qmlcontrollerlearningproxy.h"
#include "test/mixxxtest.h"
#include "util/time.h"

namespace {

/// Minimal MIDI controller driving the proxy from tests. It never opens a
/// device; messages are injected with sendMessage().
class FakeMidiController : public MidiController {
  public:
    FakeMidiController()
            : MidiController(QStringLiteral("Fake Midi Controller")) {
    }
    ~FakeMidiController() override = default;

    int open(const QString&) override {
        return 0;
    }
    int close() override {
        return 0;
    }
    bool sendBytes(const QByteArray&) override {
        return true;
    }
    bool isPolling() const override {
        return false;
    }
    void sendShortMsg(unsigned char, unsigned char, unsigned char) override {
    }
    PhysicalTransportProtocol getPhysicalTransportProtocol() const override {
        return PhysicalTransportProtocol::UNKNOWN;
    }
    QString getVendorString() const override {
        return QStringLiteral("Test Manufacturer");
    }
    std::optional<uint16_t> getVendorId() const override {
        return std::nullopt;
    }
    QString getProductString() const override {
        return QStringLiteral("Test Product");
    }
    std::optional<uint16_t> getProductId() const override {
        return std::nullopt;
    }
    QString getSerialNumber() const override {
        return QStringLiteral("123456789");
    }
    std::optional<uint8_t> getUsbInterfaceNumber() const override {
        return std::nullopt;
    }

    void sendMessage(MidiOpCode opCode, uint8_t channel, uint8_t control, uint8_t value) {
        receivedShortMessage(
                MidiUtils::statusFromOpCodeAndChannel(opCode, channel),
                control,
                value,
                mixxx::Time::elapsed());
    }
};

const ConfigKey kCrossfaderKey(QStringLiteral("[Master]"), QStringLiteral("crossfader"));

uint16_t midiKey(MidiOpCode opCode, uint8_t channel, uint8_t control) {
    return MidiKey(MidiUtils::statusFromOpCodeAndChannel(opCode, channel), control).key;
}

} // namespace

class ControlPickerModelTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pModel.reset(ControlPickerModel::createDetached());
    }

    std::unique_ptr<ControlPickerModel> m_pModel;
};

TEST_F(ControlPickerModelTest, ExposesInventoryWithoutMenu) {
    ASSERT_GT(m_pModel->count(), 0);

    const int row = m_pModel->rowForConfigKey(kCrossfaderKey.group, kCrossfaderKey.item);
    ASSERT_GE(row, 0) << "The crossfader must be part of the learnable inventory";

    const QVariantMap entry = m_pModel->get(row);
    EXPECT_EQ(kCrossfaderKey.group, entry.value(QStringLiteral("group")).toString());
    EXPECT_EQ(kCrossfaderKey.item, entry.value(QStringLiteral("key")).toString());
    EXPECT_EQ(QStringLiteral("Crossfader"),
            entry.value(QStringLiteral("title")).toString());
    EXPECT_FALSE(entry.value(QStringLiteral("description")).toString().isEmpty());
    EXPECT_EQ(QStringLiteral("Mixer"),
            entry.value(QStringLiteral("category")).toString());
    const QStringList categoryPath =
            entry.value(QStringLiteral("categoryPath")).toStringList();
    ASSERT_FALSE(categoryPath.isEmpty());
    EXPECT_EQ(QStringLiteral("Mixer"), categoryPath.first());
    const ConfigKey modelKey =
            entry.value(QStringLiteral("configKey")).value<ConfigKey>();
    EXPECT_EQ(kCrossfaderKey.group, modelKey.group);
    EXPECT_EQ(kCrossfaderKey.item, modelKey.item);
}

TEST_F(ControlPickerModelTest, RolesMatchEntryData) {
    const int row = m_pModel->rowForConfigKey(kCrossfaderKey.group, kCrossfaderKey.item);
    ASSERT_GE(row, 0);
    const QModelIndex index = m_pModel->index(row, 0);

    EXPECT_EQ(kCrossfaderKey.group,
            m_pModel->data(index, ControlPickerModel::GroupRole).toString());
    EXPECT_EQ(kCrossfaderKey.item,
            m_pModel->data(index, ControlPickerModel::KeyRole).toString());
    EXPECT_EQ(QStringLiteral("Crossfader"),
            m_pModel->data(index, ControlPickerModel::TitleRole).toString());
    EXPECT_EQ(QStringLiteral("Mixer"),
            m_pModel->data(index, ControlPickerModel::CategoryRole).toString());
    const ConfigKey roleKey =
            m_pModel->data(index, ControlPickerModel::ConfigKeyRole)
                    .value<ConfigKey>();
    EXPECT_EQ(kCrossfaderKey.group, roleKey.group);
    EXPECT_EQ(kCrossfaderKey.item, roleKey.item);
}

TEST_F(ControlPickerModelTest, MenuInventoryAndModelStayInSync) {
    ControlPickerMenu menu(nullptr);
    EXPECT_EQ(menu.controlsAvailable().size(), menu.controlModel()->count());
    EXPECT_EQ(menu.controlTitleForConfigKey(kCrossfaderKey),
            menu.controlModel()
                    ->get(menu.controlModel()->rowForConfigKey(
                            kCrossfaderKey.group, kCrossfaderKey.item))
                    .value(QStringLiteral("title"))
                    .toString());
}

class QmlControllerLearningProxyTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_pController = std::make_unique<FakeMidiController>();
        m_pController->setMapping(std::make_shared<LegacyMidiControllerMapping>());
        m_pLearning = std::make_unique<mixxx::qml::QmlControllerLearningProxy>(config());
    }

    void TearDown() override {
        m_pLearning.reset();
        m_pController.reset();
    }

    void sendButton(uint8_t control) {
        m_pController->sendMessage(MidiOpCode::NoteOn, 0x00, control, 0x7F);
        m_pController->sendMessage(MidiOpCode::NoteOff, 0x00, control, 0x00);
    }

    void sendKnob(uint8_t control, std::initializer_list<uint8_t> values) {
        for (uint8_t value : values) {
            m_pController->sendMessage(MidiOpCode::ControlChange, 0x00, control, value);
        }
    }

    std::shared_ptr<LegacyMidiControllerMapping> workingMapping() const {
        return std::dynamic_pointer_cast<LegacyMidiControllerMapping>(
                m_pController->getMapping());
    }

    const MidiInputMapping* firstMappingFor(uint16_t key) const {
        const auto& mappings = workingMapping()->getInputMappings();
        auto it = mappings.constFind(key);
        if (it == mappings.constEnd()) {
            return nullptr;
        }
        return &it.value();
    }

    std::unique_ptr<FakeMidiController> m_pController;
    std::unique_ptr<mixxx::qml::QmlControllerLearningProxy> m_pLearning;
};

TEST_F(QmlControllerLearningProxyTest, LearnsButtonAndDrivesControl) {
    ControlPushButton playButton(ConfigKey(QStringLiteral("[Channel1]"),
            QStringLiteral("play")));

    ASSERT_TRUE(m_pLearning->start(m_pController.get()));
    EXPECT_TRUE(m_pLearning->isLearning());
    EXPECT_EQ(QStringLiteral("Fake Midi Controller"), m_pLearning->deviceName());

    sendButton(0x10);
    m_pLearning->finishDetection();

    // A button sending Note On 0x7F / Note Off 0x00 is detected as two
    // mappings of the same physical control.
    ASSERT_EQ(2, m_pLearning->suggestedMappingCount());
    const QVariantList suggestions = m_pLearning->suggestedMappings();
    EXPECT_EQ(QStringLiteral("button"),
            suggestions.first().toMap().value(QStringLiteral("type")).toString());
    EXPECT_EQ(0x90, suggestions.first().toMap().value(QStringLiteral("status")).toInt());

    ASSERT_TRUE(m_pLearning->assign(QStringLiteral("[Channel1]"),
            QStringLiteral("play")));

    const auto* pMapping = firstMappingFor(midiKey(MidiOpCode::NoteOn, 0x00, 0x10));
    ASSERT_NE(nullptr, pMapping);
    const ConfigKey assigned = std::get<ConfigKey>(pMapping->control);
    EXPECT_EQ(QStringLiteral("[Channel1]"), assigned.group);
    EXPECT_EQ(QStringLiteral("play"), assigned.item);

    // The learned mapping is active on the controller right away.
    m_pController->sendMessage(MidiOpCode::NoteOn, 0x00, 0x10, 0x7F);
    EXPECT_LT(0.0, playButton.get());

    // The pending model lists the control once.
    ASSERT_EQ(1, m_pLearning->pendingMappings()->rowCount());
    EXPECT_EQ(QStringLiteral("play"),
            m_pLearning->pendingMappings()
                    ->data(m_pLearning->pendingMappings()->index(0, 0),
                            mixxx::qml::QmlControllerLearningPendingMappings::KeyRole)
                    .toString());
}

TEST_F(QmlControllerLearningProxyTest, LearnsKnobAndReassigns) {
    ASSERT_TRUE(m_pLearning->start(m_pController.get()));

    sendKnob(0x20, {0x00, 0x20, 0x40, 0x60, 0x7F});
    m_pLearning->finishDetection();

    ASSERT_EQ(1, m_pLearning->suggestedMappingCount());
    const QVariantMap suggestion = m_pLearning->suggestedMappings().first().toMap();
    EXPECT_EQ(QStringLiteral("knob"),
            suggestion.value(QStringLiteral("type")).toString());
    EXPECT_EQ(0, suggestion.value(QStringLiteral("options")).toInt());

    ASSERT_TRUE(m_pLearning->assign(QStringLiteral("[Channel1]"),
            QStringLiteral("volume")));
    ASSERT_EQ(1, m_pLearning->pendingMappings()->rowCount());

    // Reassigning the same physical control replaces the previous target.
    m_pLearning->finishDetection();
    ASSERT_TRUE(m_pLearning->assign(QStringLiteral("[Channel1]"),
            QStringLiteral("pregain")));

    const auto* pMapping = firstMappingFor(midiKey(MidiOpCode::ControlChange, 0x00, 0x20));
    ASSERT_NE(nullptr, pMapping);
    const ConfigKey assigned = std::get<ConfigKey>(pMapping->control);
    EXPECT_EQ(QStringLiteral("[Channel1]"), assigned.group);
    EXPECT_EQ(QStringLiteral("pregain"), assigned.item);
    ASSERT_EQ(1, m_pLearning->pendingMappings()->rowCount());
    EXPECT_EQ(QStringLiteral("pregain"),
            m_pLearning->pendingMappings()
                    ->data(m_pLearning->pendingMappings()->index(0, 0),
                            mixxx::qml::QmlControllerLearningPendingMappings::KeyRole)
                    .toString());

    // Exactly one input mapping remains on this physical key.
    EXPECT_EQ(1,
            workingMapping()
                    ->getInputMappings()
                    .count(midiKey(MidiOpCode::ControlChange, 0x00, 0x20)));
}

TEST_F(QmlControllerLearningProxyTest, LearnsRelativeEncoderWithOptionOverride) {
    ASSERT_TRUE(m_pLearning->start(m_pController.get()));

    // A two's complement ticker sends +1 (0x01) and -1 (0x7F), never 0x00.
    sendKnob(0x30, {0x01, 0x01, 0x7F, 0x01, 0x7F});
    m_pLearning->finishDetection();

    ASSERT_EQ(1, m_pLearning->suggestedMappingCount());
    const QVariantMap suggestion = m_pLearning->suggestedMappings().first().toMap();
    EXPECT_EQ(QStringLiteral("relative"),
            suggestion.value(QStringLiteral("type")).toString());
    const int guessedOptions = suggestion.value(QStringLiteral("options")).toInt();
    EXPECT_TRUE(guessedOptions & static_cast<int>(MidiOption::SelectKnob));

    // Manual override: invert the relative encoder as well.
    QVariantMap overrides;
    overrides.insert(QStringLiteral("invert"), true);
    ASSERT_TRUE(m_pLearning->assign(QStringLiteral("[Channel1]"),
            QStringLiteral("rate"),
            overrides));

    const auto* pMapping = firstMappingFor(midiKey(MidiOpCode::ControlChange, 0x00, 0x30));
    ASSERT_NE(nullptr, pMapping);
    EXPECT_TRUE(pMapping->options.testFlag(MidiOption::SelectKnob));
    EXPECT_TRUE(pMapping->options.testFlag(MidiOption::Invert));

    QVariantMap message;
    message.insert(QStringLiteral("status"), 0xB0);
    message.insert(QStringLiteral("control"), 0x30);
    const QVariantMap existing = m_pLearning->existingMappingFor(message);
    EXPECT_EQ(QStringLiteral("[Channel1]"),
            existing.value(QStringLiteral("group")).toString());
    EXPECT_EQ(QStringLiteral("rate"),
            existing.value(QStringLiteral("key")).toString());

    // An unmapped message reports no existing mapping.
    message.insert(QStringLiteral("control"), 0x31);
    EXPECT_TRUE(m_pLearning->existingMappingFor(message).isEmpty());
}

TEST_F(QmlControllerLearningProxyTest, SaveWritesUserMappingThatReloads) {
    ASSERT_TRUE(m_pLearning->start(m_pController.get()));

    sendButton(0x10);
    m_pLearning->finishDetection();
    ASSERT_TRUE(m_pLearning->assign(QStringLiteral("[Channel1]"),
            QStringLiteral("play")));

    sendKnob(0x20, {0x01, 0x01, 0x7F, 0x01, 0x7F});
    m_pLearning->finishDetection();
    ASSERT_TRUE(m_pLearning->assign(QStringLiteral("[Channel1]"),
            QStringLiteral("rate")));

    const QString filePath = m_pLearning->save(QStringLiteral("F10 Test Mapping"));
    ASSERT_FALSE(filePath.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(filePath));
    EXPECT_TRUE(filePath.startsWith(userMappingsPath(config())));
    EXPECT_EQ(filePath, m_pLearning->mappingFilePath());
    EXPECT_EQ(QStringLiteral("F10 Test Mapping"), m_pLearning->mappingName());
    EXPECT_FALSE(workingMapping()->isDirty());

    const auto pReloaded = std::dynamic_pointer_cast<LegacyMidiControllerMapping>(
            LegacyControllerMappingFileHandler::loadMapping(
                    QFileInfo(filePath), QDir()));
    ASSERT_NE(nullptr, pReloaded);
    EXPECT_EQ(QStringLiteral("F10 Test Mapping"), pReloaded->name());

    const auto& inputMappings = pReloaded->getInputMappings();
    EXPECT_EQ(3, inputMappings.size());
    bool foundPlay = false;
    bool foundRate = false;
    for (const MidiInputMapping& mapping : inputMappings) {
        const auto* pControl = std::get_if<ConfigKey>(&mapping.control);
        ASSERT_NE(nullptr, pControl);
        if (*pControl == ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("play"))) {
            foundPlay = true;
        }
        if (*pControl == ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("rate"))) {
            foundRate = true;
            EXPECT_TRUE(mapping.options.testFlag(MidiOption::SelectKnob));
        }
    }
    EXPECT_TRUE(foundPlay);
    EXPECT_TRUE(foundRate);
}

TEST_F(QmlControllerLearningProxyTest, StopEndsLearningWithoutLosingMapping) {
    ASSERT_TRUE(m_pLearning->start(m_pController.get()));
    sendButton(0x10);
    m_pLearning->finishDetection();
    ASSERT_TRUE(m_pLearning->assign(QStringLiteral("[Channel1]"),
            QStringLiteral("play")));

    // Stopping ends hardware listening but keeps the learned mapping.
    m_pLearning->stop();
    EXPECT_FALSE(m_pLearning->isLearning());
    EXPECT_EQ(1, m_pLearning->pendingMappings()->rowCount());
    EXPECT_NE(nullptr, firstMappingFor(midiKey(MidiOpCode::NoteOn, 0x00, 0x10)));
}
