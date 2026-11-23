#pragma once

#include <QMetaType>
#include <QRgb>
#include <QVector>
#include <array>
#include <cstddef>
#include <cstdint>

#include "track/trackid.h"

/// One compact column of a stem detail lane.
///
/// The shipping reducer writes the same entry to every row of the column's
/// band, so a column only needs its colour and the rows that carry it. Rows
/// outside the mask are the zero entry and stay at the lane base colour.
struct StemDetailColumn {
    /// 0xRRGGBB, opaque.
    uint32_t rgba = 0;
    /// Rows carrying the colour. The band is not necessarily contiguous.
    uint64_t mask = 0;
    bool wave = false;
};

/// One stem lane: one column per device pixel of the whole track.
struct StemDetailLane {
    QVector<StemDetailColumn> columns;
    /// Exactness escape hatch for columns whose non-zero rows hold different
    /// entries. Flat, height entries per column: rowColors[c * height + r].
    /// 0 means "no pixel on this row"; allocated on the first such column.
    QVector<QRgb> rowColors;
    int spc = 1;
    int height = 21;
    qint64 firstSample = 0;
};

/// Immutable result of one full-track reduction, shared between renderers.
struct StemDetailTrack {
    std::array<StemDetailLane, 4> lanes;
    int laneCount = 0;
    int64_t totalSamples = 0;
    uint32_t sampleRate = 0;
    TrackId trackId;
    float magnification = 1.0f;
    /// Changes whenever a new detail picture is published. Page keys include
    /// this so a rebuilt picture (for example after stem separation finishes)
    /// cannot reuse textures from the previous one.
    quint64 generation = 0;
};

/// Cache key: track, source rate, frames per device pixel, picture height and
/// reducer magnification.
struct StemDetailKey {
    TrackId trackId;
    uint32_t sampleRate = 0;
    int samplesPerColumn = 1;
    int height = 21;
    float magnification = 1.0f;
};

bool operator==(const StemDetailKey& lhs, const StemDetailKey& rhs);
bool operator!=(const StemDetailKey& lhs, const StemDetailKey& rhs);

inline size_t qHash(const StemDetailKey& key, size_t seed = 0) {
    size_t hash = qHash(key.trackId, seed);
    hash = qHash(key.sampleRate, hash);
    hash = qHash(key.samplesPerColumn, hash);
    hash = qHash(key.height, hash);
    hash = qHash(key.magnification, hash);
    return hash;
}

Q_DECLARE_METATYPE(StemDetailKey)
