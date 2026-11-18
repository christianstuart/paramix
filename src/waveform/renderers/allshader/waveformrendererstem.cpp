#include "waveform/renderers/allshader/waveformrendererstem.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numeric>
#include <utility>

#include "control/controlproxy.h"
#include "engine/channels/enginedeck.h"
#include "rendergraph/material/rgbamaterial.h"
#include "rendergraph/material/texturematerial.h"
#include "rendergraph/texture.h"
#include "rendergraph/vertexupdaters/texturedvertexupdater.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/math.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/stemdetail/stemdetailcache.h"
#include "waveform/waveform.h"
#include "waveform/waveformwidgetfactory.h"

namespace {

/// Opacity of a muted (or solo-excluded) stem lane. The shape and colour stay
/// readable, but clearly ghosted so a muted stem is obviously out of the mix.
constexpr float kGhostedStemOpacity = 0.32f;
/// The shipped `stem_opacity` default. It is mapped to a fully opaque lane so
/// the detail picture keeps its exact #181818 base; lower values fade the
/// lanes.
constexpr float kOpaqueStemOpacity = 0.75f;
constexpr int kMinLaneDeviceHeight = 9;
constexpr int kMaxLaneDeviceHeight = 63;
constexpr int kVerticesPerRectangle = 6;

inline float laneAlphaFromOpacity(float opacity) {
    return std::clamp(opacity / kOpaqueStemOpacity, 0.0f, 1.0f);
}

struct TextureQuad {
    float x0;
    float y0;
    float x1;
    float y1;
    float u0;
    float v0;
    float u1;
    float v1;
};

} // namespace

using namespace rendergraph;

namespace allshader {

/// Draws the coloured waveform bands of all lanes as one nearest-filtered
/// texture. The lane bases stay as opaque geometry on the parent node.
class StemDetailTextureNode final : public rendergraph::GeometryNode {
  public:
    StemDetailTextureNode() {
        initForRectangles<rendergraph::TextureMaterial>(4);
        geometry().allocate(0);
    }

    void updateTexture(rendergraph::Context* pContext, const QImage& image) {
        auto& textureMaterial = dynamic_cast<rendergraph::TextureMaterial&>(material());
        textureMaterial.setTexture(std::make_unique<rendergraph::Texture>(
                pContext, image, rendergraph::Texture::Filtering::Nearest));
        textureMaterial.setUniform(1, 1.0f);
        markDirtyMaterial();
    }

    void updateGeometry(const TextureQuad* pQuads, int quadCount) {
        const int vertexCount = quadCount * kVerticesPerRectangle;
        geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
        geometry().allocate(vertexCount);
        if (vertexCount > 0) {
            TexturedVertexUpdater vertexUpdater{
                    geometry().vertexDataAs<Geometry::TexturedPoint2D>()};
            for (int i = 0; i < quadCount; ++i) {
                const TextureQuad& quad = pQuads[i];
                vertexUpdater.addRectangle({quad.x0, quad.y0},
                        {quad.x1, quad.y1},
                        {quad.u0, quad.v0},
                        {quad.u1, quad.v1});
            }
            DEBUG_ASSERT(vertexUpdater.index() == vertexCount);
        }
        markDirtyGeometry();
    }

    void clear() {
        if (geometry().vertexCount() == 0) {
            return;
        }
        geometry().allocate(0);
        markDirtyGeometry();
    }
};

WaveformRendererStem::WaveformRendererStem(
        WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip),
          m_splitStemTracks(false),
          m_reorderOnChange(true),
          m_opacity(kOpaqueStemOpacity),
          m_hasGeometry(false),
          m_pTextureNode(nullptr),
          m_textureValid(false),
          m_textureDetail(nullptr),
          m_texturePixelLength(0),
          m_textureLaneDeviceHeight(0),
          m_textureLaneCount(0),
          m_textureSamplesPerColumn(0),
          m_textureDetailSamplesPerColumn(0),
          m_textureLaneAlpha(0.0f),
          m_textureMutedMask(0),
          m_textureOrderSignature(0),
          m_textureSlip(false),
          m_textureWholeTrack(false),
          m_textureFirstSample(0.0),
          m_textureColumns(0) {
    initForRectangles<RGBAMaterial>(0);
    m_pTextureNode = appendChildNode(std::make_unique<StemDetailTextureNode>());
    setUsePreprocess(true);
}

void WaveformRendererStem::onSetup(const QDomNode&) {
}

bool WaveformRendererStem::init() {
    m_pStemMute.clear();
    m_pPregain.reset();

    // Keep a valid drawing order even for displays that provide a static track
    // without a channel group. The stem data itself is independent of the
    // per-stem volume/mute controls, so those controls are optional.
    m_stackOrder.resize(mixxx::kMaxSupportedStems);
    std::iota(m_stackOrder.begin(), m_stackOrder.end(), 0);

    if (!m_waveformRenderer->getGroup().isEmpty()) {
        const QString& group = m_waveformRenderer->getGroup();
        for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; stemIdx++) {
            const QString stemGroup = EngineDeck::getGroupForStem(group, stemIdx);
            m_pStemMute.emplace_back(
                    std::make_unique<ControlProxy>(stemGroup,
                            QStringLiteral("mute")));
            // Lane order is fixed to the stem order (drums, bass, other,
            // vocals). Routing, gain, mute and Pad FX changes must not shuffle
            // the waveform lanes while the user is mixing.
        }
        // Deck pregain (linear gain) drives the reducer magnification the same
        // way the shipping generator's setMagnification does.
        m_pPregain = std::make_unique<ControlProxy>(group,
                QStringLiteral("pregain"));
    }

#ifndef __SCENEGRAPH__
    auto* pWaveformWidgetFactory = WaveformWidgetFactory::instance();
    setSplitStemTracks(pWaveformWidgetFactory->isStemSplitTracks());
    connect(pWaveformWidgetFactory,
            &WaveformWidgetFactory::stemSplitTracksChanged,
            this,
            &WaveformRendererStem::setSplitStemTracks);
    setReorderOnChange(pWaveformWidgetFactory->isStemReorderOnChange());
    connect(pWaveformWidgetFactory,
            &WaveformWidgetFactory::stemReorderOnChangeChanged,
            this,
            &WaveformRendererStem::setReorderOnChange);
    setOutlineOpacity(pWaveformWidgetFactory->getStemOutlineOpacity());
    connect(pWaveformWidgetFactory,
            &WaveformWidgetFactory::stemOutlineOpacityChanged,
            this,
            &WaveformRendererStem::setOutlineOpacity);
    setOpacity(pWaveformWidgetFactory->getStemOpacity());
    connect(pWaveformWidgetFactory,
            &WaveformWidgetFactory::stemOpacityChanged,
            this,
            &WaveformRendererStem::setOpacity);
#endif

    if (WaveformWidgetFactory::isCreated()) {
        if (auto* pCache = WaveformWidgetFactory::instance()->stemDetailCache()) {
            connect(pCache,
                    &StemDetailCache::detailReady,
                    this,
                    &WaveformRendererStem::onDetailReady,
                    Qt::QueuedConnection);
        }
    }
    return true;
}

void WaveformRendererStem::preprocess() {
    if (!preprocessInner()) {
        if (geometry().vertexCount() != 0) {
            geometry().allocate(0);
            markDirtyGeometry();
        }
        if (m_pTextureNode) {
            m_pTextureNode->clear();
        }
    }
}

bool WaveformRendererStem::preprocessInner() {
    TrackPointer pTrack = m_waveformRenderer->getTrackInfo();

    if (!pTrack || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return false;
    }

    // Sample the track metadata at most once a second. The duration, sample
    // rate and stem count are immutable while the track is loaded, except that
    // stems can appear after separation finishes; the periodic refresh keeps
    // that case working without locking Track on every render frame.
    if (pTrack.get() != m_cachedTrack || m_cacheFramesUntilRefresh-- <= 0) {
        m_cachedTrack = pTrack.get();
        m_cachedStemCount = static_cast<int>(pTrack->getStemInfo().size());
        m_cachedTotalSamples =
                pTrack->getDuration() * pTrack->getSampleRate().value();
        m_cacheFramesUntilRefresh = 30;
    }
    // If this track isn't a stem track, let the regular waveform renderer draw
    // instead of the four-lane stem picture.
    if (m_cachedStemCount <= 0) {
        return false;
    }
    const auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                               : ::WaveformRendererAbstract::Play;

    ConstWaveformPointer waveform = pTrack->getWaveform();
    if (waveform.isNull() || waveform->getDataSize() <= 1) {
        return false;
    }
    // A track analysed without stem audio has no four-lane picture.
    if (!waveform->hasStem()) {
        return false;
    }

    const int stemCount = static_cast<int>(std::min<qsizetype>(
            m_cachedStemCount, m_stackOrder.size()));
    if (stemCount <= 0) {
        return false;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const int length = static_cast<int>(m_waveformRenderer->getLength());
    if (devicePixelRatio <= 0.0f || length <= 0) {
        return false;
    }
    const int pixelLength = std::max(
            1, static_cast<int>(std::lround(length * devicePixelRatio)));

    const double totalSamples = m_cachedTotalSamples;
    if (totalSamples <= 0.0) {
        return false;
    }
    // VisualPlayPosition returns -1 while a paused deck has no fresh vsync
    // position. Treat that as "keep the last picture" rather than translating
    // the waveform to a bogus position near the start of the track.
    if (m_waveformRenderer->getTruePosSample(positionType) < 0.0) {
        return m_hasGeometry;
    }
    const double firstSample =
            m_waveformRenderer->getFirstDisplayedPosition(positionType) * totalSamples;
    const double lastSample =
            m_waveformRenderer->getLastDisplayedPosition(positionType) * totalSamples;
    // Frames per device pixel. This must follow the viewport exactly, or the
    // picture drifts against the beat grid.
    const int samplesPerColumn = std::max(1,
            static_cast<int>(std::lround((lastSample - firstSample) / pixelLength)));

    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    if (breadth <= 0.0f) {
        return false;
    }
    const int gapDevicePx = std::max(1, static_cast<int>(std::lround(devicePixelRatio)));
    const int laneDeviceHeight = std::clamp(
            static_cast<int>(std::lround(breadth * devicePixelRatio / stemCount)) -
                    gapDevicePx,
            kMinLaneDeviceHeight,
            kMaxLaneDeviceHeight);

    // Deck gain -> generator magnification: 1 / (dB / 24 + 0.5), i.e. 2.0 at
    // 0 dB. The pregain control stores a linear gain.
    double deckGainDb = 0.0;
    if (m_pPregain) {
        const double linearGain = m_pPregain->get();
        if (linearGain > 0.0) {
            deckGainDb = ratio2db(linearGain);
        }
    }
    float magnificationDenominator =
            static_cast<float>(deckGainDb) / 24.0f + 0.5f;
    if (magnificationDenominator < 1e-4f) {
        magnificationDenominator = 1e-4f;
    }
    const float magnification = 1.0f / magnificationDenominator;

    auto* pFactory = WaveformWidgetFactory::isCreated()
            ? WaveformWidgetFactory::instance()
            : nullptr;
    auto* pCache = pFactory ? pFactory->stemDetailCache() : nullptr;
    std::shared_ptr<const StemDetailTrack> pDetail;
    if (pCache) {
        pDetail = pCache->request(
                pTrack, samplesPerColumn, laneDeviceHeight, magnification);
    }
    if (!pDetail) {
        // Keep the previous geometry and texture while the detail cache is
        // rebuilding. Clearing here made the stem picture blink out whenever
        // the zoom, device scale or another renderer rebuilt the detail.
        if (m_pDetail && m_pDetail->trackId != pTrack->getId()) {
            m_pDetail.reset();
            m_hasGeometry = false;
            m_textureValid = false;
            return false;
        }
        return m_hasGeometry;
    }
    if (m_pDetail && m_pDetail->trackId != pTrack->getId()) {
        m_pDetail.reset();
        m_hasGeometry = false;
        m_textureValid = false;
    }

    const int laneCount = std::min(stemCount, pDetail->laneCount);
    if (laneCount <= 0) {
        // A detail refresh can briefly publish an empty lane list. Keep the
        // last complete picture instead of flashing the underlying waveform.
        return m_hasGeometry;
    }

    const int totalLaneDeviceHeight =
            laneCount * laneDeviceHeight + (laneCount - 1) * gapDevicePx;
    const int topDevicePx = std::max(0,
            (static_cast<int>(std::lround(breadth * devicePixelRatio)) -
                    totalLaneDeviceHeight) /
                    2);

    // Vertical lane order follows the stack order (a stem brought to the
    // foreground moves to the bottom lane, like the old renderer).
    QVarLengthArray<int, mixxx::kMaxSupportedStems> laneStems;
    laneStems.reserve(laneCount);
    for (int stemIdx : std::as_const(m_stackOrder)) {
        if (stemIdx < 0 || stemIdx >= laneCount) {
            continue;
        }
        laneStems.append(stemIdx);
        if (laneStems.size() == laneCount) {
            break;
        }
    }
    const int drawLaneCount = static_cast<int>(laneStems.size());
    if (drawLaneCount <= 0) {
        return m_hasGeometry;
    }

    const uint selectedStems = m_waveformRenderer->getSelectedStems();
    unsigned int mutedMask = 0;
    for (int laneIndex = 0; laneIndex < drawLaneCount; ++laneIndex) {
        const int stemIdx = laneStems.at(laneIndex);
        const bool stemMuted = selectedStems
                ? !(selectedStems & 1u << stemIdx)
                : (!m_pStemMute.empty() && m_pStemMute[stemIdx]->toBool());
        if (stemMuted) {
            mutedMask |= 1u << laneIndex;
        }
    }

    uint64_t orderSignature = 0;
    for (int laneIndex = 0; laneIndex < drawLaneCount; ++laneIndex) {
        orderSignature = orderSignature * 16u +
                static_cast<uint64_t>(laneStems.at(laneIndex) + 1);
    }

    const float laneAlpha = laneAlphaFromOpacity(m_opacity);

    const int detailSamplesPerColumn =
            std::max(1, pDetail->lanes.front().spc);
    const int detailColumns = pDetail->lanes.front().columns.size();
    const double detailColumnStart = -firstSample / samplesPerColumn;

    // One run is a vertical span of rows that share the same pixel colour.
    auto forEachRun = [](const StemDetailLane& lane,
                              int columnIndex,
                              int rowLimit,
                              auto&& callback) {
        const StemDetailColumn& column = lane.columns.at(columnIndex);
        if (!column.wave || column.mask == 0) {
            return;
        }
        const uint64_t rowMask =
                column.mask & ((uint64_t{1} << rowLimit) - 1u);
        if (rowMask == 0) {
            return;
        }
        const int offset = columnIndex * lane.height;
        const bool perRow = !lane.rowColors.isEmpty() &&
                lane.rowColors.at(offset + std::countr_zero(rowMask)) != 0;
        if (perRow) {
            int runStart = -1;
            QRgb runColor = 0;
            for (int row = 0; row < rowLimit; ++row) {
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
                callback(runStart, rowLimit, runColor);
            }
        } else {
            uint64_t remaining = rowMask;
            while (remaining) {
                const int start = std::countr_zero(remaining);
                remaining &= remaining - 1u;
                int end = start + 1;
                while (end < rowLimit && (remaining & (uint64_t{1} << end))) {
                    remaining &= ~(uint64_t{1} << end);
                    ++end;
                }
                callback(start, end, column.rgba);
            }
        }
    };

    // The texture covers the viewport plus a margin. While the playhead stays
    // inside that margin only the quads move; the texture is rebuilt when the
    // visible range leaves it. This keeps texture allocation and uploads off
    // the per-frame path. Tracks whose whole picture fits in one texture use a
    // fixed whole-track range, so playback never rebuilds the texture at all.
    const int64_t totalTrackColumns =
            (pDetail->totalSamples + samplesPerColumn - 1) / samplesPerColumn;
    constexpr int64_t kMaxTextureWidth = 16384;
    const int64_t wholeTextureColumns = totalTrackColumns + 2 * pixelLength;
    const bool useWholeTrackTexture = wholeTextureColumns <= kMaxTextureWidth;
    const int marginColumns = useWholeTrackTexture ? pixelLength
                                                   : std::max(64, pixelLength / 2);
    const int textureColumns = useWholeTrackTexture
            ? static_cast<int>(wholeTextureColumns)
            : pixelLength + 2 * marginColumns;
    const bool textureParamsChanged = !m_textureValid ||
            m_textureDetail != pDetail.get() ||
            m_texturePixelLength != pixelLength ||
            m_textureLaneDeviceHeight != laneDeviceHeight ||
            m_textureLaneCount != drawLaneCount ||
            m_textureSamplesPerColumn != samplesPerColumn ||
            m_textureDetailSamplesPerColumn != detailSamplesPerColumn ||
            m_textureLaneAlpha != laneAlpha ||
            m_textureMutedMask != mutedMask ||
            m_textureOrderSignature != orderSignature ||
            m_textureSlip != m_isSlipRenderer ||
            m_textureWholeTrack != useWholeTrackTexture;
    const bool textureRangeExceeded = !m_textureValid ||
            (!useWholeTrackTexture &&
                    (firstSample < m_textureFirstSample ||
                            firstSample +
                                            static_cast<double>(pixelLength) *
                                                    samplesPerColumn >
                                    m_textureFirstSample +
                                            static_cast<double>(m_textureColumns) *
                                                    samplesPerColumn));
    bool textureRebuilt = false;
    if (textureParamsChanged || textureRangeExceeded) {
        m_textureFirstSample = useWholeTrackTexture
                ? -static_cast<double>(pixelLength) * samplesPerColumn
                : firstSample - static_cast<double>(marginColumns) * samplesPerColumn;
        m_textureColumns = textureColumns;
        m_textureWholeTrack = useWholeTrackTexture;

        const int imageWidth = textureColumns;
        const int imageHeight = totalLaneDeviceHeight;
        if (m_textureImage.size() != QSize(imageWidth, imageHeight) ||
                m_textureImage.format() != QImage::Format_RGBA8888_Premultiplied) {
            m_textureImage = QImage(imageWidth,
                    imageHeight,
                    QImage::Format_RGBA8888_Premultiplied);
        }
        m_textureImage.fill(Qt::transparent);
        const qsizetype bytesPerLine = m_textureImage.bytesPerLine();
        uchar* pBits = m_textureImage.bits();

        for (int x = 0; x < textureColumns; ++x) {
            const double sample = m_textureFirstSample +
                    static_cast<double>(x) * samplesPerColumn;
            const int columnIndex = static_cast<int>(
                    std::llround(sample / detailSamplesPerColumn));
            if (columnIndex < 0 || columnIndex >= detailColumns) {
                continue;
            }
            for (int laneIndex = 0; laneIndex < drawLaneCount; ++laneIndex) {
                const StemDetailLane& lane =
                        pDetail->lanes[static_cast<std::size_t>(
                                laneStems.at(laneIndex))];
                const bool muted = (mutedMask >> laneIndex) & 1u;
                const float alpha =
                        muted ? laneAlpha * kGhostedStemOpacity : laneAlpha;
                const int alphaByte = std::clamp(
                        static_cast<int>(std::lround(alpha * 255.0f)), 0, 255);
                if (alphaByte == 0) {
                    continue;
                }
                const int laneTopRow =
                        laneIndex * (laneDeviceHeight + gapDevicePx);
                const int visibleHeight = m_isSlipRenderer
                        ? laneDeviceHeight / 2
                        : laneDeviceHeight;
                const int rowLimit = m_isSlipRenderer ? lane.height / 2
                                                      : lane.height;

                forEachRun(lane,
                        columnIndex,
                        rowLimit,
                        [&](int rowStart, int rowEnd, QRgb color) {
                            int y0 = laneTopRow +
                                    (rowStart * laneDeviceHeight) / lane.height;
                            int y1 = laneTopRow +
                                    (rowEnd * laneDeviceHeight + lane.height - 1) /
                                            lane.height;
                            y0 = std::clamp(
                                    y0, laneTopRow, laneTopRow + visibleHeight);
                            y1 = std::clamp(
                                    y1, y0, laneTopRow + visibleHeight);
                            const auto red = static_cast<uchar>(
                                    (qRed(color) * alphaByte) / 255);
                            const auto green = static_cast<uchar>(
                                    (qGreen(color) * alphaByte) / 255);
                            const auto blue = static_cast<uchar>(
                                    (qBlue(color) * alphaByte) / 255);
                            for (int y = y0; y < y1; ++y) {
                                uchar* pPixel = pBits + y * bytesPerLine +
                                        static_cast<qsizetype>(x) * 4;
                                pPixel[0] = red;
                                pPixel[1] = green;
                                pPixel[2] = blue;
                                pPixel[3] = static_cast<uchar>(alphaByte);
                            }
                        });
            }
        }

        m_pTextureNode->updateTexture(
                m_waveformRenderer->getContext(), m_textureImage);
        m_textureDetail = pDetail.get();
        m_texturePixelLength = pixelLength;
        m_textureLaneDeviceHeight = laneDeviceHeight;
        m_textureLaneCount = drawLaneCount;
        m_textureSamplesPerColumn = samplesPerColumn;
        m_textureDetailSamplesPerColumn = detailSamplesPerColumn;
        m_textureLaneAlpha = laneAlpha;
        m_textureMutedMask = mutedMask;
        m_textureOrderSignature = orderSignature;
        m_textureSlip = m_isSlipRenderer;
        m_textureWholeTrack = useWholeTrackTexture;
        m_textureValid = true;
        textureRebuilt = true;
    }

    GeometryKey key;
    key.pDetail = pDetail.get();
    key.pixelLength = pixelLength;
    key.length = length;
    key.breadth = breadth;
    key.devicePixelRatio = devicePixelRatio;
    key.laneDeviceHeight = laneDeviceHeight;
    key.laneCount = drawLaneCount;
    key.topDevice = topDevicePx;
    key.firstColumn = static_cast<qint64>(std::floor(detailColumnStart));
    key.firstSample = firstSample;
    key.samplesPerColumn = samplesPerColumn;
    key.detailSamplesPerColumn = detailSamplesPerColumn;
    key.textureFirstSample = m_textureFirstSample;
    key.textureColumns = m_textureColumns;
    key.laneAlpha = laneAlpha;
    key.mutedMask = mutedMask;
    key.orderSignature = orderSignature;
    key.slip = m_isSlipRenderer;

    if (m_hasGeometry && key == m_geometryKey && !textureRebuilt) {
        return true;
    }

    // The lane background is transparent: only the wave pixels are drawn, so
    // the widget/deck background shows through outside the coloured bands.
    geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
    geometry().allocate(0);
    markDirtyGeometry();

    const double offsetColumns =
            (firstSample - m_textureFirstSample) / samplesPerColumn;
    const float quadX0 = 0.0f;
    const float quadX1 = static_cast<float>(pixelLength) / devicePixelRatio;
    const float quadU0 = static_cast<float>(
                                 std::clamp(offsetColumns, 0.0, std::max(0.0, static_cast<double>(m_textureColumns - pixelLength)))) /
            static_cast<float>(m_textureColumns);
    const float quadU1 = static_cast<float>(
                                 std::clamp(offsetColumns + pixelLength, 0.0, static_cast<double>(m_textureColumns))) /
            static_cast<float>(m_textureColumns);

    std::array<TextureQuad, mixxx::kMaxSupportedStems> quads{};
    int quadCount = 0;

    for (int laneIndex = 0; laneIndex < drawLaneCount; ++laneIndex) {
        const bool muted = (mutedMask >> laneIndex) & 1u;
        const float alpha = muted ? laneAlpha * kGhostedStemOpacity : laneAlpha;
        const int laneTopRow = laneIndex * (laneDeviceHeight + gapDevicePx);
        const int laneTopDevice = topDevicePx + laneTopRow;
        const int visibleHeight = m_isSlipRenderer ? laneDeviceHeight / 2
                                                   : laneDeviceHeight;
        const int alphaByte = std::clamp(
                static_cast<int>(std::lround(alpha * 255.0f)), 0, 255);

        if (alphaByte > 0 && visibleHeight > 0) {
            const float y0 = static_cast<float>(laneTopDevice) /
                    devicePixelRatio;
            const float y1 = static_cast<float>(laneTopDevice + visibleHeight) /
                    devicePixelRatio;
            const float v0 = static_cast<float>(laneTopRow) /
                    static_cast<float>(totalLaneDeviceHeight);
            const float v1 = static_cast<float>(laneTopRow + visibleHeight) /
                    static_cast<float>(totalLaneDeviceHeight);
            quads[quadCount++] = {quadX0, y0, quadX1, y1, quadU0, v0, quadU1, v1};
        }
    }

    m_pTextureNode->updateGeometry(quads.data(), quadCount);

    m_pDetail = pDetail;
    m_geometryKey = key;
    m_hasGeometry = true;

    markDirtyMaterial();
    return true;
}

} // namespace allshader
