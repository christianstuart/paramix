#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "library/analysis/analysisfeature.h"
#include "stems/stemservice.h"

namespace mixxx::stems {

/// Keeps the two background CPU consumers from competing with each other and
/// with the audio engine:
///  - batch analysis is suspended while a stem job is queued or separating;
///  - stem separation is paused while the audio callback is overloaded.
///
/// All connections are delivered on the GUI thread. Pauses owned by this
/// coordinator compose with user-initiated pauses in both services, so this
/// class can never clear a pause that the user requested.
class StemAnalysisCoordinator final : public QObject {
    Q_OBJECT

  public:
    StemAnalysisCoordinator(StemService* pStemService,
            AnalysisFeature* pAnalysisFeature,
            QObject* pParent = nullptr);
    ~StemAnalysisCoordinator() override = default;

  private slots:
    void onStemBusyChanged(bool busy);
    void onAudioLatencyTick();

  private:
    QPointer<StemService> m_pStemService;
    QPointer<AnalysisFeature> m_pAnalysisFeature;

    QTimer m_audioLatencyTimer;
    int m_highUsageTicks = 0;
    int m_lowUsageTicks = 0;
    // True only while this coordinator owns the stem pause.
    bool m_pausedForOverload = false;
};

} // namespace mixxx::stems
