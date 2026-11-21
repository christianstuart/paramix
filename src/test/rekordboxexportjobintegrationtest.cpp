#include <gtest/gtest.h>
#include <rekordbox_anlz.h>
#include <rekordbox_pdb.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSignalSpy>
#include <QStorageInfo>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QVector>
#include <memory>
#include <string>
#include <vector>

#include "library/export/rekordbox/rekordboxexportjob.h"
#include "library/export/rekordbox/rekordboxexportrequest.h"
#include "test/librarytest.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/track.h"

namespace mixxx {
namespace rekordbox {
namespace {

struct ParsedPdb {
    std::unique_ptr<kaitai::kstream> stream;
    std::unique_ptr<rekordbox_pdb_t> file;

    explicit ParsedPdb(const QByteArray& bytes)
            : stream(std::make_unique<kaitai::kstream>(
                      std::string(bytes.constData(), static_cast<size_t>(bytes.size())))),
              file(std::make_unique<rekordbox_pdb_t>(stream.get())) {
    }
};

struct ParsedAnlz {
    std::unique_ptr<kaitai::kstream> stream;
    std::unique_ptr<rekordbox_anlz_t> file;

    explicit ParsedAnlz(const QByteArray& bytes)
            : stream(std::make_unique<kaitai::kstream>(
                      std::string(bytes.constData(), static_cast<size_t>(bytes.size())))),
              file(std::make_unique<rekordbox_anlz_t>(stream.get())) {
    }
};

QByteArray readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QString decodePdbString(rekordbox_pdb_t::device_sql_string_t* string) {
    if (string == nullptr) {
        return {};
    }
    const auto* body = string->body();
    switch (string->length_and_kind()) {
    case 0x40: {
        const auto* text =
                static_cast<const rekordbox_pdb_t::device_sql_long_ascii_t*>(body);
        return QString::fromLatin1(text->text().c_str(),
                static_cast<int>(text->text().size()));
    }
    case 0x90: {
        const auto* text =
                static_cast<const rekordbox_pdb_t::device_sql_long_utf16le_t*>(body);
        const std::string& raw = text->text();
        QString out;
        for (size_t i = 0; i + 1 < raw.size(); i += 2) {
            const quint16 unit =
                    static_cast<quint8>(static_cast<unsigned char>(raw[i])) |
                    (static_cast<quint16>(
                             static_cast<quint8>(static_cast<unsigned char>(raw[i + 1])))
                            << 8);
            if (unit == 0) {
                break;
            }
            out.append(QChar(unit));
        }
        return out;
    }
    default: {
        const auto* text =
                static_cast<const rekordbox_pdb_t::device_sql_short_ascii_t*>(body);
        return QString::fromLatin1(text->text().c_str(),
                static_cast<int>(text->text().size()));
    }
    }
}

QString decodeUtf16Be(const std::string& raw) {
    QString out;
    for (size_t i = 0; i + 1 < raw.size(); i += 2) {
        const quint16 unit =
                (static_cast<quint16>(
                         static_cast<quint8>(static_cast<unsigned char>(raw[i])))
                        << 8) |
                static_cast<quint8>(static_cast<unsigned char>(raw[i + 1]));
        if (unit == 0) {
            break;
        }
        out.append(QChar(unit));
    }
    return out;
}

rekordbox_pdb_t::table_t* tableOf(
        rekordbox_pdb_t* file, rekordbox_pdb_t::page_type_t type) {
    for (auto* table : *file->tables()) {
        if (table->type() == type) {
            return table;
        }
    }
    return nullptr;
}

QVector<rekordbox_pdb_t::row_ref_t*> rowsOf(rekordbox_pdb_t::table_t* table) {
    QVector<rekordbox_pdb_t::row_ref_t*> rows;
    if (table == nullptr) {
        return rows;
    }
    auto* page = table->first_page()->body();
    const quint32 lastPage = table->last_page()->index();
    for (int guard = 0; guard < 4096; ++guard) {
        if (page->is_data_page() && page->num_rows() > 0) {
            for (auto* group : *page->row_groups()) {
                for (auto* row : *group->rows()) {
                    if (row->present()) {
                        rows.append(row);
                    }
                }
            }
        }
        if (page->page_index() == lastPage) {
            break;
        }
        const quint32 next = page->next_page()->index();
        if (next == page->page_index()) {
            break;
        }
        page = page->next_page()->body();
    }
    return rows;
}

rekordbox_anlz_t::tagged_section_t* anlzSection(
        rekordbox_anlz_t* file, rekordbox_anlz_t::section_tags_t tag, int index = 0) {
    for (const auto& section : *file->sections()) {
        if (section->fourcc() == tag) {
            if (index == 0) {
                return section.get();
            }
            --index;
        }
    }
    return nullptr;
}

class RekordboxExportJobIntegrationTest : public LibraryTest {
  protected:
    TrackPointer addTrack(const QString& fileName,
            const QString& title,
            const QString& artist,
            const QString& album) {
        const QString source = getTestDir().filePath(
                QStringLiteral("id3-test-data/cover-test-øé~ł€˚.wav"));
        const QString destination = m_sourceDir.filePath(fileName);
        EXPECT_TRUE(QFile::copy(source, destination));
        internalCollection()->getDirectoryDAO().addDirectory(
                mixxx::FileInfo(m_sourceDir.path()));
        TrackPointer track = getOrAddTrackByLocation(destination);
        if (!track) {
            return nullptr;
        }
        track->setTitle(title);
        track->setArtist(artist);
        track->setAlbum(album);
        track->setComment(QStringLiteral("Job integration test"));
        track->setYear(QStringLiteral("2024"));
        track->setTrackNumber(QStringLiteral("1"));
        track->setRating(4);
        const auto sampleRate = track->getSampleRate();
        if (sampleRate.isValid()) {
            track->trySetBeats(Beats::fromConstTempo(
                    sampleRate, audio::FramePos(0), Bpm(120.0)));
            const double framesPerSecond = sampleRate.value();
            QList<CuePointer> cues;
            cues.append(CuePointer(new Cue(CueType::HotCue,
                    0,
                    audio::FramePos(framesPerSecond),
                    audio::FramePos(),
                    RgbColor(qRgb(255, 0, 92)))));
            cues.append(CuePointer(new Cue(CueType::Loop,
                    Cue::kNoHotCue,
                    audio::FramePos(2 * framesPerSecond),
                    audio::FramePos(3 * framesPerSecond),
                    RgbColor(0))));
            track->setCuePoints(cues);
        }
        return track;
    }

    QSharedPointer<RekordboxExportRequest> makeRequest() {
        auto request = QSharedPointer<RekordboxExportRequest>::create();
        request->deviceRoot = QDir(m_deviceDir.path());
        request->deviceName = QStringLiteral("PARAMIX TEST");
        return request;
    }

    void runJob(RekordboxExportJob* pJob) {
        // The job loads ids and tracks with BlockingQueuedConnection on the
        // collection thread, so the test's main thread must keep processing
        // events while the worker runs.
        pJob->start();
        QElapsedTimer timer;
        timer.start();
        while (!pJob->isFinished() && timer.elapsed() < 120000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(5);
        }
        ASSERT_TRUE(pJob->isFinished());
        ASSERT_TRUE(pJob->wait(10000));
        QCoreApplication::processEvents();
    }

    bool hasVisibleTree() const {
        return QFileInfo::exists(m_deviceDir.filePath(QStringLiteral("PIONEER"))) ||
                QFileInfo::exists(m_deviceDir.filePath(QStringLiteral("Contents")));
    }

    bool hasStagingDirectory() const {
        return !QDir(m_deviceDir.path())
                        .entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot)
                        .isEmpty();
    }

    QTemporaryDir m_sourceDir;
    QTemporaryDir m_deviceDir;
};

TEST_F(RekordboxExportJobIntegrationTest, ReportsProgressAndCompletion) {
    ASSERT_TRUE(addTrack(QStringLiteral("one.wav"),
            QStringLiteral("One"),
            QStringLiteral("Artist A"),
            QStringLiteral("Album A")));
    ASSERT_TRUE(addTrack(QStringLiteral("two.wav"),
            QStringLiteral("Two"),
            QStringLiteral("Artist B"),
            QStringLiteral("Album B")));

    RekordboxExportJob job(nullptr, trackCollectionManager(), makeRequest());
    QSignalSpy maximumSpy(&job, &RekordboxExportJob::jobMaximum);
    QSignalSpy progressSpy(&job, &RekordboxExportJob::jobProgress);
    QSignalSpy completedSpy(&job, &RekordboxExportJob::completed);
    QSignalSpy failedSpy(&job, &RekordboxExportJob::failed);

    runJob(&job);

    EXPECT_EQ(0, failedSpy.count());
    ASSERT_EQ(1, completedSpy.count());
    EXPECT_EQ(2, completedSpy.at(0).at(0).toInt());
    ASSERT_EQ(1, maximumSpy.count());
    EXPECT_GE(maximumSpy.at(0).at(0).toInt(), 5);
    EXPECT_FALSE(progressSpy.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(
            m_deviceDir.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb"))));
}

TEST_F(RekordboxExportJobIntegrationTest, CancelLeavesNoTree) {
    ASSERT_TRUE(addTrack(QStringLiteral("one.wav"),
            QStringLiteral("One"),
            QStringLiteral("Artist A"),
            QStringLiteral("Album A")));
    ASSERT_TRUE(addTrack(QStringLiteral("two.wav"),
            QStringLiteral("Two"),
            QStringLiteral("Artist B"),
            QStringLiteral("Album B")));

    RekordboxExportJob job(nullptr, trackCollectionManager(), makeRequest());
    QSignalSpy completedSpy(&job, &RekordboxExportJob::completed);
    QSignalSpy failedSpy(&job, &RekordboxExportJob::failed);
    QObject::connect(&job,
            &RekordboxExportJob::jobProgress,
            &job,
            &RekordboxExportJob::slotCancel,
            Qt::DirectConnection);

    runJob(&job);

    EXPECT_EQ(0, completedSpy.count());
    EXPECT_EQ(0, failedSpy.count());
    EXPECT_FALSE(hasVisibleTree());
    EXPECT_FALSE(hasStagingDirectory());
}

TEST_F(RekordboxExportJobIntegrationTest, FreeSpaceRefusalLeavesNoTree) {
    TrackPointer track = addTrack(QStringLiteral("huge.wav"),
            QStringLiteral("Huge"),
            QStringLiteral("Artist"),
            QStringLiteral("Album"));
    ASSERT_TRUE(track);

    const QString location = track->getFileInfo().location();
    constexpr qint64 kHugeSize = 15LL * 1024 * 1024 * 1024 * 1024;
    QFile file(location);
    if (!file.open(QIODevice::ReadWrite) || !file.resize(kHugeSize)) {
        GTEST_SKIP() << "Could not create a sparse file large enough for the test";
    }
    file.close();
    const QStorageInfo storage(m_deviceDir.path());
    if (!storage.isValid() || storage.bytesAvailable() >= kHugeSize) {
        GTEST_SKIP() << "The test volume has more free space than the sparse file";
    }

    RekordboxExportJob job(nullptr, trackCollectionManager(), makeRequest());
    QSignalSpy completedSpy(&job, &RekordboxExportJob::completed);
    QSignalSpy failedSpy(&job, &RekordboxExportJob::failed);

    runJob(&job);

    EXPECT_EQ(0, completedSpy.count());
    ASSERT_EQ(1, failedSpy.count());
    EXPECT_NE(std::string::npos,
            failedSpy.at(0).at(0).toString().toStdString().find("space"));
    EXPECT_FALSE(hasVisibleTree());
    EXPECT_FALSE(hasStagingDirectory());
}

TEST_F(RekordboxExportJobIntegrationTest, FailureDuringAssemblyLeavesNoTree) {
    TrackPointer track = addTrack(QStringLiteral("broken.wav"),
            QStringLiteral("Broken"),
            QStringLiteral("Artist"),
            QStringLiteral("Album"));
    ASSERT_TRUE(track);

    // Replace the source file with a directory after the collection loaded it.
    // The preflight still sees a path that exists; the copy then fails mid-way.
    const QString location = track->getFileInfo().location();
    ASSERT_TRUE(QFile::remove(location));
    ASSERT_TRUE(QDir().mkpath(location));

    RekordboxExportJob job(nullptr, trackCollectionManager(), makeRequest());
    QSignalSpy completedSpy(&job, &RekordboxExportJob::completed);
    QSignalSpy failedSpy(&job, &RekordboxExportJob::failed);

    runJob(&job);

    EXPECT_EQ(0, completedSpy.count());
    EXPECT_EQ(1, failedSpy.count());
    EXPECT_FALSE(hasVisibleTree());
    EXPECT_FALSE(hasStagingDirectory());
}

TEST_F(RekordboxExportJobIntegrationTest, MapsContentAndAnalysisPaths) {
    ASSERT_TRUE(addTrack(QStringLiteral("one.wav"),
            QStringLiteral("Track One"),
            QStringLiteral("Artist One"),
            QStringLiteral("Album One")));
    ASSERT_TRUE(addTrack(QStringLiteral("two.wav"),
            QStringLiteral("Café Two"),
            QStringLiteral("Artist/Two"),
            QString()));

    RekordboxExportJob job(nullptr, trackCollectionManager(), makeRequest());
    QSignalSpy completedSpy(&job, &RekordboxExportJob::completed);
    QSignalSpy failedSpy(&job, &RekordboxExportJob::failed);
    runJob(&job);

    EXPECT_EQ(0, failedSpy.count());
    ASSERT_EQ(1, completedSpy.count());

    ParsedPdb pdb(readFile(
            m_deviceDir.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb"))));
    const auto rows =
            rowsOf(tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_TRACKS));
    ASSERT_EQ(2, rows.size());
    QHash<QString, QString> pathByTitle;
    for (auto* row : rows) {
        auto* trackRow = static_cast<rekordbox_pdb_t::track_row_t*>(row->body());
        const QString title = decodePdbString(trackRow->title());
        const QString filePath = decodePdbString(trackRow->file_path());
        const QString analyzePath = decodePdbString(trackRow->analyze_path());
        pathByTitle.insert(title, filePath);
        EXPECT_TRUE(QFileInfo::exists(
                m_deviceDir.filePath(filePath.mid(1))));
        EXPECT_TRUE(QFileInfo::exists(
                m_deviceDir.filePath(analyzePath.mid(1))));
        EXPECT_TRUE(analyzePath.startsWith(QStringLiteral("/PIONEER/USBANLZ/P001/")));
    }
    ASSERT_TRUE(pathByTitle.contains(QStringLiteral("Track One")));
    EXPECT_TRUE(pathByTitle.value(QStringLiteral("Track One"))
                    .startsWith(QStringLiteral("/Contents/Artist One/Album One/")));
    EXPECT_TRUE(pathByTitle.value(QStringLiteral("Track One"))
                    .endsWith(QStringLiteral(" - one.wav")));
    ASSERT_TRUE(pathByTitle.contains(QStringLiteral("Café Two")));
    EXPECT_TRUE(pathByTitle.value(QStringLiteral("Café Two"))
                    .startsWith(QStringLiteral("/Contents/Artist_Two/Unknown/")));
    EXPECT_TRUE(pathByTitle.value(QStringLiteral("Café Two"))
                    .endsWith(QStringLiteral(" - two.wav")));
}

TEST_F(RekordboxExportJobIntegrationTest, RoundTripsBeatsAndCuesThroughKaitai) {
    ASSERT_TRUE(addTrack(QStringLiteral("cue.wav"),
            QStringLiteral("Cue Track"),
            QStringLiteral("Artist"),
            QStringLiteral("Album")));

    RekordboxExportJob job(nullptr, trackCollectionManager(), makeRequest());
    QSignalSpy failedSpy(&job, &RekordboxExportJob::failed);
    runJob(&job);
    EXPECT_EQ(0, failedSpy.count());

    ParsedPdb pdb(readFile(
            m_deviceDir.filePath(QStringLiteral("PIONEER/rekordbox/export.pdb"))));
    const auto rows =
            rowsOf(tableOf(pdb.file.get(), rekordbox_pdb_t::PAGE_TYPE_TRACKS));
    ASSERT_EQ(1, rows.size());
    auto* pdbTrack = static_cast<rekordbox_pdb_t::track_row_t*>(rows.at(0)->body());
    EXPECT_EQ(QStringLiteral("Cue Track"), decodePdbString(pdbTrack->title()));
    EXPECT_EQ(12000u, pdbTrack->tempo());
    EXPECT_EQ(4u, pdbTrack->rating());

    const QString analyzePath = decodePdbString(pdbTrack->analyze_path());
    const QString filePath = decodePdbString(pdbTrack->file_path());
    ParsedAnlz anlz(readFile(m_deviceDir.filePath(analyzePath.mid(1))));

    auto* pathSection =
            anlzSection(anlz.file.get(), rekordbox_anlz_t::SECTION_TAGS_PATH);
    ASSERT_NE(nullptr, pathSection);
    EXPECT_EQ(filePath,
            decodeUtf16Be(
                    static_cast<rekordbox_anlz_t::path_tag_t*>(pathSection->body())->path()));

    auto* gridSection =
            anlzSection(anlz.file.get(), rekordbox_anlz_t::SECTION_TAGS_BEAT_GRID);
    ASSERT_NE(nullptr, gridSection);
    auto* grid = static_cast<rekordbox_anlz_t::beat_grid_tag_t*>(gridSection->body());
    ASSERT_GT(grid->num_beats(), 2u);
    EXPECT_EQ(500u, grid->beats()->at(1).get()->time());
    EXPECT_EQ(12000u, grid->beats()->at(1).get()->tempo());

    auto* hotSection =
            anlzSection(anlz.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES, 0);
    ASSERT_NE(nullptr, hotSection);
    auto* hot = static_cast<rekordbox_anlz_t::cue_tag_t*>(hotSection->body());
    EXPECT_EQ(rekordbox_anlz_t::CUE_LIST_TYPE_HOT_CUES, hot->type());
    ASSERT_EQ(1u, hot->cues()->size());
    EXPECT_EQ(1000u, hot->cues()->at(0)->time());

    auto* memorySection =
            anlzSection(anlz.file.get(), rekordbox_anlz_t::SECTION_TAGS_CUES, 1);
    ASSERT_NE(nullptr, memorySection);
    auto* memory = static_cast<rekordbox_anlz_t::cue_tag_t*>(memorySection->body());
    EXPECT_EQ(rekordbox_anlz_t::CUE_LIST_TYPE_MEMORY_CUES, memory->type());
    ASSERT_EQ(1u, memory->cues()->size());
    EXPECT_EQ(2000u, memory->cues()->at(0)->time());
    EXPECT_EQ(3000u, memory->cues()->at(0)->loop_time());
}

} // namespace
} // namespace rekordbox
} // namespace mixxx
