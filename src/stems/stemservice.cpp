#include "stems/stemservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStorageInfo>
#include <algorithm>
#include <utility>
#include <vector>

#include "library/dao/stemqueuedao.h"
#include "library/dao/stemsetdao.h"
#include "library/trackcollectionmanager.h"
#include "moc_stemservice.cpp"
#include "preferences/configobject.h"
#include "sources/audiosource.h"
#include "sources/soundsourceproxy.h"
#include "stems/demucslimiter.h"
#include "stems/stemcache.h"
#include "stems/stemfilewriter.h"
#include "stems/stemgpudetect.h"
#include "stems/stemresampler.h"
#include "stems/stemseparator.h"
#include "stems/stemseparatorfactory.h"
#include "track/track.h"
#include "util/db/dbconnectionpooled.h"
#include "util/db/dbconnectionpooler.h"
#include "util/logger.h"
#include "util/samplebuffer.h"

namespace mixxx::stems {
namespace {

std::unique_ptr<StemSeparator> createSeparator(
        const QString& path, const std::string& device, std::string* pError) {
#ifdef __STEM_SEPARATION__
    DemucsOnnxConfig config;
    config.modelPath = path.toStdString();
    config.device = device;
    return createDemucsOnnxSeparator(config, pError);
#else
    Q_UNUSED(path);
    Q_UNUSED(device);
    if (pError != nullptr) {
        *pError = "Stem separation is not available in this build.";
    }
    return nullptr;
#endif
}

QString defaultModelPath() {
    const QString relative = QStringLiteral("models/htdemucs.onnx");
    QStringList roots{
            QCoreApplication::applicationDirPath(),
            QDir::currentPath(),
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("..")),
            QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("../Resources")),
            QDir(QDir::currentPath()).filePath(QStringLiteral("..")),
            // Development builds keep the downloaded model outside the build
            // directory, so also look beside it.
            QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("../buildenv")),
            QDir(QDir::currentPath()).filePath(QStringLiteral("buildenv"))};
    for (const QString& dataLocation :
            QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        roots.append(QDir(dataLocation).filePath(QStringLiteral("paramix")));
    }
    for (const QString& root : std::as_const(roots)) {
        const QString candidate = QDir(root).filePath(relative);
        if (QFileInfo(candidate).isFile()) {
            return candidate;
        }
    }
    return {};
}

const mixxx::Logger kLogger("StemService");

constexpr char kGroup[] = "[Stems]";
constexpr char kOutputDirectoryKey[] = "OutputDirectory";
constexpr char kModelPathKey[] = "ModelPath";

/// Leave at least this much free space after a job, so a full disk does not
/// take the library database with it.
constexpr qint64 kMinFreeBytes = 1024LL * 1024 * 1024;
constexpr SINT kReadBlockFrames = 8192;

QString estimateTooLarge(qint64 estimatedBytes, qint64 availableBytes) {
    return QStringLiteral(
            "Not enough free space for the stem set: about %1 MB are needed "
            "and only %2 MB are available.")
            .arg((estimatedBytes + 999999) / 1000000)
            .arg(availableBytes / 1000000);
}

} // namespace

StemService::StemService(
        UserSettingsPointer pConfig, mixxx::DbConnectionPoolPtr pDbConnectionPool)
        : m_pConfig(std::move(pConfig)),
          m_pDbConnectionPool(std::move(pDbConnectionPool)) {
    qRegisterMetaType<StemService::Status>("mixxx::stems::StemService::Status");
    setObjectName(QStringLiteral("StemService"));
}

StemService::~StemService() {
    {
        QMutexLocker locker(&m_mutex);
        m_stopRequested = true;
        m_jobs.clear();
        // A running job polls this and unwinds at the next output block.
        m_cancelRequested = true;
        m_waitCondition.wakeAll();
    }
    wait();
}

// static
StemService* StemService::createInstance(
        UserSettingsPointer pConfig, mixxx::DbConnectionPoolPtr pDbConnectionPool) {
    return Singleton<StemService>::createInstance(
            std::move(pConfig), std::move(pDbConnectionPool));
}

bool StemService::queueExport(const StemExportRequest& request) {
    if (!request.track || !request.track->getId().isValid()) {
        return false;
    }
    const TrackId trackId = request.track->getId();
    {
        QMutexLocker locker(&m_mutex);
        for (const auto& job : std::as_const(m_jobs)) {
            if (job.trackId == trackId) {
                return false;
            }
        }
    }
    m_cancelRequested = false;
    const StemExportSettings settings = readStemExportSettings(m_pConfig);
    const QString outputDirectory = request.outputDirectory.trimmed().isEmpty()
            ? defaultOutputDirectory()
            : request.outputDirectory;
    const QString setName = request.setName.trimmed().isEmpty()
            ? uniqueStemSetName(outputDirectory, defaultSetName(request.track))
            : sanitizeStemSetName(request.setName);
    QString modelPathValue = m_pConfig->getValue(
                                              ConfigKey(kGroup, kModelPathKey), QString())
                                     .trimmed();
    if (modelPathValue.isEmpty()) {
        modelPathValue = defaultModelPath();
    }
    StemQueueRecord queued;
    queued.trackId = trackId;
    queued.outputDirectory = outputDirectory;
    queued.setName = setName;
    queued.format = stemFileFormatName(settings.format);
    queued.aacBitrateKbps = 0;
    // Live outside the lock: it may open a database connection on this thread.
    const QSqlDatabase database = mixxx::DbConnectionPooled(m_pDbConnectionPool);
    if (database.isOpen()) {
        StemQueueDao(database).upsert(queued);
    }
    {
        QMutexLocker locker(&m_mutex);
        for (const auto& job : std::as_const(m_jobs)) {
            if (job.trackId == trackId) {
                return false;
            }
        }
        m_jobs.enqueue(Job{request,
                trackId,
                outputDirectory,
                setName,
                modelPathValue,
                isGpuAccelerationEnabled(m_pConfig) && !m_gpuDisabled.load(),
                settings});
    }
    setStatus(trackId, Status::Queued, 0);
    m_waitCondition.wakeAll();
    if (!isRunning()) {
        // Low priority keeps the GUI, waveform and audio callback responsive.
        start(kWorkerPriority);
    }
    return true;
}

void StemService::pause() {
    m_userPaused = true;
    m_paused = true;
}

void StemService::resume() {
    m_userPaused = false;
    // A background pause (e.g. audio overload) survives an explicit resume.
    m_paused = m_backgroundPaused.load();
    m_waitCondition.wakeAll();
}

bool StemService::isPaused() const {
    return m_paused.load();
}

void StemService::setBackgroundPaused(bool paused) {
    m_backgroundPaused = paused;
    m_paused = m_userPaused.load() || paused;
    if (!m_paused.load()) {
        m_waitCondition.wakeAll();
    }
}

bool StemService::hasPendingWork() const {
    return m_busy.load();
}

void StemService::cancelAll() {
    m_cancelRequested = true;
    QQueue<Job> canceled;
    {
        QMutexLocker locker(&m_mutex);
        canceled.swap(m_jobs);
    }
    while (!canceled.isEmpty()) {
        setStatus(canceled.dequeue().trackId, Status::NotSeparated, 0);
    }
    const QSqlDatabase database = mixxx::DbConnectionPooled(m_pDbConnectionPool);
    if (database.isOpen()) {
        StemQueueDao(database).clear();
    }
    m_waitCondition.wakeAll();
}

void StemService::restoreQueuedJobs(TrackCollectionManager* pTrackCollectionManager) {
    if (pTrackCollectionManager == nullptr) {
        return;
    }
    const QSqlDatabase database = mixxx::DbConnectionPooled(m_pDbConnectionPool);
    if (!database.isOpen()) {
        return;
    }
    StemQueueDao queueDao(database);
    const QList<StemQueueRecord> records = queueDao.loadAll();
    for (const auto& record : records) {
        TrackPointer pTrack = pTrackCollectionManager->getTrackById(record.trackId);
        if (!pTrack) {
            queueDao.remove(record.trackId);
            continue;
        }
        StemExportRequest request;
        request.track = pTrack;
        request.outputDirectory = record.outputDirectory;
        request.setName = record.setName;
        queueExport(request);
    }
}

void StemService::removeStems(TrackId trackId) {
    const QSqlDatabase database = mixxx::DbConnectionPooled(m_pDbConnectionPool);
    if (!database.isOpen() || !trackId.isValid()) {
        return;
    }
    StemSetDao dao(database);
    const QList<StemSetRecord> records = dao.loadForTrack(trackId);
    for (const auto& record : records) {
        const QString setPath = QDir(record.directory).filePath(record.setName);
        // Never delete a directory that is not recognisably a stem set.
        if (!QFileInfo(QDir(setPath).filePath(QStringLiteral("stem-set.json"))).exists()) {
            kLogger.warning() << "Skipping non-stem directory" << setPath;
            continue;
        }
        if (!QDir(setPath).removeRecursively()) {
            kLogger.warning() << "Failed to remove stem set" << setPath;
        }
    }
    StemSetDao::deleteForTracks(database, QList<TrackId>{trackId});
    StemCache::removeCache(trackId);
    setStatus(trackId, Status::NotSeparated, 0, QStringLiteral("Stems removed."));
}

StemService::Status StemService::statusForTrack(TrackId trackId) const {
    {
        QMutexLocker locker(&m_mutex);
        const auto it = m_statuses.constFind(trackId);
        if (it != m_statuses.constEnd() && it.value() != Status::Ready) {
            return it.value();
        }
    }
    // No live status: an exported set may still be on disk from an earlier
    // session. The row is a rebuildable index, so this is a cheap lookup.
    const QSqlDatabase database = mixxx::DbConnectionPooled(m_pDbConnectionPool);
    if (!database.isOpen()) {
        return Status::NotSeparated;
    }
    const QList<StemSetRecord> records = StemSetDao(database).loadForTrack(trackId);
    if (records.isEmpty()) {
        return Status::NotSeparated;
    }
    for (const auto& record : records) {
        if (QFileInfo(QDir(record.directory).filePath(record.setName)).exists()) {
            return Status::Ready;
        }
    }
    return Status::MissingStemFile;
}

QString StemService::lastMessageForTrack(TrackId trackId) const {
    QMutexLocker locker(&m_mutex);
    return m_messages.value(trackId);
}

void StemService::setStatus(
        TrackId trackId, Status status, int progress, const QString& message) {
    bool busyChangedNow = false;
    bool busy = false;
    {
        QMutexLocker locker(&m_mutex);
        if (status == Status::NotSeparated) {
            m_statuses.remove(trackId);
        } else {
            m_statuses.insert(trackId, status);
        }
        m_messages.insert(trackId, message);
        busy = std::any_of(m_statuses.cbegin(),
                m_statuses.cend(),
                [](Status trackStatus) {
                    return trackStatus == Status::Queued ||
                            trackStatus == Status::Separating;
                });
        busyChangedNow = busy != m_busy.load();
        m_busy = busy;
    }
    emit trackStatusChanged(trackId, status, progress, message);
    if (busyChangedNow) {
        emit busyChanged(busy);
    }
}

QSet<TrackId> StemService::trackIdsWithStemSets() const {
    QSet<TrackId> trackIds;
    if (!m_pDbConnectionPool) {
        return trackIds;
    }
    const QSqlDatabase database = mixxx::DbConnectionPooled(m_pDbConnectionPool);
    if (!database.isOpen()) {
        return trackIds;
    }
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT DISTINCT track_id FROM stem_set"))) {
        return trackIds;
    }
    while (query.next()) {
        trackIds.insert(TrackId(query.value(0)));
    }
    return trackIds;
}

QString StemService::defaultOutputDirectory() const {
    return m_pConfig->getValue(ConfigKey(kGroup, kOutputDirectoryKey),
            QDir(m_pConfig->getSettingsPath()).filePath(QStringLiteral("Stems")));
}

QString StemService::defaultSetName(const TrackPointer& pTrack) const {
    const QString title = pTrack->getTitle();
    return title.trimmed().isEmpty() ? QStringLiteral("stems") : title;
}

void StemService::run() {
    // A worker-owned connection, because TrackCollection is main-thread only.
    mixxx::DbConnectionPooler connectionPooler(m_pDbConnectionPool);
    const QSqlDatabase daoConnection = mixxx::DbConnectionPooled(m_pDbConnectionPool);
    StemSetDao dao(daoConnection);
    StemQueueDao queueDao(daoConnection);

    std::unique_ptr<StemSeparator> pSeparator;
    QString loadedModelPath;
    QString loadedDevice;

    while (true) {
        Job job;
        {
            QMutexLocker locker(&m_mutex);
            while (!m_stopRequested && (m_jobs.isEmpty() || m_paused.load())) {
                m_waitCondition.wait(&m_mutex, 200);
            }
            if (m_stopRequested) {
                return;
            }
            job = m_jobs.dequeue();
        }
        processJob(job, &pSeparator, &loadedModelPath, &loadedDevice, dao);
        // The job is finished either way; only a crash leaves the row for the
        // next start to resume.
        queueDao.remove(job.trackId);
    }
}

void StemService::processJob(Job& job,
        std::unique_ptr<StemSeparator>* ppSeparator,
        QString* pLoadedModelPath,
        QString* pLoadedDevice,
        StemSetDao& dao) {
    const TrackId trackId = job.trackId;
    const TrackPointer pTrack = job.request.track;
    if (!pTrack) {
        setStatus(trackId, Status::Failed, 0, QStringLiteral("The track is gone."));
        return;
    }

    const StemExportSettings& settings = job.settings;
    const QString& outputDirectory = job.outputDirectory;
    const QString setName = job.setName;
    const QString setPath = QDir(outputDirectory).filePath(setName);

    if (QDir(setPath).exists()) {
        setStatus(trackId,
                Status::Failed,
                0,
                QStringLiteral("A stem set named %1 already exists.").arg(setName));
        return;
    }

    // Free-space preflight. The estimate is deliberately generous: FLAC and
    // ALAC are 24-bit, AAC scales with the requested bitrate.
    qint64 estimatedBytes = 0;
    {
        const int estimatedSampleRate = pTrack->getSampleRate().isValid()
                ? pTrack->getSampleRate().value()
                : 44100;
        const double frames = pTrack->getDuration() * estimatedSampleRate;
        estimatedBytes = static_cast<qint64>(frames * kStemChannelCount * 3.0);
        estimatedBytes = static_cast<qint64>(estimatedBytes * 1.05) + 4096;
    }
    const QString existingAncestor = QFileInfo(outputDirectory).absolutePath();
    const QStorageInfo storage(existingAncestor);
    if (storage.isValid() && storage.isReady() &&
            storage.bytesAvailable() - estimatedBytes < kMinFreeBytes) {
        setStatus(trackId,
                Status::Failed,
                0,
                estimateTooLarge(estimatedBytes, storage.bytesAvailable()));
        return;
    }

    const QString path = job.modelPath;
    if (path.isEmpty()) {
        setStatus(trackId,
                Status::Failed,
                0,
                QStringLiteral("No stem separation model is selected in Settings -> Stems."));
        return;
    }
    // `auto` resolves against the runtime's actual provider list; a provider
    // that is missing or fails to initialise degrades to CPU.
    const bool useGpu = job.gpuAcceleration && !m_gpuDisabled.load();
    const std::string device = stemDeviceForGpuAcceleration(useGpu);
    const QString deviceName = QString::fromStdString(device);
    if (!*ppSeparator || *pLoadedModelPath != path || *pLoadedDevice != deviceName) {
        ppSeparator->reset();
        std::string error;
        *ppSeparator = createSeparator(path, device, &error);
        if (!*ppSeparator) {
            setStatus(trackId,
                    Status::Failed,
                    0,
                    QStringLiteral("Could not load the stem separation model: %1")
                            .arg(QString::fromStdString(error)));
            return;
        }
        *pLoadedModelPath = path;
        *pLoadedDevice = deviceName;
        kLogger.info() << "Stem separator ready for" << path << "on"
                       << QString::fromStdString((*ppSeparator)->device());
    }

    const int modelSampleRate = (*ppSeparator)->sampleRate();

    setStatus(trackId, Status::Separating, 0);
    SoundSourceProxy proxy(pTrack);
    mixxx::AudioSource::OpenParams openParams;
    openParams.setChannelCount(mixxx::audio::ChannelCount::stereo());
    // Open at the source's native rate; the resampler below brings it to the
    // model rate and back, keeping the export/cache at the source rate.
    const mixxx::AudioSourcePointer pSource = proxy.openAudioSource(openParams);
    if (!pSource) {
        setStatus(trackId,
                Status::Failed,
                0,
                QStringLiteral("Could not open the track for reading."));
        return;
    }
    const int sourceSampleRate = pSource->getSignalInfo().getSampleRate().value();
    if (sourceSampleRate <= 0) {
        setStatus(trackId,
                Status::Failed,
                0,
                QStringLiteral("The track has an invalid sample rate."));
        return;
    }

    const SINT channels = pSource->getSignalInfo().getChannelCount().value();
    if (channels != 1 && channels != 2) {
        setStatus(trackId,
                Status::Failed,
                0,
                QStringLiteral("Stem separation needs mono or stereo audio."));
        return;
    }

    const SINT sourceFrames = pSource->frameLength();
    if (sourceFrames <= 0) {
        setStatus(trackId,
                Status::Failed,
                0,
                QStringLiteral("The track contains no audio."));
        return;
    }
    kLogger.info() << "Separating track" << trackId << "at" << sourceSampleRate
                   << "Hz," << sourceFrames << "frames; model expects"
                   << modelSampleRate << "Hz";

    std::vector<float> planar(
            static_cast<std::size_t>(2) * static_cast<std::size_t>(sourceFrames), 0.0f);
    {
        mixxx::SampleBuffer chunk(static_cast<SINT>(
                static_cast<std::size_t>(kReadBlockFrames) * channels));
        for (SINT offset = 0; offset < sourceFrames;) {
            if (m_cancelRequested.load()) {
                setStatus(trackId, Status::NotSeparated, 0, QStringLiteral("Canceled."));
                return;
            }
            const SINT count = std::min<SINT>(kReadBlockFrames, sourceFrames - offset);
            const auto readable = pSource->readSampleFrames(mixxx::WritableSampleFrames(
                    mixxx::IndexRange::forward(offset, count),
                    mixxx::SampleBuffer::WritableSlice(chunk)));
            const SINT read = readable.frameLength();
            if (read <= 0) {
                setStatus(trackId,
                        Status::Failed,
                        0,
                        QStringLiteral("Could not read the track audio."));
                return;
            }
            for (SINT frame = 0; frame < read; ++frame) {
                const std::size_t sourceIndex =
                        static_cast<std::size_t>(frame) * channels;
                const std::size_t destinationIndex =
                        static_cast<std::size_t>(offset + frame);
                // The separator takes planar [channels][frames], not interleaved
                // stereo. Interleaving here time-squeezes the whole track.
                planar[destinationIndex] = readable.readableData()[sourceIndex];
                planar[sourceFrames + destinationIndex] = channels == 1
                        ? readable.readableData()[sourceIndex]
                        : readable.readableData()[sourceIndex + 1];
            }
            offset += read;
        }
    }
    pSource->close();

    std::vector<float> modelPlanar;
    const float* pModelInput = planar.data();
    SINT modelFrames = sourceFrames;
    if (sourceSampleRate != modelSampleRate) {
        if (!resamplePlanar(planar.data(),
                    2,
                    sourceFrames,
                    sourceSampleRate,
                    modelSampleRate,
                    &modelPlanar,
                    &modelFrames)) {
            setStatus(trackId,
                    Status::Failed,
                    0,
                    QStringLiteral("Could not resample the track for separation."));
            return;
        }
        pModelInput = modelPlanar.data();
        std::vector<float>().swap(planar);
    }

    std::vector<CSAMPLE> separated(
            static_cast<std::size_t>(modelFrames) * kStemChannelCount, 0.0f);
    bool outputValid = true;
    SINT outputEnd = 0;
    const auto runSeparation = [&]() {
        outputValid = true;
        outputEnd = 0;
        return (*ppSeparator)->separate(pModelInput, modelFrames, [&](SINT offset, const CSAMPLE* pBuffer, SINT count) {
                    if (offset < 0 || count <= 0 || offset + count > modelFrames) {
                        outputValid = false;
                        return false;
                    }
                    outputEnd = std::max(outputEnd, offset + count);
                    std::copy(pBuffer,
                            pBuffer + static_cast<std::size_t>(count) * kStemChannelCount,
                            separated.begin() + static_cast<std::size_t>(offset) *
                                            kStemChannelCount);
                    return true; }, [this, trackId](double progress) { setStatus(trackId,
                                                                                                                                                                       Status::Separating,
                                                                                                                                                                       static_cast<int>(std::clamp(progress, 0.0, 1.0) * 90.0)); }, [this]() {
                    if (m_cancelRequested.load()) {
                        return true;
                    }
                    while (m_paused.load() && !m_cancelRequested.load()) {
                        QThread::msleep(50);
                    }
                    return m_cancelRequested.load(); });
    };
    bool completed = runSeparation();
    // A provider that initialized but cannot execute this graph fails on Run,
    // not on session creation. Fall back to the CPU once for this job and for
    // the rest of the process.
    if (!completed && !m_cancelRequested.load() &&
            (*ppSeparator)->device() != "cpu") {
        kLogger.warning() << "Stem separation failed on provider"
                          << QString::fromStdString((*ppSeparator)->device()) << "for track"
                          << trackId << ":"
                          << QString::fromStdString((*ppSeparator)->lastError())
                          << "- retrying on the CPU";
        m_gpuDisabled = true;
        ppSeparator->reset();
        std::string error;
        *ppSeparator = createSeparator(path, "cpu", &error);
        *pLoadedDevice = QStringLiteral("cpu");
        if (!*ppSeparator) {
            setStatus(trackId,
                    Status::Failed,
                    0,
                    QStringLiteral("Stem separation failed and the CPU fallback "
                                   "could not be loaded: %1")
                            .arg(QString::fromStdString(error)));
            return;
        }
        // The failed run may have filled part of the buffer before it threw.
        std::fill(separated.begin(), separated.end(), 0.0f);
        completed = runSeparation();
    }
    if (!completed || !outputValid) {
        if (m_cancelRequested.load()) {
            setStatus(trackId, Status::NotSeparated, 0, QStringLiteral("Canceled."));
        } else {
            setStatus(trackId,
                    Status::Failed,
                    0,
                    QStringLiteral("Stem separation failed: %1")
                            .arg(QString::fromStdString((*ppSeparator)->lastError())));
        }
        return;
    }
    kLogger.info() << "Separator covered frames" << outputEnd << "of" << modelFrames;
    std::vector<float>().swap(modelPlanar);

    std::vector<CSAMPLE> finalStems;
    if (sourceSampleRate != modelSampleRate) {
        std::vector<float> modelPlanarOut(
                static_cast<std::size_t>(modelFrames) * kStemChannelCount);
        for (SINT frame = 0; frame < modelFrames; ++frame) {
            for (int channel = 0; channel < kStemChannelCount; ++channel) {
                modelPlanarOut[static_cast<std::size_t>(channel) * modelFrames +
                        frame] =
                        separated[static_cast<std::size_t>(frame) * kStemChannelCount +
                                channel];
            }
        }
        std::vector<float> sourcePlanar;
        SINT sourceOutputFrames = 0;
        if (!resamplePlanar(modelPlanarOut.data(),
                    kStemChannelCount,
                    modelFrames,
                    modelSampleRate,
                    sourceSampleRate,
                    &sourcePlanar,
                    &sourceOutputFrames)) {
            setStatus(trackId,
                    Status::Failed,
                    0,
                    QStringLiteral("Could not resample the separated stems."));
            return;
        }
        finalStems.assign(
                static_cast<std::size_t>(sourceFrames) * kStemChannelCount, 0.0f);
        const SINT copyFrames = std::min(sourceFrames, sourceOutputFrames);
        for (SINT frame = 0; frame < copyFrames; ++frame) {
            for (int channel = 0; channel < kStemChannelCount; ++channel) {
                finalStems[static_cast<std::size_t>(frame) * kStemChannelCount +
                        channel] =
                        sourcePlanar[static_cast<std::size_t>(channel) *
                                        sourceOutputFrames +
                                frame];
            }
        }
    } else {
        finalStems = std::move(separated);
    }

    // One scalar gain over all stems and their sum keeps the mix reconstructible
    // and inside [-1, 1]; the writer then encodes without clipping.
    const float gain = demucsLimiterGain(finalStems.data(), sourceFrames);
    if (gain < 1.0f) {
        demucsApplyGain(finalStems.data(), finalStems.size(), gain);
    }

    setStatus(trackId, Status::Separating, 95);
    StemSetInfo info;
    info.baseName = setName;
    info.modelName = QFileInfo(path).completeBaseName();
    info.sampleRate = sourceSampleRate;
    info.frames = sourceFrames;
    info.format = settings.format;
    QString error;
    if (!writeStemSet(outputDirectory, info, finalStems.data(), &error)) {
        setStatus(trackId, Status::Failed, 0, error);
        return;
    }

    // The export is the user-facing artifact; the 8-channel cache is what the
    // decks and the waveform renderer consume.
    QString cacheError;
    if (!StemCache::writeCache(trackId,
                finalStems.data(),
                sourceFrames,
                sourceSampleRate,
                &cacheError)) {
        kLogger.warning() << "Stem export written but cache failed for track"
                          << trackId << cacheError;
    } else {
        emit stemsReady(trackId);
    }

    StemSetRecord record;
    record.trackId = trackId;
    record.setName = setName;
    record.directory = QDir(outputDirectory).absolutePath();
    record.format = stemFileFormatName(settings.format);
    record.model = info.modelName;
    record.sampleRate = sourceSampleRate;
    record.frameCount = sourceFrames;
    record.aacBitrateKbps = 0;
    if (!dao.insert(record)) {
        kLogger.warning() << "Stem set written but not indexed for track" << trackId;
    }

    setStatus(trackId, Status::Ready, 100);
}

} // namespace mixxx::stems
