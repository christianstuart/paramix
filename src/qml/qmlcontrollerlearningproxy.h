#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "controllers/midi/midimessage.h"
#include "preferences/usersettings.h"
#include "qml/qmlpreferencesproxy.h"

class ControlPickerModel;
class Controller;
class LegacyMidiControllerMapping;
class MidiController;
class QTimer;

namespace mixxx {
namespace qml {

class QmlControllerDeviceProxy;

/// One control learned in the current session. A single physical message can
/// expand to multiple MIDI mappings (a button sends Note On and Note Off, a
/// 14-bit knob sends MSB and LSB), so the mappings are kept together.
struct QmlControllerLearningMapping {
    ConfigKey control;
    QString title;
    QString description;
    MidiInputMappings mappings;
};

/// List model of the mappings learned in the current session, one row per
/// Paramix control. This is the backing model of the QML "Mapped controls" list.
class QmlControllerLearningPendingMappings : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Roles {
        GroupRole = Qt::UserRole + 1,
        KeyRole,
        ConfigKeyRole,
        TitleRole,
        DescriptionRole,
        StatusRole,
        ControlRole,
        ChannelRole,
        OpCodeRole,
        OptionsRole,
        OptionNamesRole,
        MappingCountRole,
        MessageRole,
    };
    Q_ENUM(Roles)

    explicit QmlControllerLearningPendingMappings(QObject* pParent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int row) const;

    void upsert(QmlControllerLearningMapping mapping);
    bool remove(const ConfigKey& control);
    void clear();
    int rowForControl(const ConfigKey& control) const;
    const QList<QmlControllerLearningMapping>& mappings() const {
        return m_mappings;
    }

  private:
    QList<QmlControllerLearningMapping> m_mappings;
};

/// Backend of the controller-based MIDI mapping UI (F10).
///
/// Hardware-first learning: the user moves a physical control, this proxy
/// reports what arrived and the mappings it suggests, and QML assigns the
/// message to a Paramix control. It mirrors the classic learning wizard
/// (DlgControllerLearning) and reuses its detection and file format, but is
/// driven entirely from the main thread and exposes no QtWidgets.
class QmlControllerLearningProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool learning READ isLearning NOTIFY learningChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceChanged)
    Q_PROPERTY(QString mappingName READ mappingName NOTIFY mappingChanged)
    Q_PROPERTY(QString mappingFilePath READ mappingFilePath NOTIFY mappingChanged)
    Q_PROPERTY(QVariantMap lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(QVariantList suggestedMappings READ suggestedMappings NOTIFY
                    suggestedMappingsChanged)
    Q_PROPERTY(int suggestedMappingCount READ suggestedMappingCount NOTIFY
                    suggestedMappingsChanged)
    Q_PROPERTY(QAbstractListModel* pendingMappings READ pendingMappings CONSTANT)
    Q_PROPERTY(QAbstractListModel* availableControls READ availableControls CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    QML_NAMED_ELEMENT(ControllerLearning)
    QML_SINGLETON

  public:
    explicit QmlControllerLearningProxy(
            UserSettingsPointer pConfig, QObject* pParent = nullptr);
    ~QmlControllerLearningProxy() override;

    static QmlControllerLearningProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool isLearning() const {
        return m_learning;
    }
    QString deviceName() const;
    QString mappingName() const;
    QString mappingFilePath() const;
    QVariantMap lastMessage() const {
        return m_lastMessage;
    }
    QVariantList suggestedMappings() const;
    int suggestedMappingCount() const {
        return m_suggestedMappings.size();
    }
    QAbstractListModel* pendingMappings() const {
        return m_pPendingMappings;
    }
    QAbstractListModel* availableControls() const;
    QString lastError() const {
        return m_lastError;
    }

    /// Starts hardware-first learning on a MIDI device. The device's current
    /// mapping is edited as a mutable copy. Assignments reach the controller
    /// thread through the same temporary mappings the classic wizard uses, so
    /// learned controls respond immediately.
    bool start(Controller* pController);
    Q_INVOKABLE bool start(QmlControllerDeviceProxy* pDevice);
    Q_INVOKABLE void stop();

    /// Detects the mapping for the messages collected so far. The detection
    /// timer calls this automatically; tests and UI can force it.
    Q_INVOKABLE void finishDetection();

    Q_INVOKABLE QVariantMap existingMappingFor(const QVariantMap& message) const;

    /// Assigns the currently suggested message(s) to a Paramix control.
    /// @param options optional overrides: switch, button, invert, softTakeover,
    ///        selectKnob, spread64 (booleans) or options (raw bit mask).
    Q_INVOKABLE bool assign(const QString& group, const QString& key);
    Q_INVOKABLE bool assign(const QString& group,
            const QString& key,
            const QVariantMap& options);
    /// Convenience overload taking a "group,key" ConfigKey string.
    Q_INVOKABLE bool assignConfigKey(const QString& configKey);
    Q_INVOKABLE bool assignConfigKey(const QString& configKey,
            const QVariantMap& options);

    /// Removes every mapping of a control learned in this session.
    Q_INVOKABLE bool remove(const QString& group, const QString& key);
    Q_INVOKABLE bool removeConfigKey(const QString& configKey);
    Q_INVOKABLE bool removeAt(int row);
    Q_INVOKABLE void clearPendingMappings();

    /// Writes the working mapping to the user mapping directory and adopts it.
    /// Bundled mappings are never modified: they are copied to a new user file.
    /// @return the written file path, or an empty string on failure.
    Q_INVOKABLE QString save();
    Q_INVOKABLE QString save(const QString& mappingName);

  signals:
    void learningChanged();
    void deviceChanged();
    void mappingChanged();
    void lastMessageChanged();
    void suggestedMappingsChanged();
    void lastErrorChanged();
    void detectionFinished();
    void saved(const QString& mappingFile);
    void error(const QString& message);
    // Adopted by ControllerManager::slotApplyMapping on its own thread.
    void applyMappingRequested(Controller* pController,
            std::shared_ptr<LegacyControllerMapping> pMapping,
            bool enabled);

  private slots:
    void slotMessageReceived(unsigned char status, unsigned char control, unsigned char value);
    void slotDetectionTimeout();

  private:
    void setError(const QString& message);
    void processMessage(unsigned char status, unsigned char control, unsigned char value);
    void recomputeSuggestions();
    void updateLastMessage();
    MidiOptions optionsForMapping(
            const QVariantMap& overrides, MidiOptions guessed) const;
    QVariantMap describeMapping(const MidiInputMapping& mapping) const;
    bool ensureWorkingMapping();
    void pushLiveMappings(const MidiInputMappings& mappings);
    void removeLiveMappings(const MidiInputMappings& mappings);
    void applyMappingThroughManager();
    void prunePendingMappings();
    QString pendingControlTitle(const ConfigKey& key) const;
    QString pendingControlDescription(const ConfigKey& key) const;

    UserSettingsPointer m_pConfig;
    Controller* m_pController = nullptr;
    MidiController* m_pMidiController = nullptr;
    std::shared_ptr<LegacyMidiControllerMapping> m_pWorkingMapping;
    QmlControllerLearningPendingMappings* m_pPendingMappings = nullptr;
    ControlPickerModel* m_pControlModel = nullptr;
    QTimer* m_pDetectionTimer = nullptr;

    bool m_learning = false;
    QList<QPair<MidiKey, unsigned char>> m_messages;
    MidiInputMappings m_suggestedMappings;
    QVariantMap m_lastMessage;
    QString m_lastError;
};

} // namespace qml
} // namespace mixxx
