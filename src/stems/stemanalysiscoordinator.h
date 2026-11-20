#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "library/analysis/analysisfeature.h"
#include "stems/stemservice.h"

class PlayerManager;

namespace mixxx::stems {

/// Keeps the background CPU consumers from competing with each other and with
/// the audio engine:
///  - batch analysis is suspended while a stem job is queued or separating;
///  - stem separation, load analysis, batch analysis and stem-detail builds are
///    suspended while the audio callback is overloaded.
///
/// All connections are delivered on the GUI thread. Pauses owned by this
/// coordinator compose with user-initiated pauses in the services it drives, so
/// this class can never clear a pause that the user requested. Every owner is
/// changed only on a state transition, so a paused job is kept and resumed
/// exactly once.
class StemAnalysisCoordinator final : public QObject {
    Q_OBJECT

  public:
    StemAnalysisCoordinator(StemService* pStemService,
            AnalysisFeature* pAnalysisFeature,
            PlayerManager* pPlayerManager,
            QObject* pParent = nullptr);
    ~StemAnalysisCoordinator() override = default;

  private slots:
    void onStemBusyChanged(bool busy);
    void onAudioLatencyTick();

  private:
    /// Applies the overload pause to every background owner. Idempotent.
    void applyBackgroundWorkPause(bool paused);
    /// Recomputes the batch-analysis suspension from its two owners.
    void updateBatchAnalysisPause();
    /// Emits at most one dropout or starvation line per second. Never runs on
    /// the audio callback.
    void logDiagnosticsIfNeeded(double usage);

    QPointer<StemService> m_pStemService;
    QPointer<AnalysisFeature> m_pAnalysisFeature;
    // Owned by CoreServices, which destroys this coordinator before the
    // PlayerManager. Raw pointer: PlayerManager is not a singleton.
    PlayerManager* m_pPlayerManager;

    QTimer m_audioLatencyTimer;
    int m_highUsageTicks = 0;
    int m_lowUsageTicks = 0;
    bool m_stemBusy = false;
    // True only while this coordinator owns the overload pause.
    bool m_pausedForOverload = false;

    // Dropout diagnostics. The underflow code and the cache-miss counter are
    // written on the audio callback and read here on the GUI thread.
    int m_lastSeenOverloadCount = 0;
    quint64 m_lastSampledCacheMissCount = 0;
    quint64 m_lastLoggedCacheMissCount = 0;
    QElapsedTimer m_timeSinceDropoutLog;
};

} // namespace mixxx::stems
