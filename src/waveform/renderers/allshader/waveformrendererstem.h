#pragma once

#include <QImage>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "util/math.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"
#include "waveform/stemdetail/stemdetailtrack.h"

class QOpenGLTexture;
class ControlProxy;
class Track;

namespace allshader {
class WaveformRendererStem;
class StemDetailTextureNode;

/// Deterministic placement of the stem detail picture.
///
/// The picture is one nearest-filtered texel per device pixel, so its geometry
/// must only depend on whole detail columns. A paused deck must produce the
/// same column start, texture range and texture offset on every preprocess
/// pass. These helpers are the pure part of that layout, split out so the
/// invariant can be tested without a scene graph.
namespace stemdetail {

/// Frames per detail column for the current zoom.
///
/// Derived from the zoom-based audio resolution (and the device pixel ratio)
/// instead of the rounded viewport width, so re-measuring or resizing the
/// widget cannot change the value that is part of the cache key.
inline int samplesPerColumn(double audioSamplesPerPixel, float devicePixelRatio) {
    if (!(audioSamplesPerPixel > 0.0) || !(devicePixelRatio > 0.0f)) {
        return 1;
    }
    // audioSamplesPerPixel is already frames per logical pixel
    // (trackPixelCount = trackSamples / 2 / audioSamplesPerPixel), and the
    // picture is one texel per device pixel. A 2x here zooms the stems out
    // against the beat grid.
    return std::max(1,
            static_cast<int>(
                    std::lround(audioSamplesPerPixel / devicePixelRatio)));
}

/// Snap a sample position onto the whole-column grid.
inline double snapToColumn(double sample, int samplesPerColumn) {
    if (samplesPerColumn <= 1) {
        return sample;
    }
    return std::round(sample / static_cast<double>(samplesPerColumn)) *
            static_cast<double>(samplesPerColumn);
}

inline bool isColumnAligned(double sample, int samplesPerColumn) {
    return samplesPerColumn <= 1 ||
            std::fmod(sample, static_cast<double>(samplesPerColumn)) == 0.0;
}

/// Detail columns visible at `sample`. Negative before the first column.
inline qint64 columnIndex(double sample, int samplesPerColumn) {
    return static_cast<qint64>(
            std::floor(sample / static_cast<double>(std::max(1, samplesPerColumn))));
}

/// Remembers the last detail-cache key. preprocess() runs on every frame, but
/// the cache only needs to be asked again when the viewport key changes or a
/// rebuild completed.
struct DetailRequestMemo {
    StemDetailKey key;
    bool valid = false;

    bool needsRequest(const StemDetailKey& next, bool refreshPending) const {
        return refreshPending || !valid || !(key == next);
    }

    void remember(const StemDetailKey& next) {
        key = next;
        valid = true;
    }

    void forget() {
        valid = false;
    }
};

/// Sample range covered by one detail texture, in samples.
struct TextureRange {
    /// First sample of the range. Always on the detail column grid.
    double firstSample = 0.0;
    int columns = 0;
    /// The range covers the whole track, so playback never rebuilds it.
    bool wholeTrack = false;

    bool operator==(const TextureRange&) const = default;
};

/// True while `firstSample` and the next `pixelLength` columns stay inside the
/// range. The range is retained on a hit so playback only uploads a texture
/// when the visible span leaves it.
inline bool textureRangeCovers(const TextureRange& range,
        double firstSample,
        int samplesPerColumn,
        int pixelLength) {
    const double columnWidth = static_cast<double>(samplesPerColumn);
    return range.columns > 0 &&
            firstSample >= range.firstSample &&
            firstSample + static_cast<double>(pixelLength) * columnWidth <=
            range.firstSample +
                    static_cast<double>(range.columns) * columnWidth;
}

/// Range to build for the current viewport. Callers must only use it when the
/// previous range does not cover the viewport, or when it is not column
/// aligned while the deck is paused.
inline TextureRange textureRangeFor(double firstSample,
        int samplesPerColumn,
        int pixelLength,
        int64_t totalSamples,
        int64_t maxTextureColumns) {
    TextureRange range;
    const int64_t totalColumns =
            (totalSamples + samplesPerColumn - 1) / samplesPerColumn;
    const int64_t wholeColumns =
            totalColumns + 2 * static_cast<int64_t>(pixelLength);
    range.wholeTrack = wholeColumns <= maxTextureColumns;
    if (range.wholeTrack) {
        // Fixed range: the whole track plus one viewport of margin on each
        // side, so a played-through track never reallocates the texture.
        range.firstSample =
                -static_cast<double>(pixelLength) * samplesPerColumn;
        range.columns = static_cast<int>(wholeColumns);
    } else {
        const int64_t marginColumns = std::max(64, pixelLength / 2);
        range.firstSample = firstSample -
                static_cast<double>(marginColumns) * samplesPerColumn;
        range.columns =
                static_cast<int>(pixelLength + 2 * marginColumns);
    }
    return range;
}

} // namespace stemdetail
} // namespace allshader

class allshader::WaveformRendererStem final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::GeometryNode {
  public:
    explicit WaveformRendererStem(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play,
            ::WaveformRendererSignalBase::Options options =
                    ::WaveformRendererSignalBase::Option::None);

    // Pure virtual from WaveformRendererSignalBase, not used
    void onSetup(const QDomNode& node) override;

    bool init() override;

    bool supportsSlip() const override {
        return true;
    }

    // Virtuals for rendergraph::Node
    void preprocess() override;

  public slots:
    /// Kept for the QML/skin API. The detail picture always shows one lane per
    /// stem, so this no longer changes the layout.
    void setSplitStemTracks(bool splitStemTracks) {
        if (m_splitStemTracks != splitStemTracks) {
            m_splitStemTracks = splitStemTracks;
            markDirtyGeometry();
        }
    }
    void setReorderOnChange(bool value) {
        m_reorderOnChange = value;
        // Reset the stem layer stack to the natural order
        std::iota(m_stackOrder.begin(), m_stackOrder.end(), 0);
        markDirtyGeometry();
    }
    /// Deprecated no-op; the lane colour and outline come from the detail
    /// consumer and cannot be modulated without breaking the exact picture.
    void setOutlineOpacity(float value) {
        Q_UNUSED(value);
    }
    /// Multiplies the lane alpha. The shipped default (0.75) maps to fully
    /// opaque so the picture keeps its #181818 base colour.
    void setOpacity(float value) {
        m_opacity = value;
        markDirtyGeometry();
    }
    /// Deprecated no-op; per-stem visual gain would alter the verified colour
    /// values.
    void setStemVisualGain(int stemIdx, float value) {
        Q_UNUSED(stemIdx);
        Q_UNUSED(value);
    }

  private slots:
    void onDetailReady() {
        // A new picture was inserted. Re-request it on the next preprocess pass
        // instead of polling the cache every frame.
        m_detailRefreshPending = true;
        markDirtyGeometry();
    }

  private:
    /// Everything the emitted geometry depends on. A rebuild is skipped while
    /// this key is unchanged.
    struct GeometryKey {
        const StemDetailTrack* pDetail = nullptr;
        int pixelLength = 0;
        int length = 0;
        float breadth = 0.0f;
        float devicePixelRatio = 1.0f;
        int laneDeviceHeight = 0;
        int laneCount = 0;
        int topDevice = 0;
        qint64 firstColumn = 0;
        double firstSample = 0.0;
        int samplesPerColumn = 0;
        int detailSamplesPerColumn = 0;
        double textureFirstSample = 0.0;
        int textureColumns = 0;
        float laneAlpha = 1.0f;
        unsigned int mutedMask = 0;
        uint64_t orderSignature = 0;
        bool slip = false;

        bool operator==(const GeometryKey& other) const = default;
    };

    bool preprocessInner();

    bool m_isSlipRenderer;
    bool m_splitStemTracks;

    bool m_reorderOnChange;
    float m_opacity;

    std::vector<std::unique_ptr<ControlProxy>> m_pStemMute;
    std::unique_ptr<ControlProxy> m_pPregain;
    /// Deck play state. A stopped play button means the visual play rate is
    /// zero, which is when the picture position is snapped onto the column
    /// grid. Null for displays without a deck group (for example the static
    /// library preview), which are treated as stationary.
    std::unique_ptr<ControlProxy> m_pPlayControl;

    QVarLengthArray<int, mixxx::kMaxSupportedStems> m_stackOrder;

    bool m_hasGeometry;
    StemDetailTextureNode* m_pTextureNode;
    QImage m_textureImage;
    bool m_textureValid;
    const StemDetailTrack* m_textureDetail;
    int m_texturePixelLength;
    int m_textureLaneDeviceHeight;
    int m_textureLaneCount;
    int m_textureSamplesPerColumn;
    int m_textureDetailSamplesPerColumn;
    float m_textureLaneAlpha;
    unsigned int m_textureMutedMask;
    std::array<float, mixxx::kMaxSupportedStems> m_muteMix{};
    unsigned int m_textureMuteMixSignature;
    uint64_t m_textureOrderSignature;
    bool m_textureSlip;
    stemdetail::TextureRange m_textureRange;
    std::shared_ptr<const StemDetailTrack> m_pDetail;
    GeometryKey m_geometryKey;

    // Cache key of the last detail request. preprocess() runs on every frame,
    // but the cache is only consulted when the key (track, zoom, lane height,
    // magnification) changes or a rebuild completed.
    stemdetail::DetailRequestMemo m_detailRequest;
    bool m_detailRefreshPending = false;

    // Track metadata is immutable for a loaded track except for the stem
    // count. Sampling it once a second avoids repeated Track mutex
    // acquisitions on the render thread while still picking up stems that
    // finish separating after the track was loaded.
    const Track* m_cachedTrack = nullptr;
    TrackId m_cachedTrackId;
    uint32_t m_cachedSampleRate = 0;
    int m_cachedStemCount = 0;
    double m_cachedTotalSamples = 0.0;
    int m_cacheFramesUntilRefresh = 0;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererStem);
};
