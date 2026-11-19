#include <gtest/gtest.h>

#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <initializer_list>
#include <memory>
#include <vector>

#include "control/controlobject.h"
#include "coreservices.h"
#include "library/analysis/analysisfeature.h"
#include "library/analysis/analysislibrarytablemodel.h"
#include "library/autodj/autodjfeature.h"
#include "library/browse/browsefeature.h"
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
#include "library/serato/seratofeature.h"
#include "library/sidebarmodel.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackmodel.h"
#include "library/trackset/crate/crate.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/trackset/playlistfeature.h"
#include "library/trackset/setlogfeature.h"
#include "library/traktor/traktorfeature.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmlsidebarmodelproxy.h"
#include "qml/qmltrackmenuproxy.h"
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
        // Lives for the whole test so the library rows keep a real directory.
        ASSERT_TRUE(m_libraryTracksDir.isValid());

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
    QTemporaryDir m_libraryTracksDir;

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

/// Inserts `count` tracks into the library and returns their IDs in insertion
/// order. `tempDir` must outlive the returned rows.
QList<TrackId> insertLibraryTracks(QTemporaryDir& tempDir, int count) {
    QList<TrackId> trackIds;
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

/// The menu node with the given action id, or an empty map.
QVariantMap findMenuNode(const QVariantList& nodes, const QString& action) {
    for (const QVariant& node : nodes) {
        const QVariantMap map = node.toMap();
        if (map.value(QStringLiteral("action")).toString() == action) {
            return map;
        }
    }
    return {};
}

/// The child row of `parent` whose data column holds `playlistId`.
QModelIndex findPlaylistIndex(const mixxx::qml::QmlSidebarModelProxy& sidebar,
        const QModelIndex& parent,
        int playlistId) {
    for (int row = 0; row < sidebar.rowCount(parent); ++row) {
        const QModelIndex candidate = sidebar.index(row, 0, parent);
        if (candidate.isValid() &&
                sidebar.data(candidate, SidebarModel::DataRole).toInt() ==
                        playlistId) {
            return candidate;
        }
    }
    return {};
}

QString playlistTrackOrder(PlaylistTableModel* pModel) {
    QStringList trackIds;
    for (int row = 0; row < pModel->rowCount(); ++row) {
        trackIds.append(pModel->getTrackId(pModel->index(row, 0)).toString());
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

    const QList<TrackId> trackIds = insertLibraryTracks(m_libraryTracksDir, 5);
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

TEST_F(QmlLibraryProxyTest, SidebarPlaylistContextMenuEdits) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    const QString playlistName = QStringLiteral("Parity Editable");
    const int playlistId = playlistDao.createPlaylist(playlistName);
    ASSERT_GE(playlistId, 0);

    const int playlistRow = findFeatureRow<PlaylistFeature>(sidebar);
    ASSERT_GE(playlistRow, 0);
    const QModelIndex featureRoot = sidebar.index(playlistRow, 0);
    ASSERT_TRUE(featureRoot.isValid());

    // The source root offers the create actions.
    const QVariantList rootMenu = sidebar.contextMenu(featureRoot);
    EXPECT_FALSE(findMenuNode(rootMenu, QStringLiteral("createPlaylist")).isEmpty());
    EXPECT_FALSE(findMenuNode(rootMenu, QStringLiteral("importPlaylist")).isEmpty());

    const QModelIndex playlistIndex =
            findPlaylistIndex(sidebar, featureRoot, playlistId);
    ASSERT_TRUE(playlistIndex.isValid());

    QVariantList menu = sidebar.contextMenu(playlistIndex);
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("rename")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("duplicate")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("remove")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("export")).isEmpty());
    EXPECT_TRUE(findMenuNode(menu, QStringLiteral("rename"))
                        .value(QStringLiteral("enabled"))
                        .toBool());
    EXPECT_EQ(QStringLiteral("Lock"),
            findMenuNode(menu, QStringLiteral("lock"))
                    .value(QStringLiteral("text"))
                    .toString());

    // Locking is the only entry without a dialog, so it can be exercised for
    // real. It must also disable the destructive entries.
    sidebar.invokeContextMenuAction(playlistIndex, QStringLiteral("lock"));
    QTRY_VERIFY(playlistDao.isPlaylistLocked(playlistId));
    menu = sidebar.contextMenu(playlistIndex);
    EXPECT_EQ(QStringLiteral("Unlock"),
            findMenuNode(menu, QStringLiteral("lock"))
                    .value(QStringLiteral("text"))
                    .toString());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("rename"))
                         .value(QStringLiteral("enabled"))
                         .toBool());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("remove"))
                         .value(QStringLiteral("enabled"))
                         .toBool());
    sidebar.invokeContextMenuAction(playlistIndex, QStringLiteral("lock"));
    QTRY_VERIFY(!playlistDao.isPlaylistLocked(playlistId));

    // Invalid rows, source roots and unknown actions are no-ops; in particular
    // they must not open a dialog or change the database.
    const unsigned int playlistsBefore = playlistDao.playlistCount();
    sidebar.invokeContextMenuAction(QModelIndex(), QStringLiteral("rename"));
    sidebar.invokeContextMenuAction(QModelIndex(), QStringLiteral("duplicate"));
    sidebar.invokeContextMenuAction(QModelIndex(), QStringLiteral("remove"));
    sidebar.invokeContextMenuAction(QModelIndex(), QStringLiteral("export"));
    sidebar.invokeContextMenuAction(featureRoot, QStringLiteral("remove"));
    sidebar.invokeContextMenuAction(featureRoot, QStringLiteral("rename"));
    sidebar.invokeContextMenuAction(playlistIndex, QStringLiteral("unknown"));
    QTest::qWait(0);
    EXPECT_EQ(playlistsBefore, playlistDao.playlistCount());
    EXPECT_EQ(playlistName, playlistDao.getPlaylistName(playlistId));

    // Removing a locked playlist is a no-op and asks nothing.
    ASSERT_TRUE(playlistDao.setPlaylistLocked(playlistId, true));
    sidebar.invokeContextMenuAction(playlistIndex, QStringLiteral("remove"));
    QTest::qWait(0);
    EXPECT_TRUE(playlistDao.playlistExists(playlistId));
    ASSERT_TRUE(playlistDao.setPlaylistLocked(playlistId, false));

    EXPECT_TRUE(sidebar.contextMenu(QModelIndex()).isEmpty());
}

TEST_F(QmlLibraryProxyTest, SidebarPlaylistMenuActsOnTheClickedChildRow) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    QList<int> playlistIds;
    for (int i = 0; i < 4; ++i) {
        const int id = playlistDao.createPlaylist(
                QStringLiteral("Parity Child %1").arg(i + 1));
        ASSERT_GE(id, 0);
        playlistIds.append(id);
    }

    const int playlistRow = findFeatureRow<PlaylistFeature>(sidebar);
    ASSERT_GE(playlistRow, 0);
    const QModelIndex featureRoot = sidebar.index(playlistRow, 0);
    ASSERT_TRUE(featureRoot.isValid());

    // The fourth child is the one that the old row/column reconstruction used
    // to resolve to the first playlist.
    const QModelIndex fourthIndex =
            findPlaylistIndex(sidebar, featureRoot, playlistIds.at(3));
    ASSERT_TRUE(fourthIndex.isValid());

    sidebar.invokeContextMenuAction(fourthIndex, QStringLiteral("lock"));
    QTRY_VERIFY(playlistDao.isPlaylistLocked(playlistIds.at(3)));
    for (int i = 0; i < 3; ++i) {
        EXPECT_FALSE(playlistDao.isPlaylistLocked(playlistIds.at(i)))
                << "playlist" << i << "must stay unlocked";
    }

    // The child model can be rebuilt by these actions, so resolve the row again
    // and check that the menu now offers to unlock exactly this child.
    const QModelIndex refreshedIndex =
            findPlaylistIndex(sidebar, featureRoot, playlistIds.at(3));
    ASSERT_TRUE(refreshedIndex.isValid());
    EXPECT_EQ(QStringLiteral("Unlock"),
            findMenuNode(sidebar.contextMenu(refreshedIndex),
                    QStringLiteral("lock"))
                    .value(QStringLiteral("text"))
                    .toString());
}

TEST_F(QmlLibraryProxyTest, SidebarCrateContextMenuLocks) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    TrackCollection* pCollection = mixxx::qml::QmlLibraryProxy::get()
                                           ->trackCollectionManager()
                                           ->internalCollection();
    ASSERT_NE(nullptr, pCollection);
    Crate crate;
    crate.setName(QStringLiteral("Parity Crate"));
    CrateId crateId;
    ASSERT_TRUE(pCollection->insertCrate(crate, &crateId));

    const int crateRow = findFeatureRow<CrateFeature>(sidebar);
    ASSERT_GE(crateRow, 0);
    const QModelIndex crateRoot = sidebar.index(crateRow, 0);
    ASSERT_TRUE(crateRoot.isValid());

    QModelIndex crateIndex;
    for (int row = 0; row < sidebar.rowCount(crateRoot); ++row) {
        const QModelIndex candidate = sidebar.index(row, 0, crateRoot);
        if (candidate.isValid() &&
                sidebar.data(candidate, SidebarModel::DataRole) ==
                        crateId.toVariant()) {
            crateIndex = candidate;
            break;
        }
    }
    ASSERT_TRUE(crateIndex.isValid());

    QVariantList menu = sidebar.contextMenu(crateIndex);
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("rename")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("duplicate")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("remove")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("export")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("lock")).isEmpty());

    sidebar.invokeContextMenuAction(crateIndex, QStringLiteral("lock"));
    const auto crateIsLocked = [&pCollection, crateId]() {
        Crate crate;
        return pCollection->crates().readCrateById(crateId, &crate) &&
                crate.isLocked();
    };
    QTRY_VERIFY(crateIsLocked());
    // Locking rebuilds the crate child model and deletes the TreeItems, so the
    // old index must not be reused.
    crateIndex = QModelIndex();
    for (int row = 0; row < sidebar.rowCount(crateRoot); ++row) {
        const QModelIndex candidate = sidebar.index(row, 0, crateRoot);
        if (candidate.isValid() &&
                sidebar.data(candidate, SidebarModel::DataRole) ==
                        crateId.toVariant()) {
            crateIndex = candidate;
            break;
        }
    }
    ASSERT_TRUE(crateIndex.isValid());
    menu = sidebar.contextMenu(crateIndex);
    EXPECT_EQ(QStringLiteral("Unlock"),
            findMenuNode(menu, QStringLiteral("lock"))
                    .value(QStringLiteral("text"))
                    .toString());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("remove"))
                         .value(QStringLiteral("enabled"))
                         .toBool());

    // Invalid rows and drops are ignored.
    sidebar.invokeContextMenuAction(QModelIndex(), QStringLiteral("lock"));
    QTest::qWait(0);
    EXPECT_FALSE(sidebar.dropUrls(QModelIndex(), {}));
    EXPECT_FALSE(sidebar.dropUrls(QModelIndex(),
            {QUrl::fromLocalFile(QStringLiteral("parity.mp3"))}));
}

TEST_F(QmlLibraryProxyTest, SidebarAddActionOnlyOnSourcesThatCreateItems) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    // The hover "+" runs the first entry of the same row's context menu.
    const int playlistsRow = findFeatureRow<PlaylistFeature>(sidebar);
    ASSERT_GE(playlistsRow, 0);
    const QModelIndex playlistsRoot = sidebar.index(playlistsRow, 0);
    ASSERT_TRUE(playlistsRoot.isValid());
    EXPECT_EQ(QStringLiteral("createPlaylist"), sidebar.addAction(playlistsRoot));

    const int cratesRow = findFeatureRow<CrateFeature>(sidebar);
    ASSERT_GE(cratesRow, 0);
    EXPECT_EQ(QStringLiteral("createCrate"),
            sidebar.addAction(sidebar.index(cratesRow, 0)));

    const int browseRow = findFeatureRow<BrowseFeature>(sidebar);
    ASSERT_GE(browseRow, 0);
    EXPECT_EQ(QStringLiteral("addToLibrary"),
            sidebar.addAction(sidebar.index(browseRow, 0)));

    // Sources without an add action, and every child row, show no "+".
    const int tracksRow = findFeatureRow<MixxxLibraryFeature>(sidebar);
    ASSERT_GE(tracksRow, 0);
    EXPECT_TRUE(sidebar.addAction(sidebar.index(tracksRow, 0)).isEmpty());
    EXPECT_TRUE(sidebar.addAction(QModelIndex()).isEmpty());

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    const int playlistId = playlistDao.createPlaylist(QStringLiteral("Parity Add"));
    ASSERT_GE(playlistId, 0);
    const QModelIndex playlistIndex =
            findPlaylistIndex(sidebar, playlistsRoot, playlistId);
    ASSERT_TRUE(playlistIndex.isValid());
    EXPECT_TRUE(sidebar.addAction(playlistIndex).isEmpty());
}

TEST_F(QmlLibraryProxyTest, SidebarTracksTheSelectedSource) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    // update() activates the default source, which is the first row.
    const QModelIndex defaultIndex = sidebar.index(0, 0);
    ASSERT_TRUE(defaultIndex.isValid());
    EXPECT_EQ(defaultIndex, sidebar.selectedIndex());

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    const int playlistId =
            playlistDao.createPlaylist(QStringLiteral("Parity Selected"));
    ASSERT_GE(playlistId, 0);

    const int playlistsRow = findFeatureRow<PlaylistFeature>(sidebar);
    ASSERT_GE(playlistsRow, 0);
    const QModelIndex playlistsRoot = sidebar.index(playlistsRow, 0);
    ASSERT_TRUE(playlistsRoot.isValid());

    sidebar.activate(playlistsRoot);
    EXPECT_EQ(playlistsRoot, sidebar.selectedIndex());

    const QModelIndex playlistIndex =
            findPlaylistIndex(sidebar, playlistsRoot, playlistId);
    ASSERT_TRUE(playlistIndex.isValid());
    sidebar.activate(playlistIndex);
    EXPECT_EQ(playlistIndex, sidebar.selectedIndex());

    // Deleting the selected playlist rebuilds the child model. The tracked row
    // must not outlive its TreeItem.
    playlistDao.deletePlaylist(playlistId);
    QTest::qWait(0);
    EXPECT_FALSE(sidebar.selectedIndex().isValid());
}

/// Ids of the sources returned by QmlSidebarModelProxy::sources(), in order.
QStringList sourceIds(const QVariantList& sources) {
    QStringList ids;
    ids.reserve(sources.size());
    for (const QVariant& source : sources) {
        ids.append(source.toMap().value(QStringLiteral("id")).toString());
    }
    return ids;
}

TEST_F(QmlLibraryProxyTest, SidebarListsEverySourceIncludingHiddenOnes) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    // The Show* preference decides which features exist, so this list no
    // longer depends on which external libraries are installed.
    EXPECT_EQ(QStringList({QStringLiteral("tracks"),
                      QStringLiteral("autodj"),
                      QStringLiteral("playlist"),
                      QStringLiteral("crates"),
                      QStringLiteral("computer"),
                      QStringLiteral("recordings"),
                      QStringLiteral("history"),
                      QStringLiteral("prepare"),
                      QStringLiteral("rhythmbox"),
                      QStringLiteral("banshee"),
                      QStringLiteral("itunes"),
                      QStringLiteral("traktor"),
                      QStringLiteral("rekordbox"),
                      QStringLiteral("serato")}),
            sourceIds(sidebar.sources()).mid(0, 14));

    // The availability role mirrors LibraryFeature::isAvailable().
    const int tracksRow = findFeatureRow<MixxxLibraryFeature>(sidebar);
    ASSERT_GE(tracksRow, 0);
    EXPECT_TRUE(sidebar.index(tracksRow, 0)
                        .data(mixxx::qml::QmlSidebarModelProxy::AvailableRole)
                        .toBool());

    const int traktorRow = findFeatureRow<TraktorFeature>(sidebar);
    ASSERT_GE(traktorRow, 0);
    const QModelIndex traktorIndex = sidebar.index(traktorRow, 0);
    auto* pTraktor = dynamic_cast<TraktorFeature*>(sidebar.features().at(traktorRow));
    ASSERT_NE(nullptr, pTraktor);
    const bool traktorAvailable = traktorIndex
                                          .data(mixxx::qml::QmlSidebarModelProxy::AvailableRole)
                                          .toBool();
    EXPECT_EQ(pTraktor->isAvailable(), traktorAvailable);

    // An unavailable source stays in the tree but cannot be activated.
    if (!traktorAvailable) {
        mixxx::qml::QmlLibraryTrackListModel* pShownTracklist = sidebar.tracklist();
        sidebar.activate(traktorIndex);
        EXPECT_EQ(pShownTracklist, sidebar.tracklist());
    }
}

TEST_F(QmlLibraryProxyTest, SidebarHidesAndShowsSources) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    const int rowCount = sidebar.rowCount();
    const int seratoRow = findFeatureRow<SeratoFeature>(sidebar);
    ASSERT_GE(seratoRow, 0);

    sidebar.setSourceHidden(QStringLiteral("serato"), true);
    EXPECT_EQ(rowCount - 1, sidebar.rowCount());
    EXPECT_EQ(-1, findFeatureRow<SeratoFeature>(sidebar));
    EXPECT_TRUE(sourceIds(sidebar.sources()).contains(QStringLiteral("serato")));
    EXPECT_FALSE(findMenuNode(sidebar.contextMenu(sidebar.index(0, 0)),
                       QStringLiteral("hideSource"))
                         .isEmpty());

    // Unknown ids are ignored instead of being written to Config.
    sidebar.setSourceHidden(QStringLiteral("not-a-source"), true);
    EXPECT_EQ(rowCount - 1, sidebar.rowCount());

    sidebar.setSourceHidden(QStringLiteral("serato"), false);
    EXPECT_EQ(rowCount, sidebar.rowCount());
    EXPECT_GE(findFeatureRow<SeratoFeature>(sidebar), 0);

    // showAllSources() restores several hidden sources at once.
    sidebar.setSourceHidden(QStringLiteral("serato"), true);
    sidebar.setSourceHidden(QStringLiteral("traktor"), true);
    EXPECT_EQ(rowCount - 2, sidebar.rowCount());
    sidebar.showAllSources();
    EXPECT_EQ(rowCount, sidebar.rowCount());
}

TEST_F(QmlLibraryProxyTest, SidebarHidingTheShownSourceShowsTheFirstSource) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    const int seratoRow = findFeatureRow<SeratoFeature>(sidebar);
    ASSERT_GE(seratoRow, 0);
    sidebar.activate(sidebar.index(seratoRow, 0));
    ASSERT_NE(nullptr, sidebar.tracklist());

    sidebar.setSourceHidden(QStringLiteral("serato"), true);

    // The hidden source cannot stay selected; the first visible source wins.
    ASSERT_NE(nullptr, sidebar.tracklist());
    EXPECT_TRUE(sidebar.selectedIndex().isValid());
    EXPECT_EQ(sidebar.index(0, 0), sidebar.selectedIndex());
}

TEST_F(QmlLibraryProxyTest, TrackListRemoveHonorsLockAndBounds) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    const QList<TrackId> trackIds = insertLibraryTracks(m_libraryTracksDir, 3);
    ASSERT_EQ(3, static_cast<int>(trackIds.size()));

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    const int playlistId = playlistDao.createPlaylist(QStringLiteral("Parity Locked"));
    ASSERT_GE(playlistId, 0);
    ASSERT_TRUE(playlistDao.appendTracksToPlaylist(trackIds, playlistId));
    ASSERT_TRUE(playlistDao.setPlaylistLocked(playlistId, true));

    const int playlistRow = findFeatureRow<PlaylistFeature>(sidebar);
    ASSERT_GE(playlistRow, 0);
    const QModelIndex playlistIndex = findPlaylistIndex(
            sidebar, sidebar.index(playlistRow, 0), playlistId);
    ASSERT_TRUE(playlistIndex.isValid());
    sidebar.activate(playlistIndex);
    ASSERT_NE(nullptr, sidebar.tracklist());
    ASSERT_EQ(3, sidebar.tracklist()->rowCount());

    mixxx::qml::QmlLibraryTrackMenu menu;
    // A locked playlist ignores the removal without asking for confirmation.
    menu.removeSelectedRows(sidebar.tracklist(), {0});
    EXPECT_EQ(3, sidebar.tracklist()->rowCount());
    // Empty and out-of-range selections are ignored as well.
    menu.removeSelectedRows(sidebar.tracklist(), {});
    menu.removeSelectedRows(sidebar.tracklist(), {-1, 99});
    EXPECT_EQ(3, sidebar.tracklist()->rowCount());
}

TEST_F(QmlLibraryProxyTest, TrackMenuDropAndReorderValidateInput) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    const QList<TrackId> trackIds = insertLibraryTracks(m_libraryTracksDir, 4);
    ASSERT_EQ(4, static_cast<int>(trackIds.size()));

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    const int playlistId = playlistDao.createPlaylist(QStringLiteral("Parity Reorder"));
    ASSERT_GE(playlistId, 0);
    ASSERT_TRUE(playlistDao.appendTracksToPlaylist(trackIds, playlistId));

    const int playlistRow = findFeatureRow<PlaylistFeature>(sidebar);
    ASSERT_GE(playlistRow, 0);
    const QModelIndex playlistIndex = findPlaylistIndex(
            sidebar, sidebar.index(playlistRow, 0), playlistId);
    ASSERT_TRUE(playlistIndex.isValid());
    sidebar.activate(playlistIndex);
    ASSERT_NE(nullptr, sidebar.tracklist());
    auto* pPlaylistModel =
            dynamic_cast<PlaylistTableModel*>(sidebar.tracklist()->sourceModel());
    ASSERT_NE(nullptr, pPlaylistModel);
    ASSERT_EQ(4, pPlaylistModel->rowCount());

    const auto expectedOrder = [&trackIds](std::initializer_list<int> positions) {
        QStringList ids;
        for (int position : positions) {
            ids.append(trackIds.at(position).toString());
        }
        return ids.join(QLatin1Char(','));
    };

    mixxx::qml::QmlLibraryTrackMenu menu;
    // Nothing usable: no model, empty lists, unsupported files.
    EXPECT_EQ(0, menu.importDroppedUrls(nullptr, {}));
    EXPECT_EQ(0, menu.importDroppedUrls(sidebar.tracklist(), {}));
    QTemporaryDir dropDir;
    ASSERT_TRUE(dropDir.isValid());
    const QString textFile = dropDir.filePath(QStringLiteral("notes.txt"));
    {
        QFile file(textFile);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("not audio");
    }
    EXPECT_EQ(0, menu.importDroppedUrls(sidebar.tracklist(),
            {QUrl::fromLocalFile(textFile)}));

    // Reordering ignores empty and out-of-range rows.
    menu.moveSelectedRows(sidebar.tracklist(), {}, 0);
    menu.moveSelectedRows(sidebar.tracklist(), {-1, 42}, 0);
    EXPECT_EQ(expectedOrder({0, 1, 2, 3}), playlistTrackOrder(pPlaylistModel));

    // A single row moves to the drop row.
    menu.moveSelectedRows(sidebar.tracklist(), {0}, 2);
    EXPECT_EQ(expectedOrder({1, 0, 2, 3}), playlistTrackOrder(pPlaylistModel));

    // A block moved past the last row keeps its order.
    menu.moveSelectedRows(sidebar.tracklist(), {1, 2}, 4);
    EXPECT_EQ(expectedOrder({1, 3, 0, 2}), playlistTrackOrder(pPlaylistModel));
}

TEST_F(QmlLibraryProxyTest, TrackListRemoveSuccessPath) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    const QList<TrackId> trackIds = insertLibraryTracks(m_libraryTracksDir, 3);
    ASSERT_EQ(3, static_cast<int>(trackIds.size()));

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    const int playlistId = playlistDao.createPlaylist(QStringLiteral("Parity Remove"));
    ASSERT_GE(playlistId, 0);
    ASSERT_TRUE(playlistDao.appendTracksToPlaylist(trackIds, playlistId));

    const int playlistRow = findFeatureRow<PlaylistFeature>(sidebar);
    ASSERT_GE(playlistRow, 0);
    const QModelIndex playlistIndex = findPlaylistIndex(
            sidebar, sidebar.index(playlistRow, 0), playlistId);
    ASSERT_TRUE(playlistIndex.isValid());
    sidebar.activate(playlistIndex);
    ASSERT_NE(nullptr, sidebar.tracklist());
    auto* pPlaylistModel =
            dynamic_cast<PlaylistTableModel*>(sidebar.tracklist()->sourceModel());
    ASSERT_NE(nullptr, pPlaylistModel);
    ASSERT_EQ(3, pPlaylistModel->rowCount());

    // Without the confirmation the removal runs straight through: this is the
    // path a real Delete key press takes after "Don't ask again".
    pPlaylistModel->setRequireConfirmationToHideRemoveTracks(false);
    mixxx::qml::QmlLibraryTrackMenu menu;
    menu.removeSelectedRows(sidebar.tracklist(), {1});
    ASSERT_EQ(2, pPlaylistModel->rowCount());
    EXPECT_EQ(trackIds.at(0).toString() + QLatin1Char(',') +
                    trackIds.at(2).toString(),
            playlistTrackOrder(pPlaylistModel));
}

TEST_F(QmlLibraryProxyTest, SidebarDropsAndImportPlaceTracks) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    const QString wavPath =
            getTestDir().filePath(QStringLiteral("fixtures/beatgrid_128bpm.wav"));
    ASSERT_TRUE(QFile::exists(wavPath)) << wavPath.toStdString();
    const QUrl wavUrl = QUrl::fromLocalFile(wavPath);

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    const int playlistId = playlistDao.createPlaylist(QStringLiteral("Parity Drops"));
    ASSERT_GE(playlistId, 0);

    const int playlistRow = findFeatureRow<PlaylistFeature>(sidebar);
    ASSERT_GE(playlistRow, 0);
    QModelIndex playlistIndex = findPlaylistIndex(
            sidebar, sidebar.index(playlistRow, 0), playlistId);
    ASSERT_TRUE(playlistIndex.isValid());
    sidebar.activate(playlistIndex);
    ASSERT_NE(nullptr, sidebar.tracklist());
    auto* pPlaylistModel =
            dynamic_cast<PlaylistTableModel*>(sidebar.tracklist()->sourceModel());
    ASSERT_NE(nullptr, pPlaylistModel);

    mixxx::qml::QmlLibraryTrackMenu menu;
    // A real audio file placed with a destination row.
    ASSERT_EQ(1, menu.importDroppedUrls(sidebar.tracklist(), {wavUrl}, 0));
    ASSERT_EQ(1, pPlaylistModel->rowCount());
    const TrackId importedId =
            pPlaylistModel->getTrackId(pPlaylistModel->index(0, 0));

    // A second, distinct file dropped before row 0 is inserted, not appended.
    const QString secondPath =
            m_libraryTracksDir.filePath(QStringLiteral("parity-second.wav"));
    ASSERT_TRUE(QFile::copy(wavPath, secondPath));
    ASSERT_EQ(1, menu.importDroppedUrls(sidebar.tracklist(), {QUrl::fromLocalFile(secondPath)}, 0));
    ASSERT_EQ(2, pPlaylistModel->rowCount());
    const TrackId secondId =
            pPlaylistModel->getTrackId(pPlaylistModel->index(0, 0));
    EXPECT_NE(importedId, secondId);
    EXPECT_EQ(importedId, pPlaylistModel->getTrackId(pPlaylistModel->index(1, 0)));

    // A drop on the playlist row appends through the feature path.
    EXPECT_TRUE(sidebar.dropUrls(playlistIndex, {wavUrl}));
    ASSERT_EQ(3, pPlaylistModel->rowCount());
    EXPECT_EQ(importedId, pPlaylistModel->getTrackId(pPlaylistModel->index(2, 0)));

    // A locked playlist rejects the drop without asking and keeps its rows.
    ASSERT_TRUE(playlistDao.setPlaylistLocked(playlistId, true));
    playlistIndex = findPlaylistIndex(
            sidebar, sidebar.index(playlistRow, 0), playlistId);
    ASSERT_TRUE(playlistIndex.isValid());
    EXPECT_FALSE(sidebar.dropUrls(playlistIndex, {wavUrl}));
    EXPECT_EQ(3, pPlaylistModel->rowCount());
    ASSERT_TRUE(playlistDao.setPlaylistLocked(playlistId, false));
}

TEST_F(QmlLibraryProxyTest, SidebarHistoryProtectsTodaysPlaylist) {
    mixxx::qml::QmlSidebarModelProxy sidebar;
    sidebar.setColumns({});
    sidebar.update();

    PlaylistDAO& playlistDao = mixxx::qml::QmlLibraryProxy::get()
                                       ->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();
    const int historyRow = findFeatureRow<SetlogFeature>(sidebar);
    ASSERT_GE(historyRow, 0);
    const QModelIndex historyRoot = sidebar.index(historyRow, 0);
    ASSERT_TRUE(historyRoot.isValid());
    ASSERT_GE(sidebar.rowCount(historyRoot), 1);

    // The fresh profile has exactly today's history playlist.
    const QModelIndex todayIndex = sidebar.index(0, 0, historyRoot);
    ASSERT_TRUE(todayIndex.isValid());
    const int playlistId = sidebar.data(todayIndex, SidebarModel::DataRole).toInt();
    ASSERT_TRUE(playlistDao.playlistExists(playlistId));

    const QVariantList menu = sidebar.contextMenu(todayIndex);
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("remove")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("lock")).isEmpty());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("remove"))
                    .value(QStringLiteral("enabled"))
                    .toBool());
    EXPECT_FALSE(findMenuNode(menu, QStringLiteral("lock"))
                    .value(QStringLiteral("enabled"))
                    .toBool());

    // Even a direct invoke must not lock the playlist that is being recorded.
    sidebar.invokeContextMenuAction(todayIndex, QStringLiteral("lock"));
    QTest::qWait(0);
    EXPECT_FALSE(playlistDao.isPlaylistLocked(playlistId));
}

} // namespace
