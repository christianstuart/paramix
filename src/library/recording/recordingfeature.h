#pragma once

#include "library/browse/browsetablemodel.h"
#include "library/libraryfeature.h"
#include "library/proxytrackmodel.h"
#include "preferences/usersettings.h"

class RecordingManager;
class FolderTreeModel;

class RecordingFeature final : public LibraryFeature {
    Q_OBJECT
  public:
    RecordingFeature(Library* parent,
                     UserSettingsPointer pConfig,
                     RecordingManager* pRecordingManager);
    ~RecordingFeature() override = default;

    QVariant title() override;

    void bindLibraryWidget(WLibrary* libraryWidget,
                    KeyboardEventFilter* keyboard) override;

    TreeItemModel* sidebarModel() const override;

  public slots:
    void activate() override;

  signals:
    void setRootIndex(const QModelIndex&);
    void requestRestoreSearch();
    void refreshBrowseModel();

  private:
    /// Points the recording browser model at the current recording directory.
    void updateBrowseModel();

    RecordingManager* const m_pRecordingManager;

    FolderTreeModel* m_pSidebarModel;

    /// The QML track list uses these directly. The classic view in
    /// DlgRecording owns a second pair, but bindLibraryWidget() is never
    /// called by the QML interface.
    BrowseTableModel m_browseModel;
    ProxyTrackModel m_proxyModel;
};
