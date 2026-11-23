#include "waveform/stemdetail/stempagebuilder.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "moc_stempagebuilder.cpp"
#include "util/assert.h"

namespace {

/// Expand the compact column mask into pixel rows. Callback receives
/// [rowStart, rowEnd) and the colour for that run.
template<typename Callback>
void forEachRun(const StemDetailLane& lane,
        int columnIndex,
        Callback&& callback) {
    if (columnIndex < 0 || columnIndex >= lane.columns.size()) {
        return;
    }
    const StemDetailColumn& column = lane.columns.at(columnIndex);
    if (!column.wave || column.mask == 0) {
        return;
    }
    const uint64_t rowMask = column.mask;
    const int offset = columnIndex * lane.height;
    const bool perRow = !lane.rowColors.isEmpty() &&
            lane.rowColors.at(offset + std::countr_zero(rowMask)) != 0;
    if (perRow) {
        int runStart = -1;
        QRgb runColor = 0;
        for (int row = 0; row < lane.height; ++row) {
            QRgb color = 0;
            if ((rowMask >> row) & 1u) {
                color = lane.rowColors.at(offset + row);
                if (color == 0) {
                    color = column.rgba;
                }
            }
            if (color != runColor) {
                if (runColor != 0) {
                    callback(runStart, row, runColor);
                }
                runColor = color;
                runStart = row;
            }
        }
        if (runColor != 0) {
            callback(runStart, lane.height, runColor);
        }
    } else {
        uint64_t remaining = rowMask;
        while (remaining) {
            const int start = std::countr_zero(remaining);
            remaining &= remaining - 1u;
            int end = start + 1;
            while (end < lane.height && (remaining & (uint64_t{1} << end))) {
                remaining &= ~(uint64_t{1} << end);
                ++end;
            }
            callback(start, end, column.rgba);
        }
    }
}

struct LaneTarget {
    const StemDetailLane* pLane = nullptr;
    int top = 0;
    int height = 0;
    /// Device row where detail row r starts.
    std::vector<int> rowStart;
    /// Device row where detail row r ends (exclusive).
    std::vector<int> rowEnd;
};

} // namespace

QImage buildStemPageImage(const StemDetailTrack& detail, const StemPageKey& key) {
    const int laneCount = std::clamp(key.laneCount, 0, detail.laneCount);
    const int laneHeight = key.laneDeviceHeight;
    const int pageWidth = key.pageWidth;
    if (laneCount <= 0 || laneHeight <= 0 || pageWidth <= 0) {
        return {};
    }
    const int gap = std::max(0, key.gapDevicePx);
    const int imageHeight = laneCount * laneHeight + (laneCount - 1) * gap;

    QImage image(pageWidth, imageHeight, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return {};
    }
    image.fill(0);
    if (image.bytesPerLine() != pageWidth * static_cast<qsizetype>(sizeof(QRgb))) {
        // The typed line writes below assume a tightly packed RGBA32 image.
        return {};
    }

    std::vector<LaneTarget> lanes(static_cast<std::size_t>(laneCount));
    for (int laneIndex = 0; laneIndex < laneCount; ++laneIndex) {
        const StemDetailLane& lane =
                detail.lanes[static_cast<std::size_t>(laneIndex)];
        LaneTarget& target = lanes[static_cast<std::size_t>(laneIndex)];
        target.pLane = &lane;
        target.top = laneIndex * (laneHeight + gap);
        target.height = laneHeight;
        target.rowStart.resize(static_cast<std::size_t>(lane.height) + 1);
        target.rowEnd.resize(static_cast<std::size_t>(lane.height) + 1);
        for (int row = 0; row <= lane.height; ++row) {
            target.rowStart[static_cast<std::size_t>(row)] =
                    (row * laneHeight) / lane.height;
            target.rowEnd[static_cast<std::size_t>(row)] =
                    (row * laneHeight + lane.height - 1) / lane.height;
        }
    }

    const int outputSpc = std::max(1, key.samplesPerColumn);
    const int detailSpc = std::max(1, key.detailSamplesPerColumn);
    const double firstSample =
            static_cast<double>(key.pageIndex) * pageWidth * outputSpc;

    for (int x = 0; x < pageWidth; ++x) {
        const double sample = firstSample + static_cast<double>(x) * outputSpc;
        const int detailColumn = static_cast<int>(
                std::llround(sample / detailSpc));
        for (int laneIndex = 0; laneIndex < laneCount; ++laneIndex) {
            LaneTarget& target = lanes[static_cast<std::size_t>(laneIndex)];
            const StemDetailLane& lane = *target.pLane;
            forEachRun(lane,
                    detailColumn,
                    [&](int rowStart, int rowEnd, QRgb color) {
                        int y0 = target.top +
                                target.rowStart[static_cast<std::size_t>(
                                        rowStart)];
                        int y1 = target.top +
                                target.rowEnd[static_cast<std::size_t>(rowEnd)];
                        y0 = std::clamp(y0, target.top, target.top + target.height);
                        y1 = std::clamp(y1, y0, target.top + target.height);
                        const QRgb pixel = 0xff000000u | (color & 0x00ffffffu);
                        for (int y = y0; y < y1; ++y) {
                            auto* pLine = reinterpret_cast<QRgb*>(
                                    image.scanLine(y));
                            pLine[x] = pixel;
                        }
                    });
        }
    }
    return image;
}

StemPageBuilder::StemPageBuilder(QObject* parent)
        : QObject(parent) {
    qRegisterMetaType<StemPageKey>("StemPageKey");
    qRegisterMetaType<std::shared_ptr<const StemDetailTrack>>(
            "std::shared_ptr<const StemDetailTrack>");
}

void StemPageBuilder::build(const StemPageKey& key,
        std::shared_ptr<const StemDetailTrack> detail) {
    if (!detail) {
        emit built(key, QImage());
        return;
    }
    emit built(key, buildStemPageImage(*detail, key));
}
