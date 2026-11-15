#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

#include "analyzer/trackanalysisscheduler.h"
#include "library/libraryfeature.h"
#include "library/treeitemmodel.h"
#include "preferences/usersettings.h"
#include "track/trackid.h"
#include "util/parented_ptr.h"

class DlgAnalysis;

class AnalysisFeature : public LibraryFeature {
    Q_OBJECT
    Q_PROPERTY(bool analysisActive READ isAnalysisActive NOTIFY analysisActiveChanged)
    // Percent, 0..100 aggregate over all scheduled tracks
    Q_PROPERTY(int analysisProgress READ analysisProgress NOTIFY analysisProgressChanged)
    Q_PROPERTY(int analysisCurrent READ analysisCurrent NOTIFY analysisProgressChanged)
    Q_PROPERTY(int analysisTotal READ analysisTotal NOTIFY analysisProgressChanged)
    Q_PROPERTY(QString analysisTrackTitle READ analysisTrackTitle NOTIFY
                    analysisProgressChanged)
  public:
    AnalysisFeature(Library* pLibrary,
                    UserSettingsPointer pConfig);
    ~AnalysisFeature() override;

    QVariant title() override {
        return m_title;
    }

    bool dropAccept(const QList<QUrl>& urls, QObject* pSource) override;
    bool dragMoveAccept(const QList<QUrl>& urls) override;
    void bindLibraryWidget(WLibrary* libraryWidget,
                    KeyboardEventFilter* keyboard) override;

    /// Number of batch analyzer worker threads. Exposed so the startup audit
    /// log reports the same value that is actually used.
    static int analyzerThreadCount();

    TreeItemModel* sidebarModel() const override;
    void refreshLibraryModels();

    bool isAnalysisActive() const {
        return m_analysisActive;
    }
    int analysisProgress() const {
        return m_analysisProgress;
    }
    int analysisCurrent() const {
        return m_analysisCurrent;
    }
    int analysisTotal() const {
        return m_analysisTotal;
    }
    QString analysisTrackTitle() const {
        return m_analysisTrackTitle;
    }

  signals:
    void analysisActive(bool bActive);
    void analysisActiveChanged();
    void analysisProgressChanged();
    void trackProgress(TrackId trackId, AnalyzerProgress progress);

  public slots:
    void activate() override;
    void analyzeTracks(const QList<AnalyzerScheduledTrack>& tracks);

    void suspendAnalysis();
    void resumeAnalysis();
    void stopAnalysis();

    /// Suspends or resumes batch analysis on behalf of a background
    /// coordinator, e.g. while stem separation is running. This composes with
    /// the state controlled by suspendAnalysis()/resumeAnalysis(), so a
    /// background resume never overrides an explicit suspend and vice versa.
    void setBackgroundSuspended(bool suspended);

  private slots:
    void onTrackAnalysisSchedulerProgress(AnalyzerProgress currentTrackProgress, int currentTrackNumber, int totalTracksCount);
    void onTrackAnalysisSchedulerFinished();
    void onTrackAnalysisProgress(TrackId trackId, AnalyzerProgress analyzerProgress);

  private:
    /// Applies the combined suspension state to the scheduler, if one exists.
    void applySuspension();

    // Sets the title of this feature to the default name, given by
    // m_sAnalysisTitleName
    void resetTitle();

    // Sets the title of this feature to the default name followed by (x / y)
    // where x is the current track being analyzed and y is the total number of
    // tracks in the job
    void setTitleProgress(int currentTrackNumber, int totalTracksCount);

    // Resolves the title of the track that is currently being analyzed. The
    // result is cached per TrackId, because progress updates arrive frequently
    // and must not hit the database each time.
    QString analysisTitleForTrack(TrackId trackId);

    const QString m_baseTitle;

    TrackAnalysisScheduler::Pointer m_pTrackAnalysisScheduler;

    // Suspension state is composed from an explicit request
    // (suspendAnalysis()/resumeAnalysis()) and the background coordinator.
    // A scheduler is born suspended, hence the initial `true`.
    bool m_userSuspended = false;
    bool m_backgroundSuspended = false;
    bool m_suspendApplied = true;

    parented_ptr<TreeItemModel> m_pSidebarModel;
    DlgAnalysis* m_pAnalysisView;

    // The title is dynamic and reflects the current progress
    QString m_title;

    // State exposed to QML through the properties above
    bool m_analysisActive = false;
    int m_analysisProgress = 0;
    int m_analysisCurrent = 0;
    int m_analysisTotal = 0;
    QString m_analysisTrackTitle;
    TrackId m_analysisTrackId;
    QHash<TrackId, QString> m_analysisTitleCache;
};
