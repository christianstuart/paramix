#include "stems/stemanalysiscoordinator.h"

#include "control/controlobject.h"
#include "moc_stemanalysiscoordinator.cpp"
#include "preferences/configobject.h"
#include "util/logger.h"

namespace mixxx::stems {
namespace {

const mixxx::Logger kLogger("StemAnalysisCoordinator");

const ConfigKey kAudioLatencyUsageKey = ConfigKey(
        QStringLiteral("[App]"), QStringLiteral("audio_latency_usage"));

constexpr int kAudioLatencyCheckIntervalMs = 500;
constexpr double kPauseAboveUsage = 0.85;
constexpr double kResumeBelowUsage = 0.60;
constexpr int kHighUsageTicksToPause = 3;
constexpr int kLowUsageTicksToResume = 2;

} // namespace

StemAnalysisCoordinator::StemAnalysisCoordinator(
        StemService* pStemService,
        AnalysisFeature* pAnalysisFeature,
        QObject* pParent)
        : QObject(pParent),
          m_pStemService(pStemService),
          m_pAnalysisFeature(pAnalysisFeature) {
    if (m_pStemService) {
        connect(m_pStemService,
                &StemService::busyChanged,
                this,
                &StemAnalysisCoordinator::onStemBusyChanged,
                Qt::QueuedConnection);
        // The service may already have restored queued jobs before this
        // coordinator was constructed.
        onStemBusyChanged(m_pStemService->hasPendingWork());

        m_audioLatencyTimer.setInterval(kAudioLatencyCheckIntervalMs);
        m_audioLatencyTimer.setTimerType(Qt::CoarseTimer);
        connect(&m_audioLatencyTimer,
                &QTimer::timeout,
                this,
                &StemAnalysisCoordinator::onAudioLatencyTick);
        m_audioLatencyTimer.start();
    }
}

void StemAnalysisCoordinator::onStemBusyChanged(bool busy) {
    if (m_pAnalysisFeature) {
        m_pAnalysisFeature->setBackgroundSuspended(busy);
    }
}

void StemAnalysisCoordinator::onAudioLatencyTick() {
    if (!m_pStemService) {
        return;
    }

    if (!m_pStemService->hasPendingWork()) {
        m_highUsageTicks = 0;
        m_lowUsageTicks = 0;
        if (m_pausedForOverload) {
            m_pStemService->setBackgroundPaused(false);
            m_pausedForOverload = false;
            kLogger.info()
                    << "Stem separation pause released: no stem job is running";
        }
        return;
    }

    const double usage = ControlObject::get(kAudioLatencyUsageKey);

    if (usage > kPauseAboveUsage) {
        m_lowUsageTicks = 0;
        if (!m_pausedForOverload &&
                ++m_highUsageTicks >= kHighUsageTicksToPause) {
            m_pStemService->setBackgroundPaused(true);
            m_pausedForOverload = true;
            m_highUsageTicks = 0;
            kLogger.info()
                    << "Pausing stem separation: audio latency usage"
                    << usage
                    << "stayed above"
                    << kPauseAboveUsage
                    << "for"
                    << kHighUsageTicksToPause
                    << "checks";
        }
    } else if (usage < kResumeBelowUsage) {
        m_highUsageTicks = 0;
        if (m_pausedForOverload &&
                ++m_lowUsageTicks >= kLowUsageTicksToResume) {
            m_pStemService->setBackgroundPaused(false);
            m_pausedForOverload = false;
            m_lowUsageTicks = 0;
            kLogger.info()
                    << "Resuming stem separation: audio latency usage"
                    << usage
                    << "dropped below"
                    << kResumeBelowUsage
                    << "for"
                    << kLowUsageTicksToResume
                    << "checks";
        }
    } else {
        // In the hysteresis band: require fresh consecutive samples.
        m_highUsageTicks = 0;
        m_lowUsageTicks = 0;
    }
}

} // namespace mixxx::stems
