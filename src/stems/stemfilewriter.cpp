#include "stems/stemfilewriter.h"

#include <algorithm>
#include <array>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#endif
#include <sndfile.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>
#include <cerrno>
#include <cstring>

#include "stems/stemseparatorbackend.h"
#include "util/assert.h"

namespace mixxx::stems {
namespace {

constexpr int kBlockFrames = 8192;
constexpr char kSidecarName[] = "stem-set.json";

constexpr std::array<const char*, kStemCount> kStemNames = {
        "drums", "bass", "other", "vocals"};
constexpr std::array<const char*, kStemCount> kStemColors = {
        "#C13BD6", "#F08A24", "#22C7D6", "#2FBF71"};

void setError(QString* pError, const QString& message) {
    if (pError != nullptr) {
        *pError = message;
    }
}

SNDFILE* openSndFileForWrite(const QString& path, SF_INFO* pInfo) {
#ifdef _WIN32
    return sf_wchar_open(path.toStdWString().c_str(), SFM_WRITE, pInfo);
#else
    return sf_open(path.toUtf8().constData(), SFM_WRITE, pInfo);
#endif
}

bool writeBlockToSndFile(SNDFILE* pFile,
        const CSAMPLE* pStems,
        SINT frames,
        int stem,
        QString* pError) {
    std::vector<CSAMPLE> block(static_cast<std::size_t>(kBlockFrames) * 2);
    for (SINT offset = 0; offset < frames; offset += kBlockFrames) {
        const SINT count = std::min<SINT>(kBlockFrames, frames - offset);
        for (SINT frame = 0; frame < count; ++frame) {
            block[static_cast<std::size_t>(frame) * 2] =
                    pStems[(offset + frame) * kStemChannelCount + stem * 2];
            block[static_cast<std::size_t>(frame) * 2 + 1] =
                    pStems[(offset + frame) * kStemChannelCount + stem * 2 + 1];
        }
        const sf_count_t expected = count * 2;
        if (sf_write_float(pFile, block.data(), expected) != expected) {
            setError(pError,
                    QStringLiteral("Could not write stem audio: %1")
                            .arg(QString::fromUtf8(sf_strerror(pFile))));
            return false;
        }
    }
    return true;
}

bool writeSndFileStem(const QString& path,
        const CSAMPLE* pStems,
        SINT frames,
        int stem,
        int sampleRate,
        int format,
        QString* pError) {
    SF_INFO info{};
    info.samplerate = sampleRate;
    info.channels = 2;
    info.format = format;

    SNDFILE* pFile = openSndFileForWrite(path, &info);
    if (pFile == nullptr) {
        // sf_strerror(nullptr) only gives a generic message; prefer the OS
        // error the failed open reported.
        const int openError = errno;
        const QString reason = openError != 0
                ? QString::fromLocal8Bit(std::strerror(openError))
                : QString::fromUtf8(sf_strerror(nullptr));
        setError(pError, QStringLiteral("Could not create %1: %2").arg(path, reason));
        return false;
    }

    sf_command(pFile, SFC_SET_NORM_FLOAT, nullptr, SF_TRUE);
    sf_command(pFile, SFC_SET_CLIPPING, nullptr, SF_TRUE);

    const bool written = writeBlockToSndFile(pFile, pStems, frames, stem, pError);
    if (sf_close(pFile) != 0 && written) {
        setError(pError, QStringLiteral("Could not finalize %1").arg(path));
        return false;
    }
    return written;
}

bool writeSidecar(const QString& path,
        const StemSetInfo& info,
        QString* pError) {
    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("model")] = info.modelName;
    root[QStringLiteral("format")] = stemFileFormatName(info.format);
    root[QStringLiteral("lossless")] = isLosslessStemFormat(info.format);
    root[QStringLiteral("sample_rate")] = info.sampleRate;
    root[QStringLiteral("frames")] = static_cast<qint64>(info.frames);

    QJsonArray sources;
    const QString extension = stemFileExtension(info.format);
    for (int stem = 0; stem < kStemCount; ++stem) {
        QJsonObject source;
        source[QStringLiteral("name")] = QString::fromLatin1(kStemNames[stem]);
        source[QStringLiteral("file")] =
                QStringLiteral("%1.%2")
                        .arg(QString::fromLatin1(kStemNames[stem]), extension);
        source[QStringLiteral("color")] = QString::fromLatin1(kStemColors[stem]);
        sources.append(source);
    }
    root[QStringLiteral("sources")] = sources;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(pError, QStringLiteral("Could not create %1").arg(path));
        return false;
    }
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        setError(pError, QStringLiteral("Could not write %1").arg(path));
        return false;
    }
    return true;
}

} // namespace

QString sanitizeStemSetName(const QString& name) {
    QString result = name.trimmed();
    static const QRegularExpression invalid(
            QStringLiteral("[\\\\/:*?\"<>|]"));
    result.replace(invalid, QStringLiteral("_"));
    while (result.endsWith(QLatin1Char('.')) ||
            result.endsWith(QLatin1Char(' '))) {
        result.chop(1);
    }
    if (result.isEmpty()) {
        return QStringLiteral("stems");
    }
    return result;
}

QString uniqueStemSetName(const QString& outputDirectory, const QString& baseName) {
    const QString sanitized = sanitizeStemSetName(baseName);
    QDir outputDir(outputDirectory);
    if (!outputDir.exists(sanitized)) {
        return sanitized;
    }
    for (int suffix = 2; suffix < 10000; ++suffix) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(sanitized).arg(suffix);
        if (!outputDir.exists(candidate)) {
            return candidate;
        }
    }
    return sanitized;
}

bool writeStemSet(const QString& outputDirectory,
        const StemSetInfo& info,
        const CSAMPLE* pInterleavedStems,
        QString* pError) {
    if (pInterleavedStems == nullptr) {
        setError(pError, QStringLiteral("No stem audio was provided"));
        return false;
    }
    if (info.frames <= 0) {
        setError(pError, QStringLiteral("There are no frames to write"));
        return false;
    }
    if (info.sampleRate <= 0) {
        setError(pError, QStringLiteral("The sample rate is invalid"));
        return false;
    }
    if (outputDirectory.trimmed().isEmpty()) {
        setError(pError, QStringLiteral("No output folder was provided"));
        return false;
    }

    const QString name = sanitizeStemSetName(info.baseName);
    QDir outputDir(outputDirectory);
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        setError(pError,
                QStringLiteral("Could not create the output folder %1")
                        .arg(outputDirectory));
        return false;
    }
    if (outputDir.exists(name)) {
        setError(pError,
                QStringLiteral("A stem set named %1 already exists").arg(name));
        return false;
    }

    const QString tempName = QStringLiteral(".%1.tmp-%2")
                                     .arg(name,
                                             QUuid::createUuid().toString(
                                                     QUuid::WithoutBraces));
    if (!outputDir.mkdir(tempName)) {
        setError(pError,
                QStringLiteral("Could not create a temporary folder in %1")
                        .arg(outputDirectory));
        return false;
    }
    const QString tempPath = outputDir.filePath(tempName);

    const int sndFileFormat = SF_FORMAT_FLAC | SF_FORMAT_PCM_24;
    const QString extension = stemFileExtension(info.format);

    bool written = true;
    for (int stem = 0; stem < kStemCount && written; ++stem) {
        const QString path = QDir(tempPath).filePath(
                QStringLiteral("%1.%2")
                        .arg(QString::fromLatin1(kStemNames[stem]), extension));
        written = writeSndFileStem(path,
                pInterleavedStems,
                info.frames,
                stem,
                info.sampleRate,
                sndFileFormat,
                pError);
    }

    if (written) {
        written = writeSidecar(QDir(tempPath).filePath(
                                       QString::fromLatin1(kSidecarName)),
                info,
                pError);
    }

    if (written) {
        if (!outputDir.rename(tempName, name)) {
            setError(pError,
                    QStringLiteral("Could not move the stem set into place"));
            written = false;
        }
    }

    if (!written) {
        QDir(tempPath).removeRecursively();
    }
    return written;
}

} // namespace mixxx::stems
