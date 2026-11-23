#include "waveform/stemdetail/stemdetailbuilder.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

#include "analyzer/constants.h"
#include "moc_stemdetailbuilder.cpp"
#include "sources/audiosource.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/logger.h"
#include "util/samplebuffer.h"
#include "waveform/stemdetail/stemdetailcolors.h"
#include "waveform/stemdetail/stemdetailreducer.h"

namespace {

const mixxx::Logger kLogger("StemDetailBuilder");

std::atomic<quint64> s_nextDetailGeneration{1};

constexpr int kMaxStems = mixxx::kMaxSupportedStems;
// Decode larger blocks than the analysis pipeline's default chunk. The detail
// builder is a pure sample-by-sample reducer, so block boundaries have no
// effect on the result, but fewer reads cut per-chunk overhead on long tracks.
constexpr SINT kReadFramesPerChunk = mixxx::kAnalysisFramesPerChunk * 4;

/// Upper bound for a full-track reduction. A column holds up to `height`
/// per-row colours, so this caps one lane at roughly 64 MB (four lanes at
/// 256 MB); deeper zoom levels keep the previous picture until the windowed
/// rebuild (M5).
constexpr int64_t kMaxCachedPixels = 16000000;

bool isZero(const StemDetailEntry& entry) {
    return entry.coverage == 0.0f && entry.low == 0.0f && entry.mid == 0.0f &&
            entry.high == 0.0f;
}

uint32_t toRgb24(QRgb color) {
    return static_cast<uint32_t>(color & 0x00ffffffu);
}

struct StemStaging {
    std::vector<float> left;
    std::vector<float> right;
};

/// Append one compact column, keeping the per-row image only when the non-zero
/// rows actually quantise to more than one pixel colour.
void appendColumn(StemDetailLane* pLane,
        const std::vector<StemDetailEntry>& entries,
        int height,
        int columnCount,
        const StemDetailColorParams& params) {
    DEBUG_ASSERT(height <= 63);
    uint64_t mask = 0;
    std::array<QRgb, 63> rowColors{};
    uint32_t firstColor = 0;
    bool haveColor = false;
    bool uniform = true;
    for (int row = 0; row < height; ++row) {
        const StemDetailEntry& entry = entries[static_cast<std::size_t>(row)];
        if (isZero(entry)) {
            continue;
        }
        mask |= uint64_t{1} << row;
        const QRgb color = stemDetailEntryColor(entry, params);
        rowColors[static_cast<std::size_t>(row)] = color;
        const uint32_t rgb = toRgb24(color);
        if (!haveColor) {
            firstColor = rgb;
            haveColor = true;
        } else if (rgb != firstColor) {
            uniform = false;
        }
    }

    StemDetailColumn column;
    const int columnIndex = pLane->columns.size();
    if (!haveColor) {
        pLane->columns.append(column);
        return;
    }
    column.wave = true;
    column.mask = mask;
    if (uniform) {
        // The shipping reducer writes one entry to the whole band, but even
        // when the float entries differ the quantised pixel colour is usually
        // identical. Deciding on the pixel colour keeps the compact form (and
        // avoids the whole-track rowColors arena) without changing a pixel.
        column.rgba = firstColor;
    } else {
        if (pLane->rowColors.isEmpty()) {
            // Allocate the whole arena so columns built before this one keep
            // their compact colour and later ones never reallocate.
            pLane->rowColors.fill(0, columnCount * height);
        }
        const int offset = columnIndex * height;
        for (int row = 0; row < height; ++row) {
            pLane->rowColors[offset + row] =
                    isZero(entries[static_cast<std::size_t>(row)])
                    ? 0
                    : rowColors[static_cast<std::size_t>(row)];
        }
    }
    pLane->columns.append(column);
}

} // namespace

StemDetailBuilder::StemDetailBuilder(QObject* parent)
        : QObject(parent) {
    qRegisterMetaType<StemDetailKey>("StemDetailKey");
    qRegisterMetaType<std::shared_ptr<const StemDetailTrack>>(
            "std::shared_ptr<const StemDetailTrack>");
}

void StemDetailBuilder::cancel() {
    m_generation.fetch_add(1, std::memory_order_relaxed);
}

void StemDetailBuilder::build(const StemDetailKey& key,
        const TrackPointer& pTrack,
        quint64 generation) {
    // A cancel may have been issued before this queued slot started. Do not
    // clear the token and start a full decode nobody is waiting for.
    if (generation != m_generation.load(std::memory_order_relaxed)) {
        return;
    }

    if (!pTrack) {
        emit built(key, nullptr);
        return;
    }

    // Same open pattern as AnalyzerThread: an 8-channel request resolves to
    // Paramix's native stems.flac cache or to a Traktor .stem.mp4, both of
    // which interleave four stereo stems in drums/bass/other/vocals order.
    mixxx::AudioSource::OpenParams openParams;
    openParams.setChannelCount(mixxx::kAnalysisMaxChannels);
    const mixxx::AudioSourcePointer pSource =
            SoundSourceProxy(pTrack).openAudioSource(openParams);
    if (!pSource) {
        kLogger.warning() << "Failed to open source for stem detail"
                          << pTrack->getLocation();
        emit built(key, nullptr);
        return;
    }

    const int channelCount = pSource->getSignalInfo().getChannelCount();
    const int stemCount = std::min(channelCount / 2, kMaxStems);
    const SINT totalFrames = pSource->frameLength();
    if (stemCount <= 0 || totalFrames <= 0) {
        emit built(key, nullptr);
        return;
    }

    const int spc = std::max(1, key.samplesPerColumn);
    const int height = std::clamp(key.height, 1, 63);
    const int64_t columnCount =
            (static_cast<int64_t>(totalFrames) + spc - 1) / spc;
    if (columnCount * height > kMaxCachedPixels) {
        kLogger.warning() << "Stem detail too large" << columnCount << "x" << height
                          << "pixels; keeping the previous picture";
        emit built(key, nullptr);
        return;
    }
    const uint32_t sampleRate = pSource->getSignalInfo().getSampleRate().value();

    auto pDetail = std::make_shared<StemDetailTrack>();
    pDetail->laneCount = stemCount;
    pDetail->totalSamples = totalFrames;
    pDetail->sampleRate = sampleRate;
    pDetail->trackId = key.trackId;
    pDetail->magnification = key.magnification;
    pDetail->generation = s_nextDetailGeneration.fetch_add(
            1, std::memory_order_relaxed);

    std::array<std::unique_ptr<StemDetailReducer>, kMaxStems> reducers;
    std::array<StemStaging, kMaxStems> staging;
    for (int stem = 0; stem < stemCount; ++stem) {
        StemDetailConfig config;
        config.sampleRate = sampleRate;
        config.height = height;
        config.samplesPerColumn = spc;
        config.magnification = key.magnification;
        config.verticalInset = 1;
        reducers[static_cast<std::size_t>(stem)] =
                std::make_unique<StemDetailReducer>(config);

        StemDetailLane& lane = pDetail->lanes[static_cast<std::size_t>(stem)];
        lane.spc = spc;
        lane.height = height;
        lane.firstSample = 0;
        lane.columns.reserve(static_cast<int>(columnCount));
        staging[static_cast<std::size_t>(stem)].left.resize(static_cast<std::size_t>(spc));
        staging[static_cast<std::size_t>(stem)].right.resize(static_cast<std::size_t>(spc));
    }

    const StemDetailColorParams colorParams;

    auto emitColumn = [&]() {
        for (int stem = 0; stem < stemCount; ++stem) {
            const std::size_t stemIdx = static_cast<std::size_t>(stem);
            const auto& entries = reducers[stemIdx]->processColumn(
                    staging[stemIdx].left.data(), staging[stemIdx].right.data());
            appendColumn(&pDetail->lanes[stemIdx],
                    entries,
                    height,
                    static_cast<int>(columnCount),
                    colorParams);
        }
    };

    mixxx::SampleBuffer sampleBuffer(
            kReadFramesPerChunk * static_cast<SINT>(channelCount));
    SINT frameIndex = 0;
    int fill = 0;
    while (frameIndex < totalFrames) {
        if (generation != m_generation.load(std::memory_order_relaxed)) {
            return;
        }
        const SINT framesRequested =
                std::min<SINT>(kReadFramesPerChunk, totalFrames - frameIndex);
        const mixxx::ReadableSampleFrames readable = pSource->readSampleFrames(
                mixxx::WritableSampleFrames(
                        mixxx::IndexRange::forward(frameIndex, framesRequested),
                        mixxx::SampleBuffer::WritableSlice(sampleBuffer)));
        const SINT framesRead = readable.frameLength();
        if (framesRead <= 0) {
            break;
        }
        const SINT readableFrames =
                std::min<SINT>(framesRead, readable.readableLength() / channelCount);
        if (readableFrames <= 0) {
            break;
        }

        const CSAMPLE* pData = readable.readableData();
        for (SINT frame = 0; frame < readableFrames; ++frame) {
            const CSAMPLE* pFrame = pData + static_cast<std::size_t>(frame) * channelCount;
            for (int stem = 0; stem < stemCount; ++stem) {
                StemStaging& stemStaging = staging[static_cast<std::size_t>(stem)];
                stemStaging.left[static_cast<std::size_t>(fill)] =
                        pFrame[2 * stem];
                stemStaging.right[static_cast<std::size_t>(fill)] =
                        pFrame[2 * stem + 1];
            }
            if (++fill == spc) {
                emitColumn();
                fill = 0;
            }
        }
        frameIndex += readableFrames;
    }

    if (fill > 0) {
        // Zero-pad the final column exactly like the reference lane cache.
        for (int stem = 0; stem < stemCount; ++stem) {
            StemStaging& stemStaging = staging[static_cast<std::size_t>(stem)];
            std::fill(stemStaging.left.begin() + fill, stemStaging.left.end(), 0.0f);
            std::fill(stemStaging.right.begin() + fill, stemStaging.right.end(), 0.0f);
        }
        emitColumn();
    }

    pSource->close();

    if (generation != m_generation.load(std::memory_order_relaxed)) {
        return;
    }
    emit built(key, std::move(pDetail));
}
