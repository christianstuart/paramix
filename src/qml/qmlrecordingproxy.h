#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <memory>

class RecordingManager;

namespace mixxx {
namespace qml {

class QmlRecordingProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString durationText READ durationText NOTIFY durationTextChanged)
    Q_PROPERTY(QString fileSizeText READ fileSizeText NOTIFY fileSizeTextChanged)
    QML_NAMED_ELEMENT(Recording)
    QML_SINGLETON
  public:
    explicit QmlRecordingProxy(std::shared_ptr<RecordingManager> pRecordingManager,
            QObject* parent = nullptr);
    ~QmlRecordingProxy() override = default;

    static QmlRecordingProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    const QString& durationText() const {
        return m_durationText;
    }
    const QString& fileSizeText() const {
        return m_fileSizeText;
    }

    static inline std::shared_ptr<RecordingManager> s_pRecordingManager;

  signals:
    void durationTextChanged();
    void fileSizeTextChanged();

  private slots:
    void slotBytesRecorded(int bytes);
    void slotDurationRecorded(const QString& durationText);
    void slotRecordingActiveChanged(bool isRecording);

  private:
    std::shared_ptr<RecordingManager> m_pRecordingManager;
    QString m_durationText;
    QString m_fileSizeText;
};

} // namespace qml
} // namespace mixxx
