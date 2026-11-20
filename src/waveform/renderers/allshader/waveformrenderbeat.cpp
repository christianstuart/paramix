#include "waveform/renderers/allshader/waveformrenderbeat.h"

#include <QDomNode>
#include <QVector4D>
#include <algorithm>

#include "audio/frame.h"
#include "engine/engine.h"
#include "moc_waveformrenderbeat.cpp"
#include "rendergraph/geometry.h"
#include "rendergraph/material/rgbamaterial.h"
#include "rendergraph/vertexupdaters/rgbavertexupdater.h"
#include "skin/legacy/skincontext.h"
#include "track/beats.h"
#include "track/track.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "waveform/waveformwidgetfactory.h"
#include "widget/wskincolor.h"

using namespace rendergraph;

namespace {

/// Downbeats are drawn with a stronger alpha than the ordinary beat lines so
/// they read as bar starts even at the far zoom levels where the extra width
/// alone is hard to see.
constexpr float kDownbeatOpacityBoost = 1.25f;

/// Fraction of the downbeat opacity used when the bar pattern was assumed
/// (no downbeat metadata). The width stays the same, so an assumed bar line
/// is still visible but clearly dimmer than a detected one.
constexpr float kAssumedDownbeatOpacityFactor = 0.65f;

} // namespace

namespace allshader {

WaveformRenderBeat::WaveformRenderBeat(WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type)
        : ::WaveformRendererAbstract(waveformWidget),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip) {
    initForRectangles<RGBAMaterial>(0);
    setUsePreprocess(true);
}

void WaveformRenderBeat::setup(const QDomNode& node, const SkinContext& skinContext) {
    m_color = QColor(skinContext.selectString(node, QStringLiteral("BeatColor")));
    m_color = WSkinColor::getCorrectColor(m_color).toRgb();
}

void WaveformRenderBeat::draw(QPainter* painter, QPaintEvent* event) {
    Q_UNUSED(painter);
    Q_UNUSED(event);
    DEBUG_ASSERT(false);
}

void WaveformRenderBeat::preprocess() {
    if (!preprocessInner()) {
        geometry().allocate(0);
        markDirtyGeometry();
    }
}

bool WaveformRenderBeat::preprocessInner() {
    const TrackPointer trackInfo = m_waveformRenderer->getTrackInfo();

    if (!trackInfo || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return false;
    }

    const bool isStemTrack = trackInfo && trackInfo->hasStem() &&
            trackInfo->getWaveform() && trackInfo->getWaveform()->hasStem();
    const bool splitStemTracks = isStemTrack && WaveformWidgetFactory::isCreated() &&
            WaveformWidgetFactory::instance()->isStemSplitTracks();

    auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                         : ::WaveformRendererAbstract::Play;

    mixxx::BeatsPointer trackBeats = trackInfo->getBeats();
    if (!trackBeats) {
        return false;
    }

#ifndef __SCENEGRAPH__
    int alpha = m_waveformRenderer->getBeatGridAlpha();
    if (alpha == 0) {
        return false;
    }
    m_color.setAlphaF(alpha / 100.0f);
#endif

    if (!m_color.alpha()) {
        // Don't render the beatgrid lines is there are fully transparent
        return false;
    }

    // Bar pattern for the thick lines. Missing downbeat metadata is rendered
    // as an assumed 4/4 counted from the grid's first beat; the data itself
    // stays without a bar length (ADR-003).
    const mixxx::DownbeatInfo downbeatInfo = trackBeats->getDownbeatInfo();
    const downbeat::Pattern downbeatPattern = downbeat::patternFor(
            downbeatInfo.valid, downbeatInfo.beatsPerBar, downbeatInfo.phase);

    float beatR;
    float beatG;
    float beatB;
    float beatA;
    m_color.getRgbF(&beatR, &beatG, &beatB, &beatA);
    const QVector4D beatRgba(beatR, beatG, beatB, beatA);

    // Downbeats keep the ordinary beat hue and gain emphasis from the width
    // and a stronger alpha. Without a valid pattern they keep the width but
    // drop to a fraction of that alpha, so an assumed bar line never reads as
    // a detected downbeat.
    const QColor downbeatColor =
            m_downbeatColor.isValid() ? m_downbeatColor : m_color.lighter(140);
    float downbeatR;
    float downbeatG;
    float downbeatB;
    downbeatColor.getRgbF(&downbeatR, &downbeatG, &downbeatB);
    float downbeatAlpha = std::min(
            1.0f,
            beatA * kDownbeatOpacityBoost *
                    static_cast<float>(downbeatColor.alphaF()));
    if (downbeatPattern.assumed) {
        downbeatAlpha *= kAssumedDownbeatOpacityFactor;
    }
    const QVector4D downbeatRgba(downbeatR, downbeatG, downbeatB, downbeatAlpha);

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();

    const double trackSamples = m_waveformRenderer->getTrackSamples();
    if (trackSamples <= 0.0) {
        return false;
    }

    const double firstDisplayedPosition =
            m_waveformRenderer->getFirstDisplayedPosition(positionType);
    const double lastDisplayedPosition =
            m_waveformRenderer->getLastDisplayedPosition(positionType);

    // The beat grid must not be drawn before the start of the track. The first
    // displayed position is negative whenever the display extends into the
    // pre-track area, and auto-generated grids usually contain beats at
    // negative positions, which would paint beat lines left of frame 0.
    const auto trackStartPosition = mixxx::audio::kStartFramePos;
    const auto startPosition = std::max(
            mixxx::audio::FramePos::fromEngineSamplePos(
                    firstDisplayedPosition * trackSamples),
            trackStartPosition);
    const auto endPosition = mixxx::audio::FramePos::fromEngineSamplePos(
            lastDisplayedPosition * trackSamples);

    if (!startPosition.isValid() || !endPosition.isValid()) {
        return false;
    }

    const float rendererBreadth = m_waveformRenderer->getBreadth();

    const int numVerticesPerLine = 6; // 2 triangles

    auto it = trackBeats->iteratorFrom(startPosition);

    // Count the number of beats in the range to reserve space in the m_vertices vector.
    // Note that we could also use
    //   int numBearsInRange = trackBeats->numBeatsInRange(startPosition, endPosition);
    // for this, but there have been reports of that method failing with a DEBUG_ASSERT.
    int numBeatsInRange = 0;
    for (auto countIt = it;
            countIt != trackBeats->cend() && *countIt <= endPosition;
            ++countIt) {
        numBeatsInRange++;
    }

    // Index of the first beat in range, counted from the grid's first beat.
    // The iterator already counts beats from that origin, so this stays the
    // same for a given beat no matter where the viewport starts. A loop
    // counter that restarted here would make the bar lines slide while
    // scrolling. Pre-roll beats of a grid whose first beat is before frame 0
    // simply produce a negative index, which isDownbeat() handles.
    int beatIndex = 0;
    if (it != trackBeats->cend()) {
        beatIndex = static_cast<int>(it - trackBeats->cfirstmarker());
    }

    const int numBoxesPerBeat = (m_isSlipRenderer && splitStemTracks)
            ? mixxx::kMaxSupportedStems
            : 1;
    const int reserved = numBeatsInRange * numVerticesPerLine * numBoxesPerBeat;
    geometry().allocate(reserved);

    RGBAVertexUpdater vertexUpdater{
            geometry().vertexDataAs<Geometry::RGBAColoredPoint2D>()};

    const float boxBreadth = splitStemTracks
            ? rendererBreadth / static_cast<float>(mixxx::kMaxSupportedStems)
            : rendererBreadth;

    for (;
            it != trackBeats->cend() && *it <= endPosition;
            ++it, ++beatIndex) {
        double beatPosition = it->toEngineSamplePos();
        double xBeatPoint =
                m_waveformRenderer->transformSamplePositionInRendererWorld(
                        beatPosition, positionType);

        xBeatPoint = qRound(xBeatPoint * devicePixelRatio) / devicePixelRatio;

        const bool isDownbeat = downbeat::isDownbeat(beatIndex, downbeatPattern);
        // Widths are logical pixels: the scene graph scales the geometry by
        // the device pixel ratio, so a 3 px line covers 3 * dpr physical
        // pixels. Keep the beat line centred on the snapped beat position:
        // with a width of one this is exactly the old rectangle.
        const float lineWidth = isDownbeat ? std::max(1.0f, m_downbeatWidth) : 1.0f;
        const float x1 = static_cast<float>(xBeatPoint) + 0.5f * (1.0f - lineWidth);
        const float x2 = x1 + lineWidth;
        const QVector4D& color = isDownbeat ? downbeatRgba : beatRgba;

        if (m_isSlipRenderer && splitStemTracks) {
            for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
                const float posy1 = stemIdx * boxBreadth;
                const float posy2 = posy1 + boxBreadth / 2.f;
                vertexUpdater.addRectangle({x1, posy1}, {x2, posy2}, color);
            }
        } else {
            vertexUpdater.addRectangle({x1, 0.f},
                    {x2, m_isSlipRenderer ? rendererBreadth / 2 : rendererBreadth},
                    color);
        }
    }
    markDirtyGeometry();

    DEBUG_ASSERT(reserved == vertexUpdater.index());

    markDirtyMaterial();

    return true;
}

} // namespace allshader
