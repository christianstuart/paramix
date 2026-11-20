#pragma once

#include <QDir>
#include <QSet>
#include <QString>

#include "library/trackset/crate/crateid.h"

namespace mixxx {

/// A request to export the library to a rekordbox USB/SD device tree.
///
/// The target directory is the device root. The job writes `PIONEER/` and
/// `Contents/` below it, and refuses to overwrite either directory.
struct RekordboxExportRequest {
    /// Device root, for example `E:/`.
    QDir deviceRoot;

    /// Name shown by a player as the device name. Empty uses the volume name.
    QString deviceName;

    /// Crates and playlists to export. An empty set of both exports the whole
    /// library and every non-hidden playlist.
    QSet<CrateId> crateIdsToExport;
    QSet<int> playlistIdsToExport;
};

} // namespace mixxx
