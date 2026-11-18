#include <gtest/gtest.h>

#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <initializer_list>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "coreservices.h"
#include "library/analysis/analysisfeature.h"
#include "library/analysis/analysislibrarytablemodel.h"
#include "library/autodj/autodjfeature.h"
#include "library/browse/browsetablemodel.h"
#include "library/dao/playlistdao.h"
#include "library/dao/trackschema.h"
#include "library/library.h"
#include "library/missing_hidden/hiddentablemodel.h"
#include "library/missing_hidden/missingtablemodel.h"
#include "library/mixxxlibraryfeature.h"
#include "library/playlisttablemodel.h"
#include "library/proxytrackmodel.h"
#include "library/recording/recordingfeature.h"
#include "library/sidebarmodel.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackmodel.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmlsidebarmodelproxy.h"
#include "qml/qmltrackproxy.h"
#include "test/mixxxtest.h"
#include "track/beats.h"
#include "track/track.h"
#include "util/cmdlineargs.h"
#include "util/versionstore.h"

namespace {

class RejectFileDialogs final : public QObject {
  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show) {
            if (auto* dialog = qobject_cast<QFileDialog*>(watched)) {
                QTimer::singleShot(0, dialog, &QDialog::reject);
            }
        }
        return false;
    }
};

void writeTestProfile(const QString& settingsPath) {
    auto settings = UserSettingsPointer(new UserSettings(settingsPath + "mixxx.cfg"));
    settings->setValue(ConfigKey("[Config]", "Version"), VersionStore::FUTURE_UNSTABLE);
    settings->save();

    QFile soundConfig(settingsPath + "soundconfig.xml");
    ASSERT_TRUE(soundConfig.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_GT(soundConfig.write("<SoundManagerConfig api=\"None\"/>\n"), 0);
}

class QmlLibraryProxyTest : public MixxxTest {
  protected:
    void SetUp() override {
        m_previousArgs = CmdlineArgs::Instance();
        ASSERT_TRUE(m_profile.isValid());

        const QString settingsPath = m_profile.path() + QLatin1Char('/');
        writeTestProfile(settingsPath);

        CmdlineArgs::Instance().setSettingsPath(settingsPath);
        CmdlineArgs args;
        args.setSettingsPath(settingsPath);

        m_rejectFileDialogs = std::make_unique<RejectFileDialogs>();
        application()->installEventFilter(m_rejectFileDialogs.get());
        m_coreServices = std::make_shared<mixxx::CoreServices>(args, application());
        m_coreServices->initialize();
        application()->removeEventFilter(m_rejectFileDialogs.get());

        m_proxy = std::make_unique<mixxx::qml::QmlLibraryProxy>();
    }

    void TearDown() override {
        m_proxy.reset();
        m_coreServices.reset();
        if (m_rejectFileDialogs) {
            application()->removeEventFilter(m_rejectFileDialogs.get());
            m_rejectFileDialogs.reset();
        }
        CmdlineArgs::Instance() = m_previousArgs;
    }

    void setDeckControl(const QString& key, double value) {
        const ConfigKey configKey(QStringLiteral("[Channel1]"), key);
        if (!ControlObject::exists(configKey)) {
            m_testControls.emplace_back(std::make_unique<ControlObject>(configKey));
        }
        ControlObject::set(configKey, value);
    }

    /// The settings object the library and its features were created with.
    UserSettingsPointer settings() const {
        return m_coreServices ? m_coreServices->getSettings() : UserSettingsPointer();
    }

    std::unique_ptr<mixxx::qml::QmlLibraryProxy> m_proxy;

  private:
    QTemporaryDir m_profile;
    CmdlineArgs m_previousArgs;
    std::unique_ptr<RejectFileDialogs> m_rejectFileDialogs;
    std::shared_ptr<mixxx::CoreServices> m_coreServices;
    std::vector<std::unique_ptr<ControlObject>> m_testControls;
};

TEST_F(QmlLibraryProxyTest, DeckHotcueJumpDirectionMatchesLegacyRules) {
    constexpr auto kSampleRate = mixxx::audio::SampleRate(8000);
    constexpr auto kCuePosition = mixxx::audio::FramePos(10000);
    const QString group = QStringLiteral("[Channel1]");

    auto pTrack = Track::newTemporary();
    pTrack->setAudioProperties(
            mixxx::audio::ChannelCount(2),
            kSampleRate,
            mixxx::audio::Bitrate(),
            mixxx::Duration::fromSeconds(60));
    ASSERT_TRUE(pTrack->trySetBeats(mixxx::Beats::fromConstTempo(
            kSampleRate,
            mixxx::audio::kStartFramePos,
            mixxx::Bpm(48.0))));

    ASSERT_TRUE(pTrack->createAndAddCue(
            mixxx::CueType::HotCue,
            0,
            kCuePosition,
            mixxx::audio::kInvalidFramePos));
    ASSERT_TRUE(pTrack->createAndAddCue(
            mixxx::CueType::Loop,
            1,
            mixxx::audio::FramePos(20000),
            mixxx::audio::FramePos(22000)));
    ASSERT_TRUE(pTrack->createAndAddCue(
            mixxx::CueType::Jump,
            2,
            mixxx::audio::FramePos(22000),
            mixxx::audio::FramePos(20000)));
    ASSERT_TRUE(pTrack->createAndAddCue(
            mixxx::CueType::Jump,
            3,
            mixxx::audio::FramePos(20000),
            mixxx::audio::FramePos(22000)));

    mixxx::qml::QmlTrackProxy trackProxy(pTrack);
    setDeckControl(QStringLiteral("track_samples"), 40000.0);
    setDeckControl(QStringLiteral("quantize"), 0.0);

    setDeckControl(QStringLiteral("playposition"), 0.25);
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Forward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 1));

    setDeckControl(QStringLiteral("playposition"), 0.75);
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Backward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 1));

    setDeckControl(QStringLiteral("playposition"), 0.5025);
    setDeckControl(QStringLiteral("quantize"), 1.0);
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Impossible,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 1));

    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Forward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 2));
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Forward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 3));
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Backward,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 4));
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Impossible,
            m_proxy->deckHotcueJumpDirection(&trackProxy, group, 0));
    EXPECT_EQ(mixxx::qml::QmlLibraryProxy::JumpDirection::Impossible,
            m_proxy->deckHotcueJumpDirection(nullptr, group, 1));
}

/// Returns the sidebar row of the first feature of type T, or -1.
template<typename T>
int findFeatureRow(const mixxx::qml::QmlSidebarModelProxy& sidebar) {
    const QList<LibraryFeature*>& features = sidebar.features();
    for (int row = 0; row < features.size(); ++row) {
        if (dynamic_cast<T*>(features.at(row))) {
            return row;
        }
    }
    return -1;
}

void activateSidebarRow(mixxx::qml::QmlSidebarModelProxy* pSidebar, int row) {
    ASSERT_GE(row, 0);
    pSidebar->activate(pSidebar->index(row, 0));
}

/// Inserts `count` tracks into the library and returns their IDs in insertion order.
QList<TrackId> insertLibraryTracks(int count) {
    QList<TrackId> trackIds;
    QTemporaryDir tempDir;
    EXPECT_TRUE(tempDir.isValid());
    const QDir dir(tempDir.path());
    QSqlDatabase db = mixxx::qml::QmlLibraryProxy::get()
                              ->trackCollectionManager()
                              ->internalCollection()
                              ->database();
    for (int i = 0; i < count; ++i) {
        const int id = 1001 + i;
        const QString filename = QStringLiteral("parity-%1.mp3").arg(i);
        // The playlist model joins library.location to track_locations.id, so
        // a library-only row is invisible. Insert both like TrackDAO does.
        const QString location = dir.filePath(filename);
        QSqlQuery locationQuery(db);
        locationQuery.prepare(QStringLiteral(
                "INSERT INTO track_locations "
                "(location, directory, filename, filesize, fs_deleted, needs_verification) "
                "VALUES (:location, :directory, :filename, 0, 0, 0)"));
        locationQuery.bindValue(QStringLiteral(":location"), location);
        locationQuery.bindValue(QStringLiteral(":directory"), dir.path());
        locationQuery.bindValue(QStringLiteral(":filename"), filename);
        EXPECT_TRUE(locationQuery.exec())
                << locationQuery.lastError().text().toStdString();
        const int locationId = locationQuery.lastInsertId().toInt();

        QSqlQuery libraryQuery(db);
        libraryQuery.prepare(QStringLiteral(
                "INSERT INTO library (id, location, artist, title) "
                "VALUES (:id, :location, :artist, :title)"));
        libraryQuery.bindValue(QStringLiteral(":id"), id);
        libraryQuery.bindValue(QStringLiteral(":location"), locationId);
        libraryQuery.bindValue(QStringLiteral(":artist"),
                QStringLiteral("Artist %1").arg(i));
        libraryQuery.bindValue(QStringLiteral(":title"),
                QString::fromLatin1(QByteArray(1, static_cast<char>('A' + i))));
        EXPECT_TRUE(libraryQuery.exec())
                << libraryQuery.lastError().text().toStdString();
        trackIds.append(TrackId(QVariant(id)));
    }
    return trackIds;
}

/// Comma separated track IDs of the queue, so a failure shows the whole order.
QString autoDjQueueOrder(PlaylistTableModel* pQueueModel) {
    QStringList trackIds;
    for (int row = 0; row < pQueueModel->rowCount(); ++row) {
        trackIds.append(pQueueModel->getTrackId(pQueueModel->index(row, 0)).toString());
    }
    return trackIds.join(QLatin1Char(','));
}

TEST_F(QmlLibraryProxyTest, SidebarSourcesThatOnlyHadWidgetViewsExposeTrackLists) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();
    ASSERT_FALSE(sidebar.features().isEmpty());

    // Missing and Hidden Tracks are the two children of the Tracks source.
    const int tracksRow = findFeatureRow<MixxxLibraryFeature>(sidebar);
    ASSERT_GE(tracksRow, 0);
    const QModelIndex tracksRoot = sidebar.index(tracksRow, 0);
    ASSERT_TRUE(tracksRoot.isValid());

    const QModelIndex missingIndex = sidebar.index(0, 0, tracksRoot);
    ASSERT_TRUE(missingIndex.isValid());
    sidebar.activate(missingIndex);
    ASSERT_NE(nullptr, sidebar.tracklist());
    EXPECT_NE(nullptr,
            dynamic_cast<MissingTableModel*>(sidebar.tracklist()->sourceModel()));

    const QModelIndex hiddenIndex = sidebar.index(1, 0, tracksRoot);
    ASSERT_TRUE(hiddenIndex.isValid());
    sidebar.activate(hiddenIndex);
    ASSERT_NE(nullptr, sidebar.tracklist());
    EXPECT_NE(nullptr,
            dynamic_cast<HiddenTableModel*>(sidebar.tracklist()->sourceModel()));

    const int autoDjRow = findFeatureRow<AutoDJFeature>(sidebar);
    ASSERT_GE(autoDjRow, 0);
    activateSidebarRow(&sidebar, autoDjRow);
    ASSERT_NE(nullptr, sidebar.tracklist());
    auto* pAutoDjFeature =
            dynamic_cast<AutoDJFeature*>(sidebar.features().at(autoDjRow));
    ASSERT_NE(nullptr, pAutoDjFeature);
    EXPECT_EQ(pAutoDjFeature->autoDjQueueModel(),
            dynamic_cast<PlaylistTableModel*>(sidebar.tracklist()->sourceModel()));

    const int analysisRow = findFeatureRow<AnalysisFeature>(sidebar);
    ASSERT_GE(analysisRow, 0);
    activateSidebarRow(&sidebar, analysisRow);
    ASSERT_NE(nullptr, sidebar.tracklist());
    EXPECT_NE(nullptr,
            dynamic_cast<AnalysisLibraryTableModel*>(
                    sidebar.tracklist()->sourceModel()));

    const int recordingRow = findFeatureRow<RecordingFeature>(sidebar);
    ASSERT_GE(recordingRow, 0);
    activateSidebarRow(&sidebar, recordingRow);
    ASSERT_NE(nullptr, sidebar.tracklist());
    // Recordings are browsed from disk and searched through a proxy model.
    auto* pRecordingModel =
            dynamic_cast<ProxyTrackModel*>(sidebar.tracklist()->sourceModel());
    ASSERT_NE(nullptr, pRecordingModel);
    EXPECT_NE(nullptr,
            dynamic_cast<BrowseTableModel*>(pRecordingModel->sourceModel()));

    // The QML header sort guard relies on this: the queue must not advertise
    // Sorting (the processor always plays row 0), a regular source must.
    EXPECT_FALSE(pAutoDjFeature->autoDjQueueModel()->hasCapabilities(
            TrackModel::Capability::Sorting));
    activateSidebarRow(&sidebar, tracksRow);
    ASSERT_NE(nullptr, sidebar.tracklist());
    EXPECT_TRUE(sidebar.tracklist()->hasCapabilities(TrackModel::Capability::Sorting));
}

TEST_F(QmlLibraryProxyTest, AutoDjSidebarStateAndQueueEdits) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    EXPECT_FALSE(sidebar.isAutoDjActive());
    EXPECT_FALSE(sidebar.autoDjRepeat());

    const int autoDjRow = findFeatureRow<AutoDJFeature>(sidebar);
    ASSERT_GE(autoDjRow, 0);
    QSignalSpy activeSpy(&sidebar, &mixxx::qml::QmlSidebarModelProxy::autoDjActiveChanged);
    activateSidebarRow(&sidebar, autoDjRow);
    EXPECT_TRUE(sidebar.isAutoDjActive());
    EXPECT_EQ(1, activeSpy.count());

    // The preferences are the ones DlgAutoDJ and AutoDJProcessor use.
    UserSettingsPointer pSettings = settings();
    ASSERT_TRUE(pSettings);

    sidebar.setAutoDjTransitionSeconds(17);
    EXPECT_EQ(17, sidebar.autoDjTransitionSeconds());
    EXPECT_EQ(17, pSettings->getValue<int>(ConfigKey("[Auto DJ]", "Transition"), -1));

    sidebar.setAutoDjRepeat(true);
    EXPECT_TRUE(sidebar.autoDjRepeat());
    EXPECT_TRUE(pSettings->getValue<bool>(ConfigKey("[Auto DJ]", "Requeue"), false));
    sidebar.setAutoDjRepeat(false);
    EXPECT_FALSE(sidebar.autoDjRepeat());
    EXPECT_FALSE(pSettings->getValue<bool>(ConfigKey("[Auto DJ]", "Requeue"), true));

    // Empty selections and out-of-range rows are ignored, so the bar can call
    // these unconditionally.
    sidebar.removeAutoDjTracks({});
    sidebar.removeAutoDjTracks({0, 3});
    sidebar.moveAutoDjTracks({}, 1);
    sidebar.moveAutoDjTracks({0}, -1);
    sidebar.moveAutoDjTracks({0}, 1);

    // Leaving the Auto DJ source hides the bar again.
    activateSidebarRow(&sidebar, findFeatureRow<MixxxLibraryFeature>(sidebar));
    EXPECT_FALSE(sidebar.isAutoDjActive());
}

TEST_F(QmlLibraryProxyTest, AutoDjQueueRemoveAndMoveKeepTrackOrder) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    const int autoDjRow = findFeatureRow<AutoDJFeature>(sidebar);
    ASSERT_GE(autoDjRow, 0);
    activateSidebarRow(&sidebar, autoDjRow);
    ASSERT_NE(nullptr, sidebar.tracklist());
    auto* pQueueModel =
            dynamic_cast<PlaylistTableModel*>(sidebar.tracklist()->sourceModel());
    ASSERT_NE(nullptr, pQueueModel);

    const QList<TrackId> trackIds = insertLibraryTracks(5);
    ASSERT_EQ(5, static_cast<int>(trackIds.size()));

    PlaylistDAO& playlistDao =
            mixxx::qml::QmlLibraryProxy::get()
                    ->trackCollectionManager()
                    ->internalCollection()
                    ->getPlaylistDAO();
    const int playlistId = playlistDao.getPlaylistIdFromName(AUTODJ_TABLE);
    ASSERT_GE(playlistId, 0);

    const auto expectedOrder = [&trackIds](std::initializer_list<int> positions) {
        QStringList ids;
        for (int position : positions) {
            ids.append(trackIds.at(position).toString());
        }
        return ids.join(QLatin1Char(','));
    };

    auto resetQueue = [&]() {
        playlistDao.clearAutoDJQueue();
        ASSERT_TRUE(playlistDao.appendTracksToPlaylist(trackIds, playlistId));
        pQueueModel->select();
        ASSERT_EQ(5, pQueueModel->rowCount());
    };

    resetQueue();
    EXPECT_EQ(expectedOrder({0, 1, 2, 3, 4}), autoDjQueueOrder(pQueueModel));

    // Remove keeps the order of the remaining tracks and ignores rows that are
    // out of range.
    sidebar.removeAutoDjTracks({2, 5});
    EXPECT_EQ(expectedOrder({0, 1, 3, 4}), autoDjQueueOrder(pQueueModel));

    // A selection that already touches the top edge does not move.
    resetQueue();
    sidebar.moveAutoDjTracks({0, 1}, -1);
    EXPECT_EQ(expectedOrder({0, 1, 2, 3, 4}), autoDjQueueOrder(pQueueModel));

    // A block moves up one step without being reversed.
    resetQueue();
    sidebar.moveAutoDjTracks({2, 3}, -1);
    EXPECT_EQ(expectedOrder({0, 2, 3, 1, 4}), autoDjQueueOrder(pQueueModel));

    // A block moves down one step without being reversed.
    resetQueue();
    sidebar.moveAutoDjTracks({2, 3}, 1);
    EXPECT_EQ(expectedOrder({0, 1, 4, 2, 3}), autoDjQueueOrder(pQueueModel));

    // A selection that already touches the bottom edge does not move.
    resetQueue();
    sidebar.moveAutoDjTracks({3, 4}, 1);
    EXPECT_EQ(expectedOrder({0, 1, 2, 3, 4}), autoDjQueueOrder(pQueueModel));

    // Out-of-range rows are ignored in both directions.
    resetQueue();
    sidebar.moveAutoDjTracks({-1, 5, 6}, -1);
    EXPECT_EQ(expectedOrder({0, 1, 2, 3, 4}), autoDjQueueOrder(pQueueModel));
    sidebar.removeAutoDjTracks({-1, 10});
    EXPECT_EQ(expectedOrder({0, 1, 2, 3, 4}), autoDjQueueOrder(pQueueModel));
}

} // namespace
