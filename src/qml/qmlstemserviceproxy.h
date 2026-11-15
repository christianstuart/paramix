#pragma once

#include <QHash>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QString>

class QTimer;

#include "qml/qmltrackproxy.h"

namespace mixxx {
namespace stems {
class StemService;
} // namespace stems
namespace qml {

/// Singleton that exposes the stem separation queue of `StemService` to QML.
///
/// All state is maintained from the service signals, which are received on the
/// GUI thread through a queued connection. Getters return safe defaults while
/// `StemService` or `Library` have not been created yet (e.g. in unit tests).
class QmlStemServiceProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(bool hasModel READ hasModel NOTIFY hasModelChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY queueChanged)
    Q_PROPERTY(int queuedCount READ queuedCount NOTIFY queueChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY queueChanged)
    // Percent, 0..100 of the currently running job (0 when idle)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentTrackTitle READ currentTrackTitle NOTIFY progressChanged)
    Q_PROPERTY(qint64 currentTrackId READ currentTrackId NOTIFY progressChanged)
    QML_NAMED_ELEMENT(StemService)
    QML_SINGLETON

  public:
    /// Mirrors `mixxx::stems::StemService::Status`.
    enum Status {
        NotSeparated = 0,
        Queued = 1,
        Separating = 2,
        Ready = 3,
        MissingStemFile = 4,
        Failed = 5,
    };
    Q_ENUM(Status)

    explicit QmlStemServiceProxy(QObject* parent = nullptr);
    ~QmlStemServiceProxy() override = default;

    static QmlStemServiceProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    bool isAvailable() const;
    bool hasModel() const;
    bool isBusy() const {
        return activeCount() > 0;
    }
    int queuedCount() const {
        return static_cast<int>(m_queuedTracks.size());
    }
    int activeCount() const {
        return static_cast<int>(m_queuedTracks.size() + m_separatingTracks.size());
    }
    int progress() const {
        return m_progress;
    }
    QString currentTrackTitle() const {
        return m_currentTrackTitle;
    }
    qint64 currentTrackId() const {
        return m_currentTrackId;
    }

    Q_INVOKABLE bool generateStems(QmlTrackProxy* track);
    Q_INVOKABLE void removeStems(QmlTrackProxy* track);
    /// Returns -1 for an invalid track or while `StemService` does not exist.
    Q_INVOKABLE int statusForTrack(QmlTrackProxy* track);
    Q_INVOKABLE QString messageForTrack(QmlTrackProxy* track);
    Q_INVOKABLE bool hasStems(QmlTrackProxy* track);
    Q_INVOKABLE void cancelAll();

  signals:
    void trackStatusChanged(qint64 trackId, int status, int progress, QString message);
    void stemsReady(qint64 trackId);
    void hasModelChanged();
    void queueChanged();
    void progressChanged();

  private:
    void attachToService();
    /// The singleton is constructed while the status strip binds to it, which
    /// can happen before CoreServices creates the StemService. Retry until the
    /// service exists so progress/queue updates are never lost.
    void scheduleAttachRetry();
    /// Re-evaluates the cached value of `hasModel` and notifies QML on change.
    void refreshHasModel();
    bool computeHasModel() const;
    QString trackTitleForTrack(qint64 trackId) const;
    void handleTrackStatusChanged(qint64 trackId, int status, int progress, const QString& message);
    void handleStemsReady(qint64 trackId);

    mixxx::stems::StemService* m_pService = nullptr;
    QTimer* m_pAttachTimer = nullptr;

    QSet<qint64> m_queuedTracks;
    QSet<qint64> m_separatingTracks;
    /// Titles remembered from `generateStems()` as a fallback when the track
    /// cannot be resolved through the library any more.
    QHash<qint64, QString> m_titles;

    qint64 m_currentTrackId = -1;
    QString m_currentTrackTitle;
    int m_progress = 0;
    /// Last value reported through `hasModelChanged()`; `hasModel()` itself is
    /// evaluated live so a stale cache can never hide a configured model.
    bool m_hasModel = false;
};

} // namespace qml
} // namespace mixxx
