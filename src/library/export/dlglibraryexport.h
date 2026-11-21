#pragma once

#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include <QSet>
#include <QSharedPointer>

#include "preferences/usersettings.h"
#include "util/optional.h"
#include "util/parented_ptr.h"

class TrackCollectionManager;
class QWidget;
class CrateId;

namespace mixxx {

struct EnginePrimeExportRequest;
struct RekordboxExportRequest;

/// The device format the library-export dialog writes.
enum class LibraryExportTarget {
    EnginePrime,
    Rekordbox,
};

#ifdef __REKORDBOX__
/// Builds the request for a device export from the dialog's selection.
QSharedPointer<RekordboxExportRequest> createRekordboxExportRequest(
        const QDir& deviceRoot,
        const QSet<CrateId>& crateIdsToExport,
        const QSet<int>& playlistIdsToExport);
#endif

/// The DlgLibraryExport class is a UI window that gathers information from
/// the user about how they would like to export the Mixxx library.
///
/// The dialog offers the Engine DJ library format and, when built with
/// REKORDBOX_EXPORT, a rekordbox USB/SD device target. Each target has its own
/// request signal.
class DlgLibraryExport : public QDialog {
    Q_OBJECT

  public:
    DlgLibraryExport(
            QWidget* parent,
            UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager);

    /// Set the provided crate and playlist to be initially selected for export
    /// on the dialog.  If an unknown crate or playlist is provided, then no
    /// action is taken for that item.
    void setInitialSelection(std::optional<CrateId> crateId, std::optional<int> playlistId);

    /// Refresh the contents of the dialog.
    void refresh();

    /// The target the dialog will export to.
    LibraryExportTarget selectedTarget() const;

    /// Whether this build offers the rekordbox target.
    bool isRekordboxTargetAvailable() const;

    /// Switch the target to rekordbox. No-op when it is not available.
    void selectRekordboxTarget();

  signals:
    /// The startEnginePrimeExport signal is emitted when sufficient information
    /// has been gathered from the user to kick off an Engine DJ export, and
    /// details of the request are provided as part of the signal.
    void startEnginePrimeExport(QSharedPointer<mixxx::EnginePrimeExportRequest>);

#ifdef __REKORDBOX__
    /// Emitted when the user chooses the rekordbox target.
    void startRekordboxExport(QSharedPointer<mixxx::RekordboxExportRequest>);
#endif

  private slots:
    void browseExportDirectory();
    void exportRequested();

  private:
    void checkExistingDatabase();
    void checkRekordboxDestination();
    void updateTargetUi();

    UserSettingsPointer m_pConfig;
    TrackCollectionManager* m_pTrackCollectionManager;

    parented_ptr<QComboBox> m_pTargetCombo;
    parented_ptr<QRadioButton> m_pWholeLibraryRadio;
    parented_ptr<QRadioButton> m_pCratesAndPlaylistsRadio;
    parented_ptr<QListWidget> m_pCratesList;
    parented_ptr<QListWidget> m_pPlaylistsList;
    parented_ptr<QLineEdit> m_pExportDirectoryTextField;
    parented_ptr<QComboBox> m_pVersionCombo;
    parented_ptr<QLabel> m_pExistingDatabaseLabel;
};

} // namespace mixxx
