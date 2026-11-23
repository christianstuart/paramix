#include "waveform/renderers/allshader/waveformrendererstem.h"

#include <QDebug>
#include <QSet>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numeric>
#include <utility>

#include "control/controlproxy.h"
#include "engine/channels/enginedeck.h"
#include "rendergraph/material/rgbamaterial.h"
#include "rendergraph/material/texturedcolormaterial.h"
#include "rendergraph/texture.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/cmdlineargs.h"
#include "util/math.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/stemdetail/stemdetailcache.h"
#include "waveform/stemdetail/stempagebuilder.h"
#include "waveform/stemdetail/stempagecache.h"
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

struct PageQuad {
    float x0;
    float y0;
    float x1;
    float y1;
    float u0;
    float v0;
    float u1;
    float v1;
    float alpha;
};

} // namespace

using namespace rendergraph;

namespace allshader {

/// Draws one fixed-content page of the stem picture.
///
/// The texture is immutable: lane opacity, mute ghosting, lane order and slip
/// mode only change the per-vertex alpha and the quad coordinates. This keeps
/// mute fades and the opacity slider off the texture rebuild path.
class StemPageTextureNode final : public rendergraph::GeometryNode {
  public:
    StemPageTextureNode() {
        initForRectangles<TexturedColorMaterial>(mixxx::kMaxSupportedStems);
        geometry().allocate(0);
    }

    void updateTexture(rendergraph::Context* pContext, const QImage& image) {
        auto& textureMaterial = dynamic_cast<TexturedColorMaterial&>(material());
        textureMaterial.setTexture(std::make_unique<rendergraph::Texture>(
                pContext, image, rendergraph::Texture::Filtering::Nearest));
        markDirtyMaterial();
    }

    void updateGeometry(const PageQuad* pQuads, int quadCount) {
        const int vertexCount = quadCount * kVerticesPerRectangle;
        geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
        geometry().allocate(vertexCount);
        if (vertexCount > 0) {
            auto* pVertexData =
                    geometry().vertexDataAs<Geometry::TexturedColoredPoint2D>();
            auto* pWrite = pVertexData;
            const auto addTriangle = [&pWrite](float x0,
                                             float y0,
                                             float x1,
                                             float y1,
                                             float x2,
                                             float y2,
                                             float u0,
                                             float v0,
                                             float u1,
                                             float v1,
                                             float u2,
                                             float v2,
                                             float alpha) {
                const QVector4D color(1.0f, 1.0f, 1.0f, alpha);
                *pWrite++ = Geometry::TexturedColoredPoint2D{
                        {x0, y0}, {u0, v0}, color};
                *pWrite++ = Geometry::TexturedColoredPoint2D{
                        {x1, y1}, {u1, v1}, color};
                *pWrite++ = Geometry::TexturedColoredPoint2D{
                        {x2, y2}, {u2, v2}, color};
            };
            for (int i = 0; i < quadCount; ++i) {
                const PageQuad& quad = pQuads[i];
                addTriangle(quad.x0,
                        quad.y0,
                        quad.x1,
                        quad.y0,
                        quad.x0,
                        quad.y1,
                        quad.u0,
                        quad.v0,
                        quad.u1,
                        quad.v0,
                        quad.u0,
                        quad.v1,
                        quad.alpha);
                addTriangle(quad.x0,
                        quad.y1,
                        quad.x1,
                        quad.y1,
                        quad.x1,
                        quad.y0,
                        quad.u0,
                        quad.v1,
                        quad.u1,
                        quad.v1,
                        quad.u1,
                        quad.v0,
                        quad.alpha);
            }
            DEBUG_ASSERT(pWrite == pVertexData + vertexCount);
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
          m_pPageCache(nullptr) {
    initForRectangles<RGBAMaterial>(0);
    setUsePreprocess(true);
    m_statsTimer.start();
}

void WaveformRendererStem::onSetup(const QDomNode&) {
}

bool WaveformRendererStem::init() {
    m_pStemMute.clear();
    m_pPregain.reset();
    m_pPlayControl.reset();
    m_pPageCache = WaveformWidgetFactory::isCreated()
            ? WaveformWidgetFactory::instance()->stemPageCache()
            : nullptr;

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
        // A stopped play button means the visual play rate is zero: the
        // picture is stationary and its position is snapped onto whole detail
        // columns.
        m_pPlayControl = std::make_unique<ControlProxy>(group,
                QStringLiteral("play"));
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

void WaveformRendererStem::clearPageGeometry() {
    for (auto& state : m_pageNodes) {
        if (state.hasDrawKey) {
            state.pNode->clear();
            state.hasDrawKey = false;
        }
    }
    m_lastDrawnPages.clear();
    m_hasGeometry = false;
}

void WaveformRendererStem::preprocess() {
    ++m_frameCounter;
    ++m_statFrames;
    QElapsedTimer preprocessTimer;
    preprocessTimer.start();
    if (!preprocessInner()) {
        clearPageGeometry();
    }
    const qint64 elapsedUs = preprocessTimer.nsecsElapsed() / 1000;
    if (elapsedUs > m_statMaxPreprocessUs) {
        m_statMaxPreprocessUs = elapsedUs;
    }
    m_lastPreprocessUs = elapsedUs;

    if (m_statsTimer.elapsed() >= 1000) {
        if (CmdlineArgs::Instance().getDeveloper()) {
            QString cacheStats;
            if (m_pPageCache) {
                const StemPageCache::Stats stats = m_pPageCache->stats();
                cacheStats = QStringLiteral(" hits=%1 misses=%2 builds=%3 cached=%4 pending=%5")
                                     .arg(stats.hits)
                                     .arg(stats.misses)
                                     .arg(stats.builds)
                                     .arg(stats.cachedPages)
                                     .arg(stats.pendingPages);
            }
            qInfo().noquote()
                    << "StemRender" << m_waveformRenderer->getGroup()
                    << "fps" << m_statFrames
                    << "pre_us_max" << m_statMaxPreprocessUs
                    << "page_req" << m_statPageRequests
                    << "tex_upload" << m_statTextureUploads
                    << "geom_updates" << m_statGeometryUpdates
                    << "cache" << cacheStats;
        }
        m_statsTimer.restart();
        m_statFrames = 0;
        m_statPageRequests = 0;
        m_statTextureUploads = 0;
        m_statGeometryUpdates = 0;
        m_statMaxPreprocessUs = 0;
    }
}

std::shared_ptr<const QImage> WaveformRendererStem::pageImage(
        const StemPageKey& key,
        const std::shared_ptr<const StemDetailTrack>& detail,
        bool requestIfMissing) {
    if (requestIfMissing) {
        ++m_statPageRequests;
    }
    if (m_pPageCache) {
        return requestIfMissing ? m_pPageCache->request(key, detail)
                                : m_pPageCache->lookup(key);
    }

    // Standalone renderers (tests or a display without the factory) still
    // need a picture. Build synchronously, but only once per page.
    const auto it = m_localPages.constFind(key);
    if (it != m_localPages.constEnd()) {
        return it.value();
    }
    if (!requestIfMissing) {
        return {};
    }
    QImage image = buildStemPageImage(*detail, key);
    if (image.isNull()) {
        return {};
    }
    auto pImage = std::make_shared<const QImage>(std::move(image));
    m_localPages.insert(key, pImage);
    return pImage;
}

StemPageTextureNode* WaveformRendererStem::acquirePageNode(
        const StemPageKey& key,
        const QImage& image) {
    auto it = m_pageNodes.find(key);
    if (it != m_pageNodes.end()) {
        if (it->pImage != &image) {
            it->pNode->updateTexture(m_waveformRenderer->getContext(), image);
            it->pImage = &image;
            ++m_statTextureUploads;
        }
        return it->pNode;
    }

    auto* pNode = appendChildNode(std::make_unique<StemPageTextureNode>());
    pNode->updateTexture(m_waveformRenderer->getContext(), image);
    ++m_statTextureUploads;
    PageNodeState state;
    state.pNode = pNode;
    state.pImage = &image;
    state.lastUsedFrame = m_frameCounter;
    m_pageNodes.insert(key, state);
    return pNode;
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
    const bool trackChanged = pTrack.get() != m_cachedTrack;
    if (trackChanged || m_cacheFramesUntilRefresh-- <= 0) {
        m_cachedTrack = pTrack.get();
        m_cachedTrackId = pTrack->getId();
        m_cachedSampleRate = pTrack->getSampleRate().value();
        m_cachedStemCount = static_cast<int>(pTrack->getStemInfo().size());
        m_cachedTotalSamples =
                pTrack->getDuration() * pTrack->getSampleRate().value();
        m_cacheFramesUntilRefresh = 30;
    }
    if (trackChanged) {
        // Let preprocess() clear the previous track's picture once, then build
        // the new track from scratch.
        return false;
    }
    // If this track isn't a stem track, let the regular waveform renderer draw
    // instead of the four-lane stem picture.
    if (m_cachedStemCount <= 0) {
        // A non-stem track is cleared via trackChanged; a transient zero while
        // the same track is loaded must not blink the stem picture away.
        return m_hasGeometry;
    }
    const auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                               : ::WaveformRendererAbstract::Play;

    ConstWaveformPointer waveform = pTrack->getWaveform();
    if (waveform.isNull() || waveform->getDataSize() <= 1) {
        return m_hasGeometry;
    }
    // A track analysed without stem audio has no four-lane picture. Retain the
    // last complete picture during a transient waveform refresh instead of
    // briefly revealing the underlying cue/grid markers.
    if (!waveform->hasStem()) {
        return m_hasGeometry;
    }

    const int stemCount = static_cast<int>(std::min<qsizetype>(
            m_cachedStemCount, m_stackOrder.size()));
    if (stemCount <= 0) {
        return m_hasGeometry;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const int length = static_cast<int>(m_waveformRenderer->getLength());
    if (devicePixelRatio <= 0.0f || length <= 0) {
        return m_hasGeometry;
    }
    const int pixelLength = std::max(
            1, static_cast<int>(std::lround(length * devicePixelRatio)));

    const double totalSamples = m_cachedTotalSamples;
    if (totalSamples <= 0.0) {
        return m_hasGeometry;
    }
    // A negative true position means no visual position has been established
    // yet (for example a newly loaded track waiting for its first buffer).
    // Keep the last picture rather than translating the waveform to a bogus
    // position near the start of the track. WaveformWidgetRenderer holds the
    // previous position when the visual play position has no usable buffer.
    if (m_waveformRenderer->getTruePosSample(positionType) < 0.0) {
        return m_hasGeometry;
    }

    const int requestedSamplesPerColumn = stemdetail::samplesPerColumn(
            m_waveformRenderer->getAudioSamplePerPixel(), devicePixelRatio);

    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    if (breadth <= 0.0f) {
        return false;
    }
    const int gapDevicePx = std::max(1, static_cast<int>(std::lround(devicePixelRatio)));
    const int liveLaneDeviceHeight = std::clamp(
            static_cast<int>(std::lround(breadth * devicePixelRatio / stemCount)) -
                    gapDevicePx,
            kMinLaneDeviceHeight,
            kMaxLaneDeviceHeight);

    // Page keys use a stable lane height. During a layout or library animation
    // the live height changes every frame; regenerating pages for each step
    // would be a guaranteed frame drop. Draw the old pages scaled instead and
    // change the key once the height settles.
    if (m_pageLaneDeviceHeight <= 0) {
        m_pageLaneDeviceHeight = liveLaneDeviceHeight;
        m_pageGapDevicePx = gapDevicePx;
        m_laneHeightSettleTimer.restart();
    } else if (liveLaneDeviceHeight != m_pageLaneDeviceHeight ||
            gapDevicePx != m_pageGapDevicePx) {
        if (!m_laneHeightSettleTimer.isValid() ||
                m_laneHeightSettleTimer.elapsed() >= kViewportSettleMs) {
            m_pageLaneDeviceHeight = liveLaneDeviceHeight;
            m_pageGapDevicePx = gapDevicePx;
            m_laneHeightSettleTimer.restart();
        }
    } else {
        m_laneHeightSettleTimer.restart();
    }

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
        StemDetailKey requestKey;
        requestKey.trackId = m_cachedTrackId;
        requestKey.sampleRate = m_cachedSampleRate;
        requestKey.samplesPerColumn = requestedSamplesPerColumn;
        requestKey.height = m_pageLaneDeviceHeight;
        requestKey.magnification = magnification;

        // Only ask for a new detail picture after the viewport key has been
        // stable briefly. This keeps a zoom animation or a live resize from
        // posting a full-track decode request on every frame.
        if (!(m_hasDetailSettleKey && m_detailSettleKey == requestKey)) {
            m_detailSettleKey = requestKey;
            m_hasDetailSettleKey = true;
            m_detailSettleTimer.restart();
        }
        const bool settled = !m_pDetail ||
                m_detailSettleTimer.elapsed() >= kViewportSettleMs;
        if (settled &&
                (!m_pDetail || m_detailRefreshPending ||
                        m_detailRequest.needsRequest(requestKey, false))) {
            pDetail = pCache->request(pTrack,
                    requestedSamplesPerColumn,
                    m_pageLaneDeviceHeight,
                    magnification);
            m_detailRefreshPending = false;
            if (pDetail) {
                // Remember the request even when the cache returned the
                // previous zoom's picture. detailReady() wakes us when the
                // matching picture is ready instead of polling every frame.
                m_detailRequest.remember(requestKey);
            } else {
                m_detailRequest.forget();
            }
        }
    }
    if (!pDetail) {
        // Keep the previous geometry and pages while the detail cache is
        // rebuilding. Clearing here made the stem picture blink out whenever
        // the zoom, device scale or another renderer rebuilt the detail.
        if (m_pDetail && m_pDetail->trackId != m_cachedTrackId) {
            m_pDetail.reset();
            return false;
        }
        pDetail = m_pDetail;
    }

    if (!pDetail) {
        return false;
    }
    if (m_pDetail && m_pDetail->trackId != m_cachedTrackId) {
        m_pDetail.reset();
    }

    const int laneCount = std::min(stemCount, pDetail->laneCount);
    if (laneCount <= 0) {
        // A detail refresh can briefly publish an empty lane list. Keep the
        // last complete picture instead of flashing the underlying waveform.
        return m_hasGeometry;
    }

    const int contentSamplesPerColumn =
            std::max(1, pDetail->lanes.front().spc);
    const int detailLaneHeight = std::max(1, pDetail->lanes.front().height);
    const float deviceScale = devicePixelRatio;

    const double liveFirstSample =
            m_waveformRenderer->getFirstDisplayedPosition(positionType) * totalSamples;
    const bool playRateZero = !m_pPlayControl || !m_pPlayControl->toBool();
    const double firstSample = playRateZero
            ? stemdetail::snapToColumn(liveFirstSample, contentSamplesPerColumn)
            : liveFirstSample;
    const double firstDeviceColumn =
            firstSample / static_cast<double>(contentSamplesPerColumn);
    const double lastDeviceColumn =
            firstDeviceColumn + static_cast<double>(pixelLength);

    const int totalLiveLaneDeviceHeight =
            laneCount * liveLaneDeviceHeight + (laneCount - 1) * gapDevicePx;
    const int topDevicePx = std::max(0,
            (static_cast<int>(std::lround(breadth * devicePixelRatio)) -
                    totalLiveLaneDeviceHeight) /
                    2);

    // Vertical lane order follows the stack order (a stem brought to the
    // foreground moves to the bottom lane, like the old renderer). The texture
    // itself always stays in natural stem order.
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
    constexpr float kMuteFadeStep = 1.0f / 12.0f;
    std::array<float, mixxx::kMaxSupportedStems> laneAlphas{};
    uint32_t alphaSignature = 0;
    uint64_t orderSignature = 0;
    const float laneAlpha = laneAlphaFromOpacity(m_opacity);
    for (int laneIndex = 0; laneIndex < drawLaneCount; ++laneIndex) {
        const int stemIdx = laneStems.at(laneIndex);
        const bool stemMuted = selectedStems
                ? !(selectedStems & 1u << stemIdx)
                : (!m_pStemMute.empty() && m_pStemMute[stemIdx]->toBool());
        float& mix = m_muteMix[stemIdx];
        const float target = stemMuted ? 1.0f : 0.0f;
        if (mix < target) {
            mix = std::min(target, mix + kMuteFadeStep);
        } else if (mix > target) {
            mix = std::max(target, mix - kMuteFadeStep);
        }
        laneAlphas[laneIndex] = laneAlpha *
                (1.0f + (kGhostedStemOpacity - 1.0f) * mix);
        const int alphaByte = std::clamp(
                static_cast<int>(std::lround(laneAlphas[laneIndex] * 255.0f)),
                0,
                255);
        alphaSignature = alphaSignature * 257u + static_cast<uint32_t>(alphaByte);
        orderSignature = orderSignature * 16u +
                static_cast<uint64_t>(stemIdx + 1);
    }

    const qint64 totalColumns =
            (pDetail->totalSamples + contentSamplesPerColumn - 1) /
            contentSamplesPerColumn;
    if (totalColumns <= 0) {
        return m_hasGeometry;
    }

    const qint64 firstVisiblePage = std::max<qint64>(0,
            stemdetail::pageIndexForColumn(firstDeviceColumn, kPageWidth));
    const qint64 lastVisiblePage = std::min<qint64>(
            stemdetail::pageIndexForColumn(lastDeviceColumn, kPageWidth),
            stemdetail::pageIndexForColumn(totalColumns - 1, kPageWidth));

    QSet<StemPageKey> visibleKeys;
    QVarLengthArray<StemPageKey, 8> currentPageKeys;
    bool anyReady = false;

    const auto makePageKey = [&](qint64 pageIndex) {
        StemPageKey key;
        key.trackId = m_cachedTrackId;
        key.sampleRate = m_cachedSampleRate;
        key.detailSamplesPerColumn = contentSamplesPerColumn;
        key.detailLaneHeight = detailLaneHeight;
        key.samplesPerColumn = contentSamplesPerColumn;
        key.laneDeviceHeight = m_pageLaneDeviceHeight;
        key.laneCount = laneCount;
        key.gapDevicePx = m_pageGapDevicePx;
        key.pageWidth = kPageWidth;
        key.pageIndex = static_cast<int>(pageIndex);
        key.magnification = pDetail->magnification;
        key.detailGeneration = pDetail->generation;
        return key;
    };

    const auto drawPage = [&](const StemPageKey& pageKey,
                                  bool updateLastDrawn,
                                  bool requestIfMissing) {
        if (pageKey.laneCount < drawLaneCount) {
            return false;
        }
        auto nodeIt = m_pageNodes.find(pageKey);
        if (nodeIt == m_pageNodes.end()) {
            const auto image = pageImage(pageKey, pDetail, requestIfMissing);
            if (!image) {
                return false;
            }
            acquirePageNode(pageKey, *image);
            nodeIt = m_pageNodes.find(pageKey);
            if (nodeIt == m_pageNodes.end()) {
                return false;
            }
        }
        PageNodeState& state = *nodeIt;

        // Fallback pages can come from a different zoom, so derive the
        // viewport mapping from the page's own frames-per-column.
        const double currentFirstDeviceColumn =
                firstSample / static_cast<double>(pageKey.samplesPerColumn);
        const double currentLastDeviceColumn =
                currentFirstDeviceColumn + static_cast<double>(pixelLength);
        const double pageFirstDeviceColumn =
                static_cast<double>(pageKey.pageIndex) * pageKey.pageWidth;
        const double pageLastDeviceColumn =
                pageFirstDeviceColumn + pageKey.pageWidth;
        const double pageFirstVisible =
                std::max(pageFirstDeviceColumn, currentFirstDeviceColumn);
        const double pageLastVisible =
                std::min(pageLastDeviceColumn, currentLastDeviceColumn);
        if (pageLastVisible <= pageFirstVisible) {
            return false;
        }

        PageDrawKey drawKey;
        drawKey.page = pageKey;
        drawKey.firstDeviceColumn = currentFirstDeviceColumn;
        drawKey.lastDeviceColumn = currentLastDeviceColumn;
        drawKey.devicePixelRatio = deviceScale;
        drawKey.topDevice = topDevicePx;
        drawKey.laneDeviceHeight = liveLaneDeviceHeight;
        drawKey.gapDevicePx = gapDevicePx;
        drawKey.drawLaneCount = drawLaneCount;
        drawKey.orderSignature = orderSignature;
        drawKey.alphaSignature = alphaSignature;
        drawKey.slip = m_isSlipRenderer;

        const double x0 = std::clamp(
                (pageFirstVisible - currentFirstDeviceColumn) / deviceScale,
                0.0,
                static_cast<double>(length));
        const double x1 = std::clamp(
                (pageLastVisible - currentFirstDeviceColumn) / deviceScale,
                0.0,
                static_cast<double>(length));
        const float u0 = static_cast<float>(
                (pageFirstVisible - pageFirstDeviceColumn) /
                static_cast<double>(pageKey.pageWidth));
        const float u1 = static_cast<float>(
                (pageLastVisible - pageFirstDeviceColumn) /
                static_cast<double>(pageKey.pageWidth));

        if (!state.hasDrawKey || !(state.drawKey == drawKey)) {
            const int pageTotalLaneHeight =
                    pageKey.laneCount * pageKey.laneDeviceHeight +
                    (pageKey.laneCount - 1) * pageKey.gapDevicePx;
            std::array<PageQuad, mixxx::kMaxSupportedStems> quads{};
            int quadCount = 0;
            for (int laneIndex = 0; laneIndex < drawLaneCount; ++laneIndex) {
                const int stemIdx = laneStems.at(laneIndex);
                if (stemIdx >= pageKey.laneCount || stemIdx < 0) {
                    continue;
                }
                const int laneTop = topDevicePx +
                        laneIndex * (liveLaneDeviceHeight + gapDevicePx);
                const int visibleHeight = m_isSlipRenderer
                        ? liveLaneDeviceHeight / 2
                        : liveLaneDeviceHeight;
                if (visibleHeight <= 0 || laneAlphas[laneIndex] <= 0.0f) {
                    continue;
                }
                const int pageLaneTop = stemIdx *
                        (pageKey.laneDeviceHeight + pageKey.gapDevicePx);
                const int pageLaneHeight = pageKey.laneDeviceHeight;
                const int pageVisibleHeight = m_isSlipRenderer
                        ? pageLaneHeight / 2
                        : pageLaneHeight;
                const float v0 = static_cast<float>(pageLaneTop) /
                        static_cast<float>(pageTotalLaneHeight);
                const float v1 = static_cast<float>(
                                         pageLaneTop + pageVisibleHeight) /
                        static_cast<float>(pageTotalLaneHeight);
                quads[quadCount++] = {
                        static_cast<float>(x0),
                        static_cast<float>(laneTop) / deviceScale,
                        static_cast<float>(x1),
                        static_cast<float>(laneTop + visibleHeight) / deviceScale,
                        u0,
                        v0,
                        u1,
                        v1,
                        laneAlphas[laneIndex]};
            }
            state.pNode->updateGeometry(quads.data(), quadCount);
            ++m_statGeometryUpdates;
            state.drawKey = drawKey;
            state.hasDrawKey = true;
        }
        state.lastUsedFrame = m_frameCounter;
        visibleKeys.insert(pageKey);
        if (updateLastDrawn) {
            currentPageKeys.append(pageKey);
        }
        return true;
    };

    for (qint64 pageIndex = firstVisiblePage; pageIndex <= lastVisiblePage;
            ++pageIndex) {
        if (drawPage(makePageKey(pageIndex), true, true)) {
            anyReady = true;
        }
    }

    // Prefetch the pages just outside the viewport. A page covers several
    // hundred milliseconds of playback, so requesting the next one when it
    // first becomes visible would be too late. Building one page on each side
    // also keeps seeking backwards cheap.
    const std::array<qint64, 2> prefetchPages = {
            firstVisiblePage - 1, lastVisiblePage + 1};
    bool prefetchedUpload = false;
    for (qint64 pageIndex : prefetchPages) {
        if (pageIndex < 0 ||
                pageIndex > stemdetail::pageIndexForColumn(totalColumns - 1,
                                    kPageWidth)) {
            continue;
        }
        const StemPageKey key = makePageKey(pageIndex);
        auto nodeIt = m_pageNodes.find(key);
        if (nodeIt != m_pageNodes.end()) {
            nodeIt->lastUsedFrame = m_frameCounter;
            continue;
        }
        const auto image = pageImage(key, pDetail, true);
        // Upload prefetched pages before they scroll into view. Doing the
        // texture upload at the page boundary would put a multi-megabyte
        // upload on the frame that switches pages. Skip it after a slow frame
        // so an upload never extends an already tight frame budget.
        constexpr qint64 kPrefetchUploadBudgetUs = 4000;
        if (image && !m_pageNodes.contains(key) && !prefetchedUpload &&
                m_lastPreprocessUs < kPrefetchUploadBudgetUs) {
            acquirePageNode(key, *image);
            prefetchedUpload = true;
        }
    }

    if (!anyReady && !m_lastDrawnPages.isEmpty()) {
        // The new page set is still being built (zoom, resize or seek). Draw
        // the previous pages at their old scale, mapped to the current sample
        // position, instead of blinking the picture out.
        for (const StemPageKey& pageKey : std::as_const(m_lastDrawnPages)) {
            drawPage(pageKey, false, false);
        }
    }

    if (!currentPageKeys.isEmpty()) {
        m_lastDrawnPages = currentPageKeys;
    }

    // Clear pages that left the viewport, then evict the least recently used
    // GPU pages. Textures for recently visited pages are kept so seeking back
    // does not upload them again.
    for (auto it = m_pageNodes.begin(); it != m_pageNodes.end(); ++it) {
        if (!visibleKeys.contains(it.key()) && it->hasDrawKey) {
            it->pNode->clear();
            it->hasDrawKey = false;
        }
    }
    while (m_pageNodes.size() > kMaxGpuPages) {
        auto oldest = m_pageNodes.end();
        for (auto it = m_pageNodes.begin(); it != m_pageNodes.end(); ++it) {
            if (visibleKeys.contains(it.key())) {
                continue;
            }
            if (oldest == m_pageNodes.end() ||
                    it->lastUsedFrame < oldest->lastUsedFrame) {
                oldest = it;
            }
        }
        if (oldest == m_pageNodes.end()) {
            break;
        }
        const StemPageKey key = oldest.key();
        auto pNode = detachChildNode(oldest->pNode);
        m_pageNodes.erase(oldest);
        pNode.reset();
        m_lastDrawnPages.removeAll(key);
    }

    if (m_pDetail.get() != pDetail.get()) {
        m_pDetail = pDetail;
    }
    m_hasGeometry = anyReady || !m_lastDrawnPages.isEmpty() || m_hasGeometry;
    return m_hasGeometry;
}

} // namespace allshader
