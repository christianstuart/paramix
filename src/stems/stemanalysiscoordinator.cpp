#include "stems/stemanalysiscoordinator.h"

#include "control/controlobject.h"
#include "engine/cachingreader/cachingreader.h"
#include "mixer/playermanager.h"
#include "moc_stemanalysiscoordinator.cpp"
#include "preferences/configobject.h"
#include "soundio/audiodiagnostics.h"
#include "util/counter.h"
#include "util/logger.h"
#ifdef __STEM__
#include "waveform/stemdetail/stemdetailcache.h"
#include "waveform/waveformwidgetfactory.h"
#endif

namespace mixxx::stems {
namespace {

const mixxx::Logger kLogger("StemAnalysisCoordinator");

const ConfigKey kAudioLatencyUsageKey = ConfigKey(
        QStringLiteral("[App]"), QStringLiteral("audio_latency_usage"));
const ConfigKey kAudioLatencyOverloadCountKey = ConfigKey(
        QStringLiteral("[App]"), QStringLiteral("audio_latency_overload_count"));

constexpr int kAudioLatencyCheckIntervalMs = 500;
constexpr double kPauseAboveUsage = 0.85;
constexpr double kResumeBelowUsage = 0.60;
constexpr int kHighUsageTicksToPause = 3;
constexpr int kLowUsageTicksToResume = 2;
// One dropout line per second at most, even if the callback keeps underflowing.
constexpr int kDropoutLogIntervalMs = 1000;
// Reader starvation is normally one miss per 8192-frame chunk. A seek can miss
// a chunk or two; several misses in one 500 ms tick means a deck is repeatedly
// asking for audio that the worker has not delivered yet. This is a heuristic,
// not a measurement of silence.
constexpr quint64 kStarvationMissesPerTick = 4;

const char* activityLabel(bool active, bool suspended) {
    if (suspended) {
        return "suspended";
    }
    return active ? "active" : "idle";
}

} // namespace

StemAnalysisCoordinator::StemAnalysisCoordinator(
        StemService* pStemService,
        AnalysisFeature* pAnalysisFeature,
        PlayerManager* pPlayerManager,
        QObject* pParent)
        : QObject(pParent),
          m_pStemService(pStemService),
          m_pAnalysisFeature(pAnalysisFeature),
          m_pPlayerManager(pPlayerManager),
          m_lastSeenOverloadCount(static_cast<int>(
                  ControlObject::get(kAudioLatencyOverloadCountKey))),
          m_lastSampledCacheMissCount(CachingReader::cacheMissCount()),
          m_lastLoggedCacheMissCount(CachingReader::cacheMissCount()) {
    if (m_pStemService) {
        connect(m_pStemService,
                &StemService::busyChanged,
                this,
                &StemAnalysisCoordinator::onStemBusyChanged,
                Qt::QueuedConnection);
        // The service may already have restored queued jobs before this
        // coordinator was constructed.
        onStemBusyChanged(m_pStemService->hasPendingWork());
    }

    // The diagnostics line is useful even when no stem job has ever run, so the
    // timer does not depend on the stem service.
    m_audioLatencyTimer.setInterval(kAudioLatencyCheckIntervalMs);
    m_audioLatencyTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_audioLatencyTimer,
            &QTimer::timeout,
            this,
            &StemAnalysisCoordinator::onAudioLatencyTick);
    m_audioLatencyTimer.start();
}

void StemAnalysisCoordinator::onStemBusyChanged(bool busy) {
    m_stemBusy = busy;
    updateBatchAnalysisPause();
}

void StemAnalysisCoordinator::updateBatchAnalysisPause() {
    if (m_pAnalysisFeature) {
        m_pAnalysisFeature->setBackgroundSuspended(
                m_stemBusy || m_pausedForOverload);
    }
}

void StemAnalysisCoordinator::applyBackgroundWorkPause(bool paused) {
    if (m_pStemService) {
        m_pStemService->setBackgroundPaused(paused);
    }
    if (m_pPlayerManager) {
        m_pPlayerManager->setBackgroundAnalysisSuspended(paused);
    }
#ifdef __STEM__
    if (WaveformWidgetFactory::isCreated()) {
        StemDetailCache* pCache =
                WaveformWidgetFactory::instance()->stemDetailCache();
        if (pCache) {
            pCache->setBackgroundSuspended(paused);
        }
    }
#endif
    updateBatchAnalysisPause();
}

void StemAnalysisCoordinator::onAudioLatencyTick() {
    const double usage = ControlObject::get(kAudioLatencyUsageKey);

    logDiagnosticsIfNeeded(usage);

    // Hysteresis: pause after 1.5 s above 0.85, resume after 1 s below 0.60.
    // Usage inside the band neither arms nor releases a transition, so a
    // bouncing value cannot flap the background work.
    if (usage > kPauseAboveUsage) {
        m_lowUsageTicks = 0;
        if (!m_pausedForOverload &&
                ++m_highUsageTicks >= kHighUsageTicksToPause) {
            m_highUsageTicks = 0;
            m_pausedForOverload = true;
            applyBackgroundWorkPause(true);
            kLogger.info()
                    << "Pausing background work: audio latency usage"
                    << usage
                    << "stayed above"
                    << kPauseAboveUsage
                    << "for"
                    << kHighUsageTicksToPause
                    << "checks (stem separation, load analysis, batch analysis, stem-detail builds)";
        }
    } else if (usage < kResumeBelowUsage) {
        m_highUsageTicks = 0;
        if (m_pausedForOverload &&
                ++m_lowUsageTicks >= kLowUsageTicksToResume) {
            m_lowUsageTicks = 0;
            m_pausedForOverload = false;
            applyBackgroundWorkPause(false);
            kLogger.info()
                    << "Resuming background work: audio latency usage"
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

void StemAnalysisCoordinator::logDiagnosticsIfNeeded(double usage) {
    const quint64 cacheMisses = CachingReader::cacheMissCount();
    const quint64 cacheMissesThisTick = cacheMisses >= m_lastSampledCacheMissCount
            ? cacheMisses - m_lastSampledCacheMissCount
            : 0;
    m_lastSampledCacheMissCount = cacheMisses;

    // Feed the developer stat from this non-real-time tick. The CachingReader
    // used to increment it on the audio callback, which allocates a QString and
    // a StatReport exactly when the reader is starving.
    if (cacheMissesThisTick > 0) {
        Counter("CachingReader::read(): Failed to read chunk on cache miss") +=
                static_cast<int>(cacheMissesThisTick);
    }

    const int overloadCount = static_cast<int>(
            ControlObject::get(kAudioLatencyOverloadCountKey));
    if (overloadCount < m_lastSeenOverloadCount) {
        // SoundManager::resetUnderflowCount() runs when devices are set up.
        // Adopt the reset instead of waiting for the count to climb back to
        // its pre-reset value.
        m_lastSeenOverloadCount = overloadCount;
    }
    const bool underflowAdvanced = overloadCount > m_lastSeenOverloadCount;
    if (underflowAdvanced) {
        m_lastSeenOverloadCount = overloadCount;
    }
    // A reader underrun can feed silence into a healthy device callback, so no
    // PortAudio underflow code is raised in that case. Report those too,
    // otherwise the one scenario this counter exists for would stay invisible.
    const bool starvation = !underflowAdvanced &&
            cacheMissesThisTick >= kStarvationMissesPerTick;
    if (!underflowAdvanced && !starvation) {
        return;
    }

    // Rate limit. The overload counter itself advances at most once per
    // 500 ms, so this collapses a sustained underflow storm into one line per
    // second while keeping the cache-miss delta cumulative over the window.
    if (m_timeSinceDropoutLog.isValid() &&
            m_timeSinceDropoutLog.elapsed() < kDropoutLogIntervalMs) {
        return;
    }
    m_timeSinceDropoutLog.start();

    const quint64 cacheMissDelta = cacheMisses >= m_lastLoggedCacheMissCount
            ? cacheMisses - m_lastLoggedCacheMissCount
            : 0;
    m_lastLoggedCacheMissCount = cacheMisses;

    const bool stemsActive =
            m_pStemService && m_pStemService->hasPendingWork();
    const bool loadAnalysisSuspended =
            m_pPlayerManager && m_pPlayerManager->isBackgroundAnalysisSuspended();
    const bool loadAnalysisActive =
            m_pPlayerManager && m_pPlayerManager->hasPendingLoadAnalysis();
    const bool batchAnalysisSuspended =
            m_stemBusy || m_pausedForOverload;
    const bool batchAnalysisActive =
            m_pAnalysisFeature && m_pAnalysisFeature->isAnalysisActive();
    bool stemDetailActive = false;
    bool stemDetailSuspended = false;
#ifdef __STEM__
    if (WaveformWidgetFactory::isCreated()) {
        StemDetailCache* pCache =
                WaveformWidgetFactory::instance()->stemDetailCache();
        if (pCache) {
            stemDetailActive = pCache->isBuilding();
            stemDetailSuspended = m_pausedForOverload;
        }
    }
#endif

    const char* pReportKind = underflowAdvanced
            ? "Audio dropout:"
            : "Audio starvation (no device underflow):";

    kLogger.warning()
            << pReportKind
            << "underflow_code=" << mixxx::audio::diagnostics::lastUnderflowCode()
            << "[App],audio_latency_usage=" << usage
            << "cache_miss_delta=" << cacheMissDelta
            << "stems=" << (stemsActive ? "active" : "idle")
            << "load_analysis=" << activityLabel(loadAnalysisActive, loadAnalysisSuspended)
            << "batch_analysis=" << activityLabel(batchAnalysisActive, batchAnalysisSuspended)
            << "stem_detail=" << activityLabel(stemDetailActive, stemDetailSuspended)
            << "audio_latency_overload_count=" << overloadCount;
}

} // namespace mixxx::stems
