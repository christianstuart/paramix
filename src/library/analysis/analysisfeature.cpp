#include "library/analysis/analysisfeature.h"

#include <QList>
#include <QThread>
#include <QtDebug>
#include <algorithm>

#include "analyzer/analyzerscheduledtrack.h"
#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/analysis/analysislibrarytablemodel.h"
#include "library/analysis/dlganalysis.h"
#include "library/library.h"
#include "library/trackcollectionmanager.h"
#include "moc_analysisfeature.cpp"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/dnd.h"
#include "util/logger.h"
#include "widget/wlibrary.h"

namespace {

const mixxx::Logger kLogger("AnalysisFeature");

const QString kViewName = QStringLiteral("Analysis");

inline AnalyzerModeFlags getAnalyzerModeFlags(
        const UserSettingsPointer& pConfig) {
    // Always enable at least BPM detection for batch analysis, even if disabled
    // in the config for ad-hoc analysis of tracks.
    // NOTE(uklotzde, 2018-12-26): The previous comment just states the status-quo
    // of the existing code. We should rethink the configuration of analyzers when
    // refactoring/redesigning the analyzer framework.
    int modeFlags = AnalyzerModeFlags::WithBeats | AnalyzerModeFlags::LowPriority;
    if (pConfig->getValue<bool>(ConfigKey("[Library]", "EnableWaveformGenerationWithAnalysis"), true)) {
        modeFlags |= AnalyzerModeFlags::WithWaveform;
    }
    return static_cast<AnalyzerModeFlags>(modeFlags);
}

} // anonymous namespace

// static
int AnalysisFeature::analyzerThreadCount() {
    // Batch analysis is background work. Capping the workers at half of the
    // available cores (and at most 8) leaves headroom for the audio callback,
    // stem separation and the GUI. Every analyzer thread is additionally
    // started with QThread::LowPriority, so even these workers yield to
    // latency-sensitive work.
    return qMax(1, qMin(8, QThread::idealThreadCount() / 2));
}

AnalysisFeature::AnalysisFeature(
        Library* pLibrary,
        UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("prepare")),
          m_baseTitle(tr("Analyze")),
          m_pTrackAnalysisScheduler(TrackAnalysisScheduler::NullPointer()),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pAnalysisView(nullptr),
          m_pAnalysisModel(nullptr),
          m_title(m_baseTitle) {
}

AnalysisFeature::~AnalysisFeature() {
    // We need to delete m_pTrackAnalysisScheduler here immediately synchronously,
    // waiting for pending threads to have finished, to not kill them during exit
    delete m_pTrackAnalysisScheduler.release();
}

void AnalysisFeature::resetTitle() {
    m_title = m_baseTitle;
    emit featureIsLoading(this, false);
}

void AnalysisFeature::setTitleProgress(int currentTrackNumber, int totalTracksCount) {
    m_title = QString("%1 (%2 / %3)")
                      .arg(m_baseTitle,
                              QString::number(currentTrackNumber),
                              QString::number(totalTracksCount));
    emit featureIsLoading(this, false);
}

void AnalysisFeature::bindLibraryWidget(WLibrary* libraryWidget,
        KeyboardEventFilter* keyboard) {
    m_pAnalysisView = new DlgAnalysis(libraryWidget,
            m_pConfig,
            m_pLibrary);
    connect(m_pAnalysisView,
            &DlgAnalysis::loadTrack,
            this,
            &AnalysisFeature::loadTrack);
    connect(m_pAnalysisView,
            &DlgAnalysis::loadTrackToPlayer,
            this,
            [=, this](TrackPointer track, const QString& group) {
                emit loadTrackToPlayer(track, group,
#ifdef __STEM__
                        mixxx::StemChannelSelection(),
#endif
                        false);
            });
    connect(m_pAnalysisView,
            &DlgAnalysis::analyzeTracks,
            this,
            &AnalysisFeature::analyzeTracks);
    connect(m_pAnalysisView,
            &DlgAnalysis::stopAnalysis,
            this,
            &AnalysisFeature::stopAnalysis);

    connect(m_pAnalysisView,
            &DlgAnalysis::trackSelected,
            this,
            &AnalysisFeature::trackSelected);

    connect(this,
            &AnalysisFeature::analysisActive,
            m_pAnalysisView,
            &DlgAnalysis::slotAnalysisActive);

    m_pAnalysisView->installEventFilter(keyboard);

    // Let the DlgAnalysis know whether or not analysis is active.
    emit analysisActive(static_cast<bool>(m_pTrackAnalysisScheduler));

    libraryWidget->registerView(kViewName, m_pAnalysisView);
}

TreeItemModel* AnalysisFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void AnalysisFeature::refreshLibraryModels() {
    if (m_pAnalysisView) {
        m_pAnalysisView->onShow();
    } else if (m_pAnalysisModel) {
        m_pAnalysisModel->select();
    }
}

AnalysisLibraryTableModel* AnalysisFeature::analysisModel() {
    if (!m_pAnalysisModel) {
        m_pAnalysisModel = new AnalysisLibraryTableModel(
                this, m_pLibrary->trackCollectionManager());
        m_pAnalysisModel->setRecentDays(m_pConfig->getValue<unsigned int>(
                ConfigKey("[Analysis]", "RecentDays"),
                AnalysisLibraryTableModel::kDefaultRecentDays));
        // The constructor applies the default day count; re-apply it with the
        // configured one so the first view matches the preference.
        m_pAnalysisModel->showRecentSongs();
    }
    return m_pAnalysisModel;
}

void AnalysisFeature::activate() {
    // qDebug() << "AnalysisFeature::activate()";
    AnalysisLibraryTableModel* pModel = analysisModel();
    pModel->select();
    emit showTrackModel(pModel);
    if (m_pAnalysisView) {
        emit restoreSearch(m_pAnalysisView->currentSearch());
    }
    // Switch last: Library::slotShowTrackModel switches to the generic track
    // table, so the analysis view has to win in the classic UI.
    emit switchToView(kViewName);
    emit enableCoverArtDisplay(true);
}

void AnalysisFeature::analyzeTracks(const QList<AnalyzerScheduledTrack>& tracks) {
    const bool startingAnalysis = !m_pTrackAnalysisScheduler;
    if (startingAnalysis) {
        const int numAnalyzerThreads = analyzerThreadCount();
        kLogger.info()
                << "Starting analysis using"
                << numAnalyzerThreads
                << "low-priority analyzer threads";
        m_pTrackAnalysisScheduler = m_pLibrary->createTrackAnalysisScheduler(
                numAnalyzerThreads,
                getAnalyzerModeFlags(m_pConfig));
        // The scheduler starts its workers suspended and only runs them after
        // the first resume().
        m_suspendApplied = true;

        connect(m_pTrackAnalysisScheduler.get(),
                &TrackAnalysisScheduler::progress,
                m_pAnalysisView,
                &DlgAnalysis::onTrackAnalysisSchedulerProgress);
        connect(m_pTrackAnalysisScheduler.get(),
                &TrackAnalysisScheduler::finished,
                m_pAnalysisView,
                &DlgAnalysis::onTrackAnalysisSchedulerFinished);
        connect(m_pTrackAnalysisScheduler.get(),
                &TrackAnalysisScheduler::progress,
                this,
                &AnalysisFeature::onTrackAnalysisSchedulerProgress);
        connect(m_pTrackAnalysisScheduler.get(),
                &TrackAnalysisScheduler::finished,
                this,
                &AnalysisFeature::onTrackAnalysisSchedulerFinished);
        // Forward the signal to be picked up by Library.
        // Used by WOverview to render the re-analysis progress of loaded tracks.
        // This is the equivalent to PlayerManager's progress signal fired for
        // analysis triggered by loading a track.
        connect(m_pTrackAnalysisScheduler.get(),
                &TrackAnalysisScheduler::trackProgress,
                this,
                &AnalysisFeature::trackProgress);
        // Track the title of the currently analyzed track for the QML UI.
        connect(m_pTrackAnalysisScheduler.get(),
                &TrackAnalysisScheduler::trackProgress,
                this,
                &AnalysisFeature::onTrackAnalysisProgress);
    }

    const int scheduledCount = m_pTrackAnalysisScheduler->scheduleTracks(tracks);
    if (scheduledCount <= 0) {
        if (startingAnalysis) {
            // Nothing was scheduled, so the scheduler would never emit
            // progress or finished. Don't leave it behind in a running state.
            m_pTrackAnalysisScheduler.reset();
        }
        return;
    }

    if (startingAnalysis) {
        m_analysisTitleCache.clear();
        m_analysisTrackId = TrackId();
        m_analysisTrackTitle.clear();
        m_analysisCurrent = 0;
        m_analysisTotal = scheduledCount;
        m_analysisProgress = 0;
        m_analysisActive = true;
        emit analysisActiveChanged();
        emit analysisProgressChanged();
        emit analysisActive(true);
    }
    resumeAnalysis();
}

void AnalysisFeature::suspendAnalysis() {
    m_userSuspended = true;
    applySuspension();
}

void AnalysisFeature::resumeAnalysis() {
    m_userSuspended = false;
    applySuspension();
}

void AnalysisFeature::setBackgroundSuspended(bool suspended) {
    if (m_backgroundSuspended == suspended) {
        return;
    }
    m_backgroundSuspended = suspended;
    applySuspension();
}

void AnalysisFeature::applySuspension() {
    if (!m_pTrackAnalysisScheduler) {
        return; // inactive
    }
    const bool suspend = m_userSuspended || m_backgroundSuspended;
    if (m_suspendApplied == suspend) {
        return;
    }
    m_suspendApplied = suspend;
    kLogger.info() << (suspend ? "Suspending" : "Resuming")
                   << "analysis (explicit suspend:"
                   << (m_userSuspended ? "yes" : "no")
                   << ", background suspend:"
                   << (m_backgroundSuspended ? "yes" : "no")
                   << ")";
    if (suspend) {
        m_pTrackAnalysisScheduler->suspend();
    } else {
        m_pTrackAnalysisScheduler->resume();
    }
}

void AnalysisFeature::stopAnalysis() {
    if (!m_pTrackAnalysisScheduler) {
        return; // inactive
    }
    kLogger.info() << "Stopping analysis";
    m_pTrackAnalysisScheduler->stop();
}

void AnalysisFeature::onTrackAnalysisSchedulerProgress(
        AnalyzerProgress currentTrackProgress,
        int currentTrackNumber,
        int totalTracksCount) {
    // Ignore any delayed progress updates after the analysis
    // has already been stopped.
    if (!m_pTrackAnalysisScheduler) {
        return;
    }
    if (totalTracksCount > 0) {
        setTitleProgress(currentTrackNumber, totalTracksCount);
    } else {
        resetTitle();
    }

    const int previousCurrent = m_analysisCurrent;
    const int previousTotal = m_analysisTotal;
    const int previousProgress = m_analysisProgress;

    m_analysisCurrent = std::max(0, currentTrackNumber);
    m_analysisTotal = std::max(0, totalTracksCount);
    if (m_analysisTotal > 0) {
        // `currentTrackNumber` is 1-based, `currentTrackProgress` is the
        // fraction of that track that is already done. Together they form the
        // aggregate progress over all scheduled tracks.
        double completedTracks = std::max(0, m_analysisCurrent - 1);
        if (currentTrackProgress >= 0.0) {
            completedTracks += std::clamp(currentTrackProgress, 0.0, 1.0);
        }
        m_analysisProgress = std::clamp(
                static_cast<int>(completedTracks * 100.0 / m_analysisTotal), 0, 100);
    } else {
        m_analysisProgress = 0;
    }

    if (previousCurrent != m_analysisCurrent ||
            previousTotal != m_analysisTotal ||
            previousProgress != m_analysisProgress) {
        emit analysisProgressChanged();
    }
}

void AnalysisFeature::onTrackAnalysisSchedulerFinished() {
    if (!m_pTrackAnalysisScheduler) {
        return; // already inactive
    }
    kLogger.info() << "Finishing analysis";
    if (m_pTrackAnalysisScheduler) {
        // Free resources by abandoning the queue after the batch analysis
        // has completed. Batch analysis are not started very frequently
        // during a session and should be avoided while performing live.
        // If the user decides to start a new batch analysis the setup costs
        // for creating the queue with its worker threads are acceptable.
        m_pTrackAnalysisScheduler.reset();
    }
    resetTitle();

    m_analysisActive = false;
    if (m_analysisTotal > 0 && m_analysisCurrent >= m_analysisTotal) {
        // Only a run that reached the last track reports 100%. A stopped run
        // keeps its last aggregate value.
        m_analysisProgress = 100;
    }
    m_analysisTrackTitle.clear();
    m_analysisTrackId = TrackId();
    m_analysisTitleCache.clear();
    emit analysisProgressChanged();
    emit analysisActiveChanged();
    emit analysisActive(false);
}

void AnalysisFeature::onTrackAnalysisProgress(
        TrackId trackId, AnalyzerProgress /*analyzerProgress*/) {
    if (!trackId.isValid() || trackId == m_analysisTrackId) {
        return;
    }
    m_analysisTrackId = trackId;
    const QString title = analysisTitleForTrack(trackId);
    if (m_analysisTrackTitle == title) {
        return;
    }
    m_analysisTrackTitle = title;
    emit analysisProgressChanged();
}

QString AnalysisFeature::analysisTitleForTrack(TrackId trackId) {
    const auto it = m_analysisTitleCache.constFind(trackId);
    if (it != m_analysisTitleCache.constEnd()) {
        return it.value();
    }
    QString title;
    if (m_pLibrary) {
        const TrackPointer pTrack =
                m_pLibrary->trackCollectionManager()->getTrackById(trackId);
        if (pTrack) {
            title = pTrack->getTitle();
        }
    }
    m_analysisTitleCache.insert(trackId, title);
    return title;
}

bool AnalysisFeature::dropAccept(const QList<QUrl>& urls, QObject* pSource) {
    const QList<mixxx::FileInfo> fileInfos =
            // collect all tracks, accept playlist files
            DragAndDropHelper::supportedTracksFromUrls(urls, false, true);
    const QList<TrackId> trackIds =
            m_pLibrary->trackCollectionManager()->resolveTrackIds(fileInfos, pSource);
    QList<AnalyzerScheduledTrack> tracks;
    for (auto trackId : trackIds) {
        tracks.append(trackId);
    }
    analyzeTracks(tracks);
    return tracks.size() > 0;
}

bool AnalysisFeature::dragMoveAccept(const QList<QUrl>& urls) {
    return DragAndDropHelper::urlsContainSupportedTrackFiles(urls, false);
}
