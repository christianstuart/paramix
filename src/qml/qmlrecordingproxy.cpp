#include "qml/qmlrecordingproxy.h"

#include <utility>

#include "moc_qmlrecordingproxy.cpp"
#include "recording/recordingmanager.h"

namespace mixxx {
namespace qml {

namespace {
const QString kInactiveDurationText = QStringLiteral("--:--");
const QString kInactiveFileSizeText = QStringLiteral("--");

QString formatFileSize(int bytes) {
    constexpr double kMebibyte = 1024.0 * 1024.0;
    if (bytes <= 0) {
        return kInactiveFileSizeText;
    }
    const double mebibytes = bytes / kMebibyte;
    if (mebibytes >= 1024.0) {
        return QStringLiteral("%1 GB").arg(mebibytes / 1024.0, 0, 'f', 2);
    }
    if (mebibytes >= 10.0) {
        return QStringLiteral("%1 MB").arg(mebibytes, 0, 'f', 1);
    }
    return QStringLiteral("%1 MB").arg(mebibytes, 0, 'f', 2);
}
} // namespace

QmlRecordingProxy::QmlRecordingProxy(
        std::shared_ptr<RecordingManager> pRecordingManager,
        QObject* parent)
        : QObject(parent),
          m_pRecordingManager(std::move(pRecordingManager)),
          m_durationText(kInactiveDurationText),
          m_fileSizeText(kInactiveFileSizeText) {
    if (!m_pRecordingManager) {
        return;
    }

    connect(m_pRecordingManager.get(),
            &RecordingManager::durationRecorded,
            this,
            &QmlRecordingProxy::slotDurationRecorded);
    connect(m_pRecordingManager.get(),
            &RecordingManager::bytesRecorded,
            this,
            &QmlRecordingProxy::slotBytesRecorded);
    connect(m_pRecordingManager.get(),
            &RecordingManager::isRecording,
            this,
            &QmlRecordingProxy::slotRecordingActiveChanged);

    if (!m_pRecordingManager->isRecordingActive()) {
        slotRecordingActiveChanged(false);
    }
}

QmlRecordingProxy* QmlRecordingProxy::create(
        QQmlEngine* pQmlEngine,
        [[maybe_unused]] QJSEngine* pJsEngine) {
    return new QmlRecordingProxy(s_pRecordingManager, pQmlEngine);
}

void QmlRecordingProxy::slotBytesRecorded(int bytes) {
    const QString fileSizeText = formatFileSize(bytes);
    if (m_fileSizeText == fileSizeText) {
        return;
    }
    m_fileSizeText = fileSizeText;
    emit fileSizeTextChanged();
}

void QmlRecordingProxy::slotDurationRecorded(const QString& durationText) {
    if (m_durationText == durationText) {
        return;
    }
    m_durationText = durationText;
    emit durationTextChanged();
}

void QmlRecordingProxy::slotRecordingActiveChanged(bool isRecording) {
    if (isRecording) {
        return;
    }
    if (m_durationText != kInactiveDurationText) {
        m_durationText = kInactiveDurationText;
        emit durationTextChanged();
    }
    if (m_fileSizeText != kInactiveFileSizeText) {
        m_fileSizeText = kInactiveFileSizeText;
        emit fileSizeTextChanged();
    }
}

} // namespace qml
} // namespace mixxx
