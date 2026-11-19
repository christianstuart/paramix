#include "qml/qmlcontrollerlearningproxy.h"

#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <utility>

#include "controllers/controller.h"
#include "controllers/controllermanager.h"
#include "controllers/controlpickermenu.h"
#include "controllers/defs_controllers.h"
#include "controllers/learningutils.h"
#include "controllers/midi/legacymidicontrollermapping.h"
#include "controllers/midi/midicontroller.h"
#include "controllers/midi/midiutils.h"
#include "moc_qmlcontrollerlearningproxy.cpp"
#include "qml/qmlconfigproxy.h"
#include "qml/qmlpreferencesproxy.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

namespace {

/// The detection window mirrors DlgControllerLearning: a gesture ends after
/// 1.5 s without a message, so the next message starts a fresh gesture.
constexpr int kDetectionTimeoutMillis = 1500;

/// Bounds the gesture buffer so a jog wheel that never pauses cannot make
/// detection progressively more expensive. The wizard keeps everything, but it
/// only guesses once per pause; this proxy updates live.
constexpr int kMaxBufferedMessages = 256;

/// Placeholder used while guessing. LearningUtils only reads the target control
/// to look up a possible reset control, which does not apply before a target
/// has been chosen.
const ConfigKey& pendingControlKey() {
    static const ConfigKey key(QStringLiteral("[ControllerLearning]"),
            QStringLiteral("pending"));
    return key;
}

const QList<MidiOption>& displayOptions() {
    static const QList<MidiOption> options = {
            MidiOption::Invert,
            MidiOption::Rot64,
            MidiOption::Rot64Invert,
            MidiOption::Rot64Fast,
            MidiOption::Diff,
            MidiOption::Button,
            MidiOption::Switch,
            MidiOption::Spread64,
            MidiOption::HercJog,
            MidiOption::HercJogFast,
            MidiOption::SelectKnob,
            MidiOption::SoftTakeover,
            MidiOption::FourteenBitLSB,
            MidiOption::FourteenBitMSB,
    };
    return options;
}

QStringList midiOptionNames(MidiOptions options) {
    QStringList names;
    for (MidiOption option : displayOptions()) {
        if (options.testFlag(option)) {
            names.append(MidiUtils::midiOptionToTranslatedString(option));
        }
    }
    return names;
}

bool isRelativeOption(MidiOptions options) {
    return options.testFlag(MidiOption::SelectKnob) ||
            options.testFlag(MidiOption::Spread64) ||
            options.testFlag(MidiOption::Diff) ||
            options.testFlag(MidiOption::Rot64) ||
            options.testFlag(MidiOption::Rot64Invert) ||
            options.testFlag(MidiOption::Rot64Fast) ||
            options.testFlag(MidiOption::HercJog) ||
            options.testFlag(MidiOption::HercJogFast);
}

QString mappingType(const MidiInputMapping& mapping) {
    if (isRelativeOption(mapping.options)) {
        return QStringLiteral("relative");
    }
    if (mapping.options.testFlag(MidiOption::FourteenBitLSB) ||
            mapping.options.testFlag(MidiOption::FourteenBitMSB)) {
        return QStringLiteral("fourteenBit");
    }
    const MidiOpCode opCode = MidiUtils::opCodeFromStatus(mapping.key.status);
    if (mapping.options.testFlag(MidiOption::Switch) ||
            mapping.options.testFlag(MidiOption::Button) ||
            opCode == MidiOpCode::NoteOn || opCode == MidiOpCode::NoteOff) {
        return QStringLiteral("button");
    }
    if (opCode == MidiOpCode::ControlChange) {
        return QStringLiteral("knob");
    }
    return QStringLiteral("other");
}

QString hexByte(unsigned char value) {
    return QStringLiteral("0x%1").arg(
            QString::number(value, 16).toUpper().rightJustified(2, QChar('0')));
}

QString mappingMessage(const MidiInputMapping& mapping) {
    return QStringLiteral("%1 %2").arg(hexByte(mapping.key.status),
            hexByte(mapping.key.control));
}

} // namespace

QmlControllerLearningPendingMappings::QmlControllerLearningPendingMappings(
        QObject* pParent)
        : QAbstractListModel(pParent) {
}

int QmlControllerLearningPendingMappings::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_mappings.size();
}

QVariant QmlControllerLearningPendingMappings::data(
        const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_mappings.size()) {
        return QVariant();
    }
    const QmlControllerLearningMapping& mapping = m_mappings.at(index.row());
    const MidiInputMapping* pMessage = mapping.mappings.isEmpty()
            ? nullptr
            : &mapping.mappings.first();
    switch (role) {
    case GroupRole:
        return mapping.control.group;
    case KeyRole:
        return mapping.control.item;
    case ConfigKeyRole:
        return QStringLiteral("%1,%2").arg(mapping.control.group, mapping.control.item);
    case TitleRole:
        return mapping.title;
    case DescriptionRole:
        return mapping.description;
    case StatusRole:
        return pMessage ? static_cast<int>(pMessage->key.status) : 0;
    case ControlRole:
        return pMessage ? static_cast<int>(pMessage->key.control) : 0;
    case ChannelRole:
        return pMessage ? static_cast<int>(
                                  MidiUtils::channelFromStatus(pMessage->key.status))
                        : 0;
    case OpCodeRole:
        return pMessage ? static_cast<int>(MidiUtils::opCodeFromStatus(
                                  pMessage->key.status))
                        : 0;
    case OptionsRole:
        return pMessage ? pMessage->options.toInt() : 0;
    case OptionNamesRole:
        return pMessage ? midiOptionNames(pMessage->options) : QStringList();
    case MappingCountRole:
        return mapping.mappings.size();
    case MessageRole:
        return pMessage ? mappingMessage(*pMessage) : QString();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> QmlControllerLearningPendingMappings::roleNames() const {
    return {
            {GroupRole, QByteArrayLiteral("group")},
            {KeyRole, QByteArrayLiteral("key")},
            {ConfigKeyRole, QByteArrayLiteral("configKeyString")},
            {TitleRole, QByteArrayLiteral("title")},
            {DescriptionRole, QByteArrayLiteral("description")},
            {StatusRole, QByteArrayLiteral("status")},
            {ControlRole, QByteArrayLiteral("control")},
            {ChannelRole, QByteArrayLiteral("channel")},
            {OpCodeRole, QByteArrayLiteral("opCode")},
            {OptionsRole, QByteArrayLiteral("options")},
            {OptionNamesRole, QByteArrayLiteral("optionNames")},
            {MappingCountRole, QByteArrayLiteral("mappingCount")},
            {MessageRole, QByteArrayLiteral("message")},
    };
}

QVariantMap QmlControllerLearningPendingMappings::get(int row) const {
    QVariantMap result;
    if (row < 0 || row >= m_mappings.size()) {
        return result;
    }
    const QModelIndex index = this->index(row, 0);
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
        result.insert(QString::fromUtf8(it.value()), data(index, it.key()));
    }
    return result;
}

void QmlControllerLearningPendingMappings::upsert(
        QmlControllerLearningMapping mapping) {
    const int row = rowForControl(mapping.control);
    if (row < 0) {
        beginInsertRows(QModelIndex(), m_mappings.size(), m_mappings.size());
        m_mappings.append(std::move(mapping));
        endInsertRows();
        return;
    }
    m_mappings[row] = std::move(mapping);
    emit dataChanged(index(row, 0), index(row, 0));
}

bool QmlControllerLearningPendingMappings::remove(const ConfigKey& control) {
    const int row = rowForControl(control);
    if (row < 0) {
        return false;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_mappings.removeAt(row);
    endRemoveRows();
    return true;
}

void QmlControllerLearningPendingMappings::clear() {
    if (m_mappings.isEmpty()) {
        return;
    }
    beginResetModel();
    m_mappings.clear();
    endResetModel();
}

int QmlControllerLearningPendingMappings::rowForControl(
        const ConfigKey& control) const {
    for (int row = 0; row < m_mappings.size(); ++row) {
        if (m_mappings.at(row).control == control) {
            return row;
        }
    }
    return -1;
}

QmlControllerLearningProxy::QmlControllerLearningProxy(
        UserSettingsPointer pConfig, QObject* pParent)
        : QObject(pParent),
          m_pConfig(std::move(pConfig)),
          m_pPendingMappings(new QmlControllerLearningPendingMappings(this)),
          m_pDetectionTimer(new QTimer(this)) {
    qRegisterMetaType<MidiInputMappings>("MidiInputMappings");
    m_pDetectionTimer->setSingleShot(true);
    m_pDetectionTimer->setInterval(kDetectionTimeoutMillis);
    connect(m_pDetectionTimer,
            &QTimer::timeout,
            this,
            &QmlControllerLearningProxy::slotDetectionTimeout);
}

QmlControllerLearningProxy::~QmlControllerLearningProxy() {
    // Do not touch the controller here: it may already have been destroyed
    // during shutdown. Just drop our connection.
    if (m_pMidiController) {
        disconnect(m_pMidiController, nullptr, this, nullptr);
    }
}

// static
QmlControllerLearningProxy* QmlControllerLearningProxy::create(
        QQmlEngine* pQmlEngine, QJSEngine*) {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pConfig) {
        qWarning() << "UserSettings hasn't been registered yet";
        return nullptr;
    }
    return new QmlControllerLearningProxy(pConfig, pQmlEngine);
}

QString QmlControllerLearningProxy::deviceName() const {
    return m_pController ? m_pController->getName() : QString();
}

QString QmlControllerLearningProxy::mappingName() const {
    return m_pWorkingMapping ? m_pWorkingMapping->name() : QString();
}

QString QmlControllerLearningProxy::mappingFilePath() const {
    return m_pWorkingMapping ? m_pWorkingMapping->filePath() : QString();
}

QVariantList QmlControllerLearningProxy::suggestedMappings() const {
    QVariantList result;
    for (const MidiInputMapping& mapping : m_suggestedMappings) {
        QVariantMap description = describeMapping(mapping);
        // Suggestions have no target yet: the UI picks the control.
        description.remove(QStringLiteral("group"));
        description.remove(QStringLiteral("key"));
        description.remove(QStringLiteral("configKey"));
        description.remove(QStringLiteral("controlType"));
        result.append(description);
    }
    return result;
}

QAbstractListModel* QmlControllerLearningProxy::availableControls() const {
    auto* pSelf = const_cast<QmlControllerLearningProxy*>(this);
    if (!m_pControlModel) {
        pSelf->m_pControlModel = ControlPickerModel::createDetached(pSelf);
    }
    return m_pControlModel;
}

bool QmlControllerLearningProxy::start(Controller* pController) {
    VERIFY_OR_DEBUG_ASSERT(QThread::currentThread() == thread()) {
        qWarning() << "Controller learning must run on the main thread";
        return false;
    }
    stop();
    m_pController = nullptr;
    m_pMidiController = nullptr;
    m_pWorkingMapping.reset();
    m_pPendingMappings->clear();
    m_messages.clear();
    m_suggestedMappings.clear();
    m_lastMessage.clear();
    m_lastError.clear();

    if (!pController) {
        setError(tr("No controller selected."));
        return false;
    }
    auto* pMidiController = qobject_cast<MidiController*>(pController);
    if (pMidiController == nullptr) {
        setError(tr("Only MIDI controllers can be mapped from this window."));
        return false;
    }
    m_pController = pController;
    m_pMidiController = pMidiController;

    if (!ensureWorkingMapping()) {
        m_pController = nullptr;
        m_pMidiController = nullptr;
        return false;
    }

    connect(m_pMidiController,
            &MidiController::messageReceived,
            this,
            &QmlControllerLearningProxy::slotMessageReceived);
    // startLearning() is a protected slot so that only the Qt signal system
    // can call it without locks (see Controller::startLearning). Auto means
    // direct in tests and queued to the controller thread in the application.
    QMetaObject::invokeMethod(m_pMidiController, "startLearning", Qt::AutoConnection);
    m_learning = true;

    emit learningChanged();
    emit deviceChanged();
    emit mappingChanged();
    emit lastMessageChanged();
    emit suggestedMappingsChanged();
    emit lastErrorChanged();
    return true;
}

bool QmlControllerLearningProxy::start(QmlControllerDeviceProxy* pDevice) {
    return pDevice ? start(pDevice->internal()) : false;
}

void QmlControllerLearningProxy::stop() {
    if (m_pMidiController) {
        QMetaObject::invokeMethod(m_pMidiController,
                "clearTemporaryInputMappings",
                Qt::AutoConnection);
        QMetaObject::invokeMethod(m_pMidiController, "stopLearning", Qt::AutoConnection);
        disconnect(m_pMidiController, nullptr, this, nullptr);
    }
    if (m_pDetectionTimer) {
        m_pDetectionTimer->stop();
    }
    m_messages.clear();
    if (!m_suggestedMappings.isEmpty()) {
        m_suggestedMappings.clear();
        emit suggestedMappingsChanged();
    }
    if (m_learning) {
        m_learning = false;
        emit learningChanged();
    }
}

void QmlControllerLearningProxy::finishDetection() {
    m_pDetectionTimer->stop();
    recomputeSuggestions();
    updateLastMessage();
    emit detectionFinished();
}

void QmlControllerLearningProxy::slotMessageReceived(
        unsigned char status, unsigned char control, unsigned char value) {
    if (!m_learning) {
        return;
    }
    processMessage(status, control, value);
}

void QmlControllerLearningProxy::processMessage(
        unsigned char status, unsigned char control, unsigned char value) {
    const MidiKey key(status, control);
    if (MidiUtils::isClockSignal(key)) {
        return;
    }
    if (!m_pDetectionTimer->isActive()) {
        // A pause longer than the detection window means a new gesture.
        m_messages.clear();
    }
    m_messages.append(qMakePair(key, value));
    while (m_messages.size() > kMaxBufferedMessages) {
        m_messages.removeFirst();
    }
    m_pDetectionTimer->start();
    recomputeSuggestions();
    updateLastMessage();
}

void QmlControllerLearningProxy::slotDetectionTimeout() {
    recomputeSuggestions();
    updateLastMessage();
    emit detectionFinished();
}

void QmlControllerLearningProxy::recomputeSuggestions() {
    m_suggestedMappings.clear();
    if (!m_messages.isEmpty()) {
        m_suggestedMappings = LearningUtils::guessMidiInputMappings(
                pendingControlKey(), m_messages);
    }
    emit suggestedMappingsChanged();
}

void QmlControllerLearningProxy::updateLastMessage() {
    if (m_messages.isEmpty()) {
        if (!m_lastMessage.isEmpty()) {
            m_lastMessage.clear();
            emit lastMessageChanged();
        }
        return;
    }
    const QPair<MidiKey, unsigned char>& last = m_messages.last();
    const MidiOpCode opCode = MidiUtils::opCodeFromStatus(last.first.status);
    const unsigned char channel = MidiUtils::channelFromStatus(last.first.status);

    QVariantMap message;
    message.insert(QStringLiteral("status"), static_cast<int>(last.first.status));
    message.insert(QStringLiteral("control"), static_cast<int>(last.first.control));
    message.insert(QStringLiteral("value"), static_cast<int>(last.second));
    message.insert(QStringLiteral("channel"), static_cast<int>(channel));
    message.insert(QStringLiteral("opCode"), static_cast<int>(opCode));
    message.insert(QStringLiteral("opCodeName"),
            MidiUtils::opCodeToTranslatedString(opCode));
    message.insert(QStringLiteral("type"),
            m_suggestedMappings.isEmpty()
                    ? QStringLiteral("other")
                    : mappingType(m_suggestedMappings.first()));
    message.insert(QStringLiteral("message"),
            QStringLiteral("%1 %2 %3").arg(hexByte(last.first.status), hexByte(last.first.control), hexByte(last.second)));
    const QVariantMap existing = existingMappingFor(message);
    if (!existing.isEmpty()) {
        message.insert(QStringLiteral("existingMapping"), existing);
    }
    m_lastMessage = message;
    emit lastMessageChanged();
}

bool QmlControllerLearningProxy::ensureWorkingMapping() {
    if (m_pWorkingMapping) {
        return true;
    }
    if (!m_pController) {
        setError(tr("No controller selected."));
        return false;
    }
    if (const auto pExisting = m_pController->getMapping()) {
        m_pWorkingMapping =
                std::dynamic_pointer_cast<LegacyMidiControllerMapping>(pExisting);
        if (!m_pWorkingMapping) {
            setError(tr("The selected device does not use a MIDI mapping."));
            return false;
        }
        return true;
    }
    // Editing the empty mapping starts in this proxy. It is adopted by the
    // controller (and, on save, by ControllerManager) before any message is
    // processed by the controller thread.
    m_pWorkingMapping = std::make_shared<LegacyMidiControllerMapping>();
    applyMappingThroughManager();
    return true;
}

void QmlControllerLearningProxy::applyMappingThroughManager() {
    if (!m_pController || !m_pWorkingMapping) {
        return;
    }
    const auto pManager = QmlControllerManagerProxy::registeredManager();
    if (!pManager || !pManager->thread()->isRunning()) {
        // Tests and headless use: no controller thread, so adopt directly.
        m_pController->setMapping(m_pWorkingMapping);
        return;
    }
    // ControllerManager owns the controller thread. slotApplyMapping must run
    // there and blocks until the mapping has been cloned (same protocol as
    // DlgPrefController).
    const Qt::ConnectionType connectionType =
            pManager->thread() == QThread::currentThread()
            ? Qt::DirectConnection
            : Qt::BlockingQueuedConnection;
    const QMetaObject::Connection connection =
            connect(this,
                    &QmlControllerLearningProxy::applyMappingRequested,
                    pManager.get(),
                    &ControllerManager::slotApplyMapping,
                    connectionType);
    emit applyMappingRequested(m_pController, m_pWorkingMapping, true);
    disconnect(connection);
}

void QmlControllerLearningProxy::pushLiveMappings(
        const MidiInputMappings& mappings) {
    if (!m_pMidiController || mappings.isEmpty()) {
        return;
    }
    // Same mechanism as DlgControllerLearning: the controller thread applies
    // the temporary mapping immediately, then folds it into the live mapping.
    QMetaObject::invokeMethod(m_pMidiController,
            "learnTemporaryInputMappings",
            Qt::AutoConnection,
            Q_ARG(MidiInputMappings, mappings));
    QMetaObject::invokeMethod(m_pMidiController,
            "commitTemporaryInputMappings",
            Qt::AutoConnection);
}

void QmlControllerLearningProxy::removeLiveMappings(
        const MidiInputMappings& mappings) {
    if (!m_pMidiController || mappings.isEmpty()) {
        return;
    }
    const QPointer<MidiController> pMidiController = m_pMidiController;
    QMetaObject::invokeMethod(
            m_pMidiController,
            [pMidiController, mappings]() {
                if (!pMidiController) {
                    return;
                }
                for (const MidiInputMapping& mapping : mappings) {
                    pMidiController->removeInputMapping(mapping.key.key, mapping);
                }
            },
            Qt::AutoConnection);
}

QVariantMap QmlControllerLearningProxy::existingMappingFor(
        const QVariantMap& message) const {
    if (!m_pWorkingMapping) {
        return QVariantMap();
    }
    if (!message.contains(QStringLiteral("status")) ||
            !message.contains(QStringLiteral("control"))) {
        return QVariantMap();
    }
    const int status = message.value(QStringLiteral("status")).toInt();
    const int control = message.value(QStringLiteral("control")).toInt();
    if (status <= 0 || status > 0xFF || control < 0 || control > 0xFF) {
        return QVariantMap();
    }
    const MidiKey key(static_cast<unsigned char>(status),
            static_cast<unsigned char>(control));
    const auto& mappings = m_pWorkingMapping->getInputMappings();
    auto [it, end] = mappings.equal_range(key.key);
    if (it == end) {
        return QVariantMap();
    }
    return describeMapping(it.value());
}

QVariantMap QmlControllerLearningProxy::describeMapping(
        const MidiInputMapping& mapping) const {
    const MidiOpCode opCode = MidiUtils::opCodeFromStatus(mapping.key.status);
    QVariantMap info;
    info.insert(QStringLiteral("status"), static_cast<int>(mapping.key.status));
    info.insert(QStringLiteral("control"), static_cast<int>(mapping.key.control));
    info.insert(QStringLiteral("channel"),
            static_cast<int>(MidiUtils::channelFromStatus(mapping.key.status)));
    info.insert(QStringLiteral("opCode"), static_cast<int>(opCode));
    info.insert(QStringLiteral("opCodeName"),
            MidiUtils::opCodeToTranslatedString(opCode));
    info.insert(QStringLiteral("options"), mapping.options.toInt());
    info.insert(QStringLiteral("optionNames"), midiOptionNames(mapping.options));
    info.insert(QStringLiteral("type"), mappingType(mapping));
    info.insert(QStringLiteral("message"), mappingMessage(mapping));
    info.insert(QStringLiteral("mappingDescription"), mapping.description);

    if (const auto* pControl = std::get_if<ConfigKey>(&mapping.control)) {
        info.insert(QStringLiteral("controlType"), QStringLiteral("config"));
        info.insert(QStringLiteral("group"), pControl->group);
        info.insert(QStringLiteral("key"), pControl->item);
        info.insert(QStringLiteral("configKey"),
                QStringLiteral("%1,%2").arg(pControl->group, pControl->item));
        if (m_pControlModel) {
            const int row = m_pControlModel->rowForConfigKey(
                    pControl->group, pControl->item);
            if (row >= 0) {
                const QVariantMap control = m_pControlModel->get(row);
                info.insert(QStringLiteral("title"),
                        control.value(QStringLiteral("title")));
                info.insert(QStringLiteral("controlDescription"),
                        control.value(QStringLiteral("description")));
            }
        }
    } else {
        info.insert(QStringLiteral("controlType"), QStringLiteral("script"));
    }
    return info;
}

MidiOptions QmlControllerLearningProxy::optionsForMapping(
        const QVariantMap& overrides, MidiOptions guessed) const {
    if (overrides.isEmpty()) {
        return guessed;
    }
    MidiOptions options = guessed;
    if (overrides.contains(QStringLiteral("options"))) {
        options = MidiOptions(overrides.value(QStringLiteral("options")).toInt());
    }
    const auto applyFlag = [&](const QString& name, MidiOption option) {
        if (overrides.contains(name)) {
            options.setFlag(option, overrides.value(name).toBool());
        }
    };
    applyFlag(QStringLiteral("switch"), MidiOption::Switch);
    applyFlag(QStringLiteral("button"), MidiOption::Button);
    applyFlag(QStringLiteral("invert"), MidiOption::Invert);
    applyFlag(QStringLiteral("softTakeover"), MidiOption::SoftTakeover);
    applyFlag(QStringLiteral("selectKnob"), MidiOption::SelectKnob);
    applyFlag(QStringLiteral("spread64"), MidiOption::Spread64);
    applyFlag(QStringLiteral("fourteenBit"), MidiOption::FourteenBitMSB);

    if (overrides.value(QStringLiteral("selectKnob")).toBool()) {
        options.setFlag(MidiOption::Spread64, false);
        options.setFlag(MidiOption::Diff, false);
        options.setFlag(MidiOption::FourteenBitLSB, false);
        options.setFlag(MidiOption::FourteenBitMSB, false);
    }
    if (overrides.value(QStringLiteral("spread64")).toBool()) {
        options.setFlag(MidiOption::SelectKnob, false);
        options.setFlag(MidiOption::Diff, false);
        options.setFlag(MidiOption::FourteenBitLSB, false);
        options.setFlag(MidiOption::FourteenBitMSB, false);
    }
    if (overrides.value(QStringLiteral("fourteenBit")).toBool()) {
        options.setFlag(MidiOption::SelectKnob, false);
        options.setFlag(MidiOption::Spread64, false);
        options.setFlag(MidiOption::Diff, false);
    }
    return options;
}

QString QmlControllerLearningProxy::pendingControlTitle(const ConfigKey& key) const {
    if (m_pControlModel) {
        const int row = m_pControlModel->rowForConfigKey(key.group, key.item);
        if (row >= 0) {
            return m_pControlModel->get(row).value(QStringLiteral("title")).toString();
        }
    }
    return QString();
}

QString QmlControllerLearningProxy::pendingControlDescription(
        const ConfigKey& key) const {
    if (m_pControlModel) {
        const int row = m_pControlModel->rowForConfigKey(key.group, key.item);
        if (row >= 0) {
            return m_pControlModel->get(row)
                    .value(QStringLiteral("description"))
                    .toString();
        }
    }
    return QString();
}

bool QmlControllerLearningProxy::assign(const QString& group, const QString& key) {
    return assign(group, key, QVariantMap());
}

bool QmlControllerLearningProxy::assign(const QString& group,
        const QString& key,
        const QVariantMap& options) {
    if (!ensureWorkingMapping()) {
        return false;
    }
    const ConfigKey control(group, key);
    if (!control.isValid()) {
        setError(tr("Cannot assign a message to an invalid control."));
        return false;
    }
    if (m_suggestedMappings.isEmpty()) {
        setError(tr("Move a control on the controller first."));
        return false;
    }

    MidiInputMappings learned;
    for (const MidiInputMapping& suggestion : std::as_const(m_suggestedMappings)) {
        MidiInputMapping mapping = suggestion;
        mapping.control = control;
        mapping.options = optionsForMapping(options, suggestion.options);
        learned.append(mapping);
    }

    // Reassigning a physical control replaces whatever it was mapped to.
    for (const MidiInputMapping& mapping : std::as_const(learned)) {
        m_pWorkingMapping->removeInputMapping(mapping.key.key);
    }
    for (const MidiInputMapping& mapping : std::as_const(learned)) {
        m_pWorkingMapping->addInputMapping(mapping.key.key, mapping);
    }
    pushLiveMappings(learned);

    QmlControllerLearningMapping pending;
    pending.control = control;
    pending.title = pendingControlTitle(control);
    pending.description = pendingControlDescription(control);
    pending.mappings = learned;
    m_pPendingMappings->upsert(pending);
    prunePendingMappings();

    m_lastError.clear();
    emit lastErrorChanged();
    emit mappingChanged();
    // The active mapping of the last message may have been replaced.
    updateLastMessage();
    return true;
}

bool QmlControllerLearningProxy::assignConfigKey(const QString& configKey) {
    return assignConfigKey(configKey, QVariantMap());
}

bool QmlControllerLearningProxy::assignConfigKey(const QString& configKey,
        const QVariantMap& options) {
    const ConfigKey control = ConfigKey::parseCommaSeparated(configKey);
    if (!control.isValid()) {
        setError(tr("Cannot assign a message to an invalid control."));
        return false;
    }
    return assign(control.group, control.item, options);
}

void QmlControllerLearningProxy::prunePendingMappings() {
    if (!m_pWorkingMapping) {
        return;
    }
    for (int row = m_pPendingMappings->rowCount() - 1; row >= 0; --row) {
        const QmlControllerLearningMapping& entry =
                m_pPendingMappings->mappings().at(row);
        bool allPresent = true;
        for (const MidiInputMapping& mapping : entry.mappings) {
            bool found = false;
            const auto& inputMappings = m_pWorkingMapping->getInputMappings();
            auto [it, end] = inputMappings.equal_range(mapping.key.key);
            for (; it != end; ++it) {
                if (it.value() == mapping) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                allPresent = false;
                break;
            }
        }
        if (!allPresent) {
            const ConfigKey control = entry.control;
            m_pPendingMappings->remove(control);
        }
    }
}

bool QmlControllerLearningProxy::remove(const QString& group, const QString& key) {
    if (!ensureWorkingMapping()) {
        return false;
    }
    const ConfigKey control(group, key);
    MidiInputMappings removed;
    const auto& inputMappings = m_pWorkingMapping->getInputMappings();
    for (auto it = inputMappings.constBegin(); it != inputMappings.constEnd(); ++it) {
        const auto* pControl = std::get_if<ConfigKey>(&it.value().control);
        if (pControl && *pControl == control) {
            removed.append(it.value());
        }
    }
    for (const MidiInputMapping& mapping : std::as_const(removed)) {
        m_pWorkingMapping->removeInputMapping(mapping.key.key, mapping);
    }
    removeLiveMappings(removed);
    m_pPendingMappings->remove(control);
    emit mappingChanged();
    return !removed.isEmpty();
}

bool QmlControllerLearningProxy::removeConfigKey(const QString& configKey) {
    const ConfigKey control = ConfigKey::parseCommaSeparated(configKey);
    if (!control.isValid()) {
        return false;
    }
    return remove(control.group, control.item);
}

bool QmlControllerLearningProxy::removeAt(int row) {
    if (row < 0 || row >= m_pPendingMappings->rowCount()) {
        return false;
    }
    const ConfigKey control = m_pPendingMappings->mappings().at(row).control;
    return remove(control.group, control.item);
}

void QmlControllerLearningProxy::clearPendingMappings() {
    while (m_pPendingMappings->rowCount() > 0) {
        const int row = m_pPendingMappings->rowCount() - 1;
        if (!removeAt(row)) {
            // Nothing left in the working mapping, drop the stale row.
            m_pPendingMappings->remove(
                    m_pPendingMappings->mappings().at(row).control);
        }
    }
}

QString QmlControllerLearningProxy::save() {
    return save(QString());
}

QString QmlControllerLearningProxy::save(const QString& mappingName) {
    if (!ensureWorkingMapping()) {
        return QString();
    }

    QString name = mappingName.trimmed();
    if (name.isEmpty()) {
        name = m_pWorkingMapping->name().trimmed();
    }
    if (name.isEmpty() && m_pController) {
        name = m_pController->getName().trimmed();
    }
    if (name.isEmpty()) {
        name = tr("Controller Mapping");
    }

    const QString userDir = userMappingsPath(m_pConfig);
    if (!QDir(userDir).exists() && !QDir().mkpath(userDir)) {
        setError(tr("Could not create the user mapping directory."));
        return QString();
    }

    QString filePath;
    const QString existingFilePath = m_pWorkingMapping->filePath();
    const bool existingIsUserMapping = !existingFilePath.isEmpty() &&
            QFileInfo(existingFilePath)
                            .absoluteDir()
                            .absolutePath()
                            .append(QChar('/')) == userDir;
    if (existingIsUserMapping) {
        filePath = existingFilePath;
    } else {
        // Bundled mappings are never touched: write a user copy instead.
        QString fileName = QString(name).replace(QChar('/'), QChar('-'));
        filePath = userDir + fileName + QStringLiteral(".midi.xml");
    }

    m_pWorkingMapping->setName(name);
    if (!m_pWorkingMapping->saveMapping(filePath)) {
        setError(tr("Failed to save the mapping to %1.").arg(filePath));
        return QString();
    }
    m_pWorkingMapping->setFilePath(filePath);
    m_pWorkingMapping->setDirty(false);
    // Stop listening before ControllerManager closes and reopens the device.
    stop();
    applyMappingThroughManager();

    m_lastError.clear();
    emit lastErrorChanged();
    emit mappingChanged();
    emit saved(filePath);
    return filePath;
}

void QmlControllerLearningProxy::setError(const QString& message) {
    m_lastError = message;
    emit lastErrorChanged();
    emit error(message);
}

} // namespace qml
} // namespace mixxx
