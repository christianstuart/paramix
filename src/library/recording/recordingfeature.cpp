#include "library/recording/recordingfeature.h"

#include "controllers/keyboard/keyboardeventfilter.h"
#include "library/browse/foldertreemodel.h"
#include "library/library.h"
#include "library/recording/dlgrecording.h"
#include "moc_recordingfeature.cpp"
#include "recording/recordingmanager.h"
#include "widget/wlibrary.h"

namespace {

const QString kViewName = QStringLiteral("Recording");

} // anonymous namespace

RecordingFeature::RecordingFeature(Library* pLibrary,
        UserSettingsPointer pConfig,
        RecordingManager* pRecordingManager)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("recordings")),
          m_pRecordingManager(pRecordingManager),
          m_pSidebarModel(new FolderTreeModel(this)),
          m_browseModel(this,
                  pLibrary->trackCollectionManager(),
                  pRecordingManager,
                  "mixxx.db.model.recording"),
          m_proxyModel(&m_browseModel, true) {
    m_proxyModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel.setSortCaseSensitivity(Qt::CaseInsensitive);
}

QVariant RecordingFeature::title() {
    return QVariant(tr("Recordings"));
}

TreeItemModel* RecordingFeature::sidebarModel() const {
    return m_pSidebarModel;
}

void RecordingFeature::bindLibraryWidget(WLibrary* pLibraryWidget,
                                  KeyboardEventFilter *keyboard) {
    //The view will be deleted by LibraryWidget
    DlgRecording* pRecordingView = new DlgRecording(pLibraryWidget,
                                                    m_pConfig,
                                                    m_pLibrary,
                                                    m_pRecordingManager,
                                                    keyboard);

    pRecordingView->installEventFilter(keyboard);
    pLibraryWidget->registerView(kViewName, pRecordingView);
    connect(pRecordingView,
            &DlgRecording::loadTrack,
            this,
            &RecordingFeature::loadTrack);
    connect(pRecordingView,
            &DlgRecording::loadTrackToPlayer,
            this,
            &RecordingFeature::loadTrackToPlayer);
    connect(this,
            &RecordingFeature::refreshBrowseModel,
            pRecordingView,
            &DlgRecording::refreshBrowseModel);
    connect(this,
            &RecordingFeature::requestRestoreSearch,
            pRecordingView,
            &DlgRecording::slotRestoreSearch);
    connect(pRecordingView,
            &DlgRecording::restoreSearch,
            this,
            &RecordingFeature::restoreSearch);
    connect(pRecordingView,
            &DlgRecording::restoreModelState,
            this,
            &RecordingFeature::restoreModelState);
}

void RecordingFeature::activate() {
    updateBrowseModel();
    emit refreshBrowseModel();
    emit requestRestoreSearch();
    emit enableCoverArtDisplay(false);
    emit showTrackModel(&m_proxyModel);
    // Switch last: Library::slotShowTrackModel switches to the generic track
    // table, so the recording view has to win in the classic UI.
    emit switchToView(kViewName);
}

void RecordingFeature::updateBrowseModel() {
    m_browseModel.setPath(mixxx::FileAccess(
            mixxx::FileInfo(m_pRecordingManager->getRecordingDir())));
}

void RecordingFeature::stopPendingTasks() {
    // BrowseTableModel shares a ref-counted browse thread; all references have
    // to be released before the Library is destroyed.
    m_browseModel.stopBrowseThread();
    m_browseModel.releaseBrowseThread();
}
