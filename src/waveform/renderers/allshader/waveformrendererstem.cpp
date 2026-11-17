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
#include "rendergraph/vertexupdaters/rgbavertexupdater.h"
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
constexpr float kLaneBaseRed = 24.0f / 255.0f;
constexpr float kLaneBaseGreen = 24.0f / 255.0f;
constexpr float kLaneBaseBlue = 24.0f / 255.0f;
constexpr int kMinLaneDeviceHeight = 9;
constexpr int kMaxLaneDeviceHeight = 63;
constexpr int kVerticesPerRectangle = 6;

inline float laneAlphaFromOpacity(float opacity) {
    return std::clamp(opacity / kOpaqueStemOpacity, 0.0f, 1.0f);
}

inline float colorChannel(QRgb color, int shift) {
    return static_cast<float>((color >> shift) & 0xffu) / 255.0f;
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
          m_pTextureNode(nullptr) {
    initForRectangles<RGBAMaterial>(0);
    m_pTextureNode = appendChildNode(std::make_unique<StemDetailTextureNode>());
    setUsePreprocess(true);
}

void WaveformRendererStem::onSetup(const QDomNode&) {
}

bool WaveformRendererStem::init() {
    m_pStemGain.clear();
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
            m_pStemGain.emplace_back(
                    std::make_unique<ControlProxy>(stemGroup,
                            QStringLiteral("volume")));
            m_pStemMute.emplace_back(
                    std::make_unique<ControlProxy>(stemGroup,
                            QStringLiteral("mute")));
            auto bringToForeground = [this, stemIdx](double) {
                if (!m_reorderOnChange) {
                    return;
                }
                m_stackOrder.removeAll(stemIdx);
                m_stackOrder.append(stemIdx);
                markDirtyGeometry();
            };
            m_pStemGain.back()->connectValueChanged(this, bringToForeground);
            m_pStemMute.back()->connectValueChanged(this, bringToForeground);
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

    const auto stemInfo = pTrack->getStemInfo();
    // If this track isn't a stem track, let the regular waveform renderer draw
    // instead of the four-lane stem picture.
    if (stemInfo.isEmpty()) {
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
            stemInfo.size(), m_stackOrder.size()));
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

    const double totalSamples =
            pTrack->getDuration() * pTrack->getSampleRate().value();
    if (totalSamples <= 0.0) {
        return false;
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
        // Keep the previous geometry (if any) instead of clearing the display
        // while the first build is still running.
        m_pDetail.reset();
        m_hasGeometry = false;
        return false;
    }
    if (m_pDetail && m_pDetail->trackId != pTrack->getId()) {
        m_pDetail.reset();
        m_hasGeometry = false;
    }

    const int laneCount = std::min(stemCount, pDetail->laneCount);
    if (laneCount <= 0) {
        m_hasGeometry = false;
        return false;
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
        m_hasGeometry = false;
        return false;
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
    const double detailColumnEnd =
            (static_cast<double>(detailColumns) * detailSamplesPerColumn -
                    firstSample) /
            samplesPerColumn;
    const int startX = std::clamp(
            static_cast<int>(std::ceil(detailColumnStart)), 0, pixelLength);
    const int endX = std::clamp(
            static_cast<int>(std::ceil(detailColumnEnd)), 0, pixelLength);

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
    key.laneAlpha = laneAlpha;
    key.mutedMask = mutedMask;
    key.orderSignature = orderSignature;
    key.slip = m_isSlipRenderer;

    if (m_hasGeometry && key == m_geometryKey) {
        return true;
    }

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

    auto detailColumnFor = [&](int x, int laneSamplesPerColumn) {
        return static_cast<int>(std::llround(
                (firstSample + static_cast<double>(x) * samplesPerColumn) /
                laneSamplesPerColumn));
    };

    const int imageWidth = pixelLength;
    const int imageHeight = totalLaneDeviceHeight;
    if (m_textureImage.size() != QSize(imageWidth, imageHeight) ||
            m_textureImage.format() != QImage::Format_RGBA8888_Premultiplied) {
        m_textureImage = QImage(imageWidth,
                imageHeight,
                QImage::Format_RGBA8888_Premultiplied);
    }
    m_textureImage.fill(Qt::transparent);

    const bool drawBase = !m_isSlipRenderer && endX > startX;
    const int baseVertexCount = drawBase
            ? drawLaneCount * kVerticesPerRectangle
            : 0;
    geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
    geometry().allocate(baseVertexCount);
    markDirtyGeometry();
    RGBAVertexUpdater baseUpdater{
            geometry().vertexDataAs<Geometry::RGBAColoredPoint2D>()};

    std::array<TextureQuad, mixxx::kMaxSupportedStems> quads{};
    int quadCount = 0;

    for (int laneIndex = 0; laneIndex < drawLaneCount; ++laneIndex) {
        const int stemIdx = laneStems.at(laneIndex);
        const StemDetailLane& lane =
                pDetail->lanes[static_cast<std::size_t>(stemIdx)];
        const bool muted = (mutedMask >> laneIndex) & 1u;
        const float alpha = muted ? laneAlpha * kGhostedStemOpacity : laneAlpha;
        const int laneTopRow = laneIndex * (laneDeviceHeight + gapDevicePx);
        const int laneTopDevice = topDevicePx + laneTopRow;
        const int laneBottomDevice = laneTopDevice + laneDeviceHeight;
        const int visibleHeight = m_isSlipRenderer ? laneDeviceHeight / 2
                                                   : laneDeviceHeight;
        const int rowLimit = m_isSlipRenderer ? lane.height / 2 : lane.height;
        const int alphaByte = std::clamp(
                static_cast<int>(std::lround(alpha * 255.0f)), 0, 255);

        if (drawBase) {
            // Opaque lane base, clipped to the part of the track that the
            // picture covers (pre/post-track stays window background).
            baseUpdater.addRectangle(
                    {static_cast<float>(startX) / devicePixelRatio,
                            static_cast<float>(laneTopDevice) / devicePixelRatio},
                    {static_cast<float>(endX) / devicePixelRatio,
                            static_cast<float>(laneBottomDevice) / devicePixelRatio},
                    {kLaneBaseRed, kLaneBaseGreen, kLaneBaseBlue, alpha});
        }

        if (alphaByte > 0 && endX > startX) {
            for (int x = startX; x < endX; ++x) {
                const int columnIndex = detailColumnFor(x, lane.spc);
                if (columnIndex < 0 || columnIndex >= lane.columns.size()) {
                    continue;
                }
                forEachRun(lane,
                        columnIndex,
                        rowLimit,
                        [&](int rowStart, int rowEnd, QRgb color) {
                            int y0 = laneTopRow +
                                    (rowStart * laneDeviceHeight) / lane.height;
                            int y1 = laneTopRow +
                                    (rowEnd * laneDeviceHeight + lane.height - 1) /
                                            lane.height;
                            y0 = std::clamp(y0, laneTopRow, laneTopRow + visibleHeight);
                            y1 = std::clamp(y1, y0, laneTopRow + visibleHeight);
                            const auto red = static_cast<uchar>(
                                    (qRed(color) * alphaByte) / 255);
                            const auto green = static_cast<uchar>(
                                    (qGreen(color) * alphaByte) / 255);
                            const auto blue = static_cast<uchar>(
                                    (qBlue(color) * alphaByte) / 255);
                            for (int y = y0; y < y1; ++y) {
                                uchar* pPixel = m_textureImage.scanLine(y) +
                                        static_cast<qsizetype>(x) * 4;
                                pPixel[0] = red;
                                pPixel[1] = green;
                                pPixel[2] = blue;
                                pPixel[3] = static_cast<uchar>(alphaByte);
                            }
                        });
            }
        }

        if (visibleHeight > 0 && endX > startX) {
            const float x0 = static_cast<float>(startX) / devicePixelRatio;
            const float x1 = static_cast<float>(endX) / devicePixelRatio;
            const float y0 = static_cast<float>(laneTopDevice) / devicePixelRatio;
            const float y1 = static_cast<float>(laneTopDevice + visibleHeight) /
                    devicePixelRatio;
            const float u0 = static_cast<float>(startX) / imageWidth;
            const float u1 = static_cast<float>(endX) / imageWidth;
            const float v0 = static_cast<float>(laneTopRow) / imageHeight;
            const float v1 = static_cast<float>(laneTopRow + visibleHeight) /
                    imageHeight;
            quads[quadCount++] = {x0, y0, x1, y1, u0, v0, u1, v1};
        }
    }

    DEBUG_ASSERT(baseUpdater.index() == baseVertexCount);

    m_pTextureNode->updateTexture(m_waveformRenderer->getContext(), m_textureImage);
    m_pTextureNode->updateGeometry(quads.data(), quadCount);

    m_pDetail = pDetail;
    m_geometryKey = key;
    m_hasGeometry = true;

    markDirtyMaterial();
    return true;
}

} // namespace allshader
