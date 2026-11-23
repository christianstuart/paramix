#pragma once

#include <QMetaType>
#include <QString>
#include <cstddef>

#include "track/trackid.h"

/// One fixed-width page of the stem waveform picture.
///
/// Pages are pure content: natural stem order, full lane alpha and full lane
/// height. Mute, opacity, slip and lane order are applied by the renderer, so
/// changing them never invalidates a page.
///
/// `pageIndex` addresses `pageWidth` output columns. One output column is one
/// device pixel, so a page is immutable for a given detail picture, zoom and
/// lane height.
struct StemPageKey {
    TrackId trackId;
    /// Source sample rate of the detail picture.
    quint32 sampleRate = 0;
    /// Frames per detail column of the source picture.
    int detailSamplesPerColumn = 1;
    /// Detail row count the picture was reduced with.
    int detailLaneHeight = 1;
    /// Frames per output column (device pixel) at render time.
    int samplesPerColumn = 1;
    /// Output lane height in device pixels.
    int laneDeviceHeight = 1;
    int laneCount = 1;
    /// Device pixels between two output lanes.
    int gapDevicePx = 1;
    int pageWidth = 4096;
    int pageIndex = 0;
    /// Magnification the detail picture was reduced with.
    float magnification = 1.0f;
    /// Generation of the detail picture. A rebuilt picture gets a new value so
    /// page caches never serve content from the previous one.
    quint64 detailGeneration = 0;

    bool operator==(const StemPageKey&) const = default;
};

inline size_t qHash(const StemPageKey& key, size_t seed = 0) {
    size_t hash = qHash(key.trackId, seed);
    hash = qHash(key.sampleRate, hash);
    hash = qHash(key.detailSamplesPerColumn, hash);
    hash = qHash(key.detailLaneHeight, hash);
    hash = qHash(key.samplesPerColumn, hash);
    hash = qHash(key.laneDeviceHeight, hash);
    hash = qHash(key.laneCount, hash);
    hash = qHash(key.gapDevicePx, hash);
    hash = qHash(key.pageWidth, hash);
    hash = qHash(key.pageIndex, hash);
    hash = qHash(key.magnification, hash);
    hash = qHash(key.detailGeneration, hash);
    return hash;
}

Q_DECLARE_METATYPE(StemPageKey)
