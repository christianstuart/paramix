#include "stems/stemfileformat.h"

#include <algorithm>

#include "preferences/configobject.h"

namespace mixxx::stems {
namespace {

constexpr char kGroup[] = "[Stems]";
constexpr char kFileFormatKey[] = "FileFormat";
constexpr char kAacBitrateKey[] = "AacBitrate";

int clampedAacBitrate(int bitrateKbps) {
    return std::clamp(bitrateKbps, kMinAacBitrateKbps, kMaxAacBitrateKbps);
}

} // namespace

StemFileFormat parseStemFileFormat(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    return StemFileFormat::Flac;
}

QString stemFileFormatName(StemFileFormat format) {
    Q_UNUSED(format);
    return QStringLiteral("flac");
}

QString stemFileExtension(StemFileFormat format) {
    Q_UNUSED(format);
    return QStringLiteral("flac");
}

bool isLosslessStemFormat(StemFileFormat format) {
    Q_UNUSED(format);
    return true;
}

StemExportSettings readStemExportSettings(UserSettingsPointer pConfig) {
    StemExportSettings settings;
    if (!pConfig) {
        return settings;
    }
    settings.format = parseStemFileFormat(pConfig->getValue(
            ConfigKey(kGroup, kFileFormatKey), QStringLiteral("flac")));
    return settings;
}

void writeStemExportSettings(
        UserSettingsPointer pConfig, const StemExportSettings& settings) {
    if (!pConfig) {
        return;
    }
    pConfig->setValue(
            ConfigKey(kGroup, kFileFormatKey), stemFileFormatName(settings.format));
}

} // namespace mixxx::stems
