#include "waveform/renderers/allshader/waveformrendererstem.h"

#include <QFont>
#include <QImage>
#include <QOpenGLTexture>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

#include "control/controlproxy.h"
#include "engine/channels/enginedeck.h"
#include "engine/engine.h"
#include "rendergraph/material/rgbamaterial.h"
#include "rendergraph/vertexupdaters/rgbavertexupdater.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/math.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "waveform/waveformwidgetfactory.h"

namespace {
#ifdef __SCENEGRAPH__
// FIXME this is a workaround an issue with waveform only drawing partially in
// SG. The workaround is to reduce the the number of vertices, by reducing the
// precision of waveform strips.
const int kMinColumnPixels = 2;
#else
const int kMinColumnPixels = 1;
#endif

// Columns cover at least this many summary frames, so that individual waves
// stay readable when zoomed in (chunky, Traktor-like columns) instead of being
// drawn as sub-pixel slivers.
const double kMinFramesPerColumn = 1.5;
// Upper bound for the width of a drawn column, in device pixels. Keeping this
// small is what preserves detail when zoomed in: a 24 px cap turned every
// column into a wide block and made close-up waveforms look pixelated.
const int kMaxColumnPixels = 8;

// Per-lane scaling. Every stem is normalised against its own 95th percentile so
// each lane fills its height the way the reference does; the relative loudness
// between stems is then read from the stem faders rather than from lane height.
const float kLaneFill = 0.98f;
// Stems whose whole-track peak is below this are treated as (near) silent and
// are not amplified, so their noise floor never fills a lane.
const float kMinStemPeak = 8.0f;
// Opacity of a muted stem lane. The shape and colour stay readable, but clearly
// ghosted so a muted stem is obviously out of the mix.
const float kGhostedStemOpacity = 0.32f;
// Spectrum ramp used inside every stem lane, matched to the Traktor reference:
// lows orange, mids green, highs magenta. The low/mid pair blends into the warm
// side (yellow to green) and the *share* of high energy then pulls the hue
// across to magenta once it dominates a column, which is what makes a drum lane
// with cymbals read magenta while a vocal stays green.
constexpr float kStemLowHue = 0.08f; // amber / orange
constexpr float kStemMidHue = 0.38f; // green
constexpr float kStemHighHue = 0.84f; // magenta
// Mid is nudged up, high is only used as a share: at the same loudness highs
// carry far less amplitude than lows, so raw amplitudes would never reach the
// magenta end.
constexpr float kStemLowGain = 1.00f;
constexpr float kStemMidGain = 1.10f;
constexpr float kStemSaturation = 0.86f;
// Share of high-band energy at which a column has fully turned magenta.
constexpr float kStemHighMixLow = 0.10f;
constexpr float kStemHighMixHigh = 0.34f;
// Each lane is anchored to its reference hue (drums magenta, bass amber, other
// cyan, vocals green) and the spectrum may push it at most this far either way
// around the wheel. That reproduces the reference exactly: drums drift between
// magenta and pink, bass between amber and orange, other between cyan and green,
// vocals between green and cyan, without any lane wandering into another's
// family.
constexpr float kStemFamilyThrow = 0.09f;
constexpr float kStemDrumsHue = 0.811f; // #C13BD6
constexpr float kStemBassHue = 0.083f; // #F08A24
constexpr float kStemOtherHue = 0.514f; // #22C7D6
constexpr float kStemVocalsHue = 0.410f; // #2FBF71

/// Reference hue for a stem, matched by label the same way the QML lane strip
/// classifies them.
inline float stemFamilyHue(const QString& label, int stemIdx) {
    const QString value = label.toLower();
    if (value.contains(QStringLiteral("vocal")) ||
            value.contains(QStringLiteral("acapella")) ||
            value.contains(QStringLiteral("voice"))) {
        return kStemVocalsHue;
    }
    if (value.contains(QStringLiteral("drum"))) {
        return kStemDrumsHue;
    }
    if (value.contains(QStringLiteral("bass"))) {
        return kStemBassHue;
    }
    if (value.contains(QStringLiteral("other"))) {
        return kStemOtherHue;
    }
    constexpr float kDefaultHues[4] = {kStemDrumsHue,
            kStemBassHue,
            kStemOtherHue,
            kStemVocalsHue};
    return kDefaultHues[std::clamp(stemIdx, 0, 3)];
}

inline float smoothStep(float edge0, float edge1, float value) {
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}
} // namespace

using namespace rendergraph;

namespace allshader {

WaveformRendererStem::WaveformRendererStem(
        WaveformWidgetRenderer* waveformWidget,
        ::WaveformRendererAbstract::PositionSource type,
        ::WaveformRendererSignalBase::Options options)
        : WaveformRendererSignalBase(waveformWidget, options),
          m_isSlipRenderer(type == ::WaveformRendererAbstract::Slip),
          m_splitStemTracks(false),
          m_outlineOpacity(0.15f),
          m_opacity(0.75f) {
    initForRectangles<RGBAMaterial>(0);
    setUsePreprocess(true);
}

void WaveformRendererStem::onSetup(const QDomNode&) {
}

bool WaveformRendererStem::init() {
    m_pStemGain.clear();
    m_pStemMute.clear();

    // Keep a valid drawing order even for displays that provide a static track
    // without a channel group. The stem data itself is independent of the
    // per-stem volume/mute controls, so those controls are optional.
    m_stackOrder.resize(mixxx::kMaxSupportedStems);
    std::iota(m_stackOrder.begin(), m_stackOrder.end(), 0);

    if (m_waveformRenderer->getGroup().isEmpty()) {
        return true;
    }
    for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; stemIdx++) {
        QString stemGroup = EngineDeck::getGroupForStem(m_waveformRenderer->getGroup(), stemIdx);
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
        };
        m_pStemGain.back()->connectValueChanged(this, bringToForeground);
        m_pStemMute.back()->connectValueChanged(this, bringToForeground);
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
    return true;
}

void WaveformRendererStem::preprocess() {
    if (!preprocessInner()) {
        if (geometry().vertexCount() != 0) {
            geometry().allocate(0);
            markDirtyGeometry();
        }
    }
}

bool WaveformRendererStem::preprocessInner() {
    TrackPointer pTrack = m_waveformRenderer->getTrackInfo();

    if (!pTrack || (m_isSlipRenderer && !m_waveformRenderer->isSlipActive())) {
        return false;
    }

    auto stemInfo = pTrack->getStemInfo();
    // If this track isn't a stem track, skip the rendering
    if (stemInfo.isEmpty()) {
        return false;
    }
    auto positionType = m_isSlipRenderer ? ::WaveformRendererAbstract::Slip
                                         : ::WaveformRendererAbstract::Play;

    ConstWaveformPointer waveform = pTrack->getWaveform();
    if (waveform.isNull()) {
        return false;
    }

    const int dataSize = waveform->getDataSize();
    if (dataSize <= 1) {
        return false;
    }

    const WaveformData* data = waveform->data();
    if (data == nullptr) {
        return false;
    }
    // If this waveform doesn't contain stem data, skip the rendering
    if (!waveform->hasStem()) {
        return false;
    }

    const uint selectedStems = m_waveformRenderer->getSelectedStems();

    // Number of stems that are actually drawn (the stack order never holds more
    // entries than the maximum number of supported stems).
    const int stemCount = static_cast<int>(std::min<qsizetype>(
            stemInfo.size(), m_stackOrder.size()));
    if (stemCount <= 0) {
        return false;
    }

    const float devicePixelRatio = m_waveformRenderer->getDevicePixelRatio();
    const int length = static_cast<int>(m_waveformRenderer->getLength());
    const int pixelLength = std::max(1,
            static_cast<int>(m_waveformRenderer->getLength() * devicePixelRatio));
    const float invDevicePixelRatio = 1.f / devicePixelRatio;

    // See waveformrenderersimple.cpp for a detailed explanation of the frame and index calculation
    const int visualFramesSize = dataSize / 2;
    const double firstVisualFrame =
            m_waveformRenderer->getFirstDisplayedPosition(positionType) * visualFramesSize;
    const double lastVisualFrame =
            m_waveformRenderer->getLastDisplayedPosition(positionType) * visualFramesSize;

    // Represents the # of visual frames per horizontal pixel.
    const double visualIncrementPerPixel =
            (lastVisualFrame - firstVisualFrame) / static_cast<double>(pixelLength);
    if (visualIncrementPerPixel == 0.0) {
        return false;
    }

    // Per-band gain from the EQ knobs.
    float allGain(1.0);
    getGains(&allGain, nullptr, nullptr, nullptr);

    const float breadth = static_cast<float>(m_waveformRenderer->getBreadth());
    const float stemBreadth = m_splitStemTracks
            ? breadth / static_cast<float>(mixxx::kMaxSupportedStems)
            : 0.f;
    const float halfBreadth = (m_splitStemTracks ? stemBreadth : breadth) / 2.0f;

    const float heightFactor = allGain * halfBreadth / m_maxValue;

    // Column width: at close zoom several summary frames are aggregated into one
    // drawn column so that single waves read as chunky blocks instead of being
    // smeared into a solid area.
    int columnPixels = kMinColumnPixels;
    if (visualIncrementPerPixel > 0.0) {
        columnPixels = static_cast<int>(
                std::ceil(kMinFramesPerColumn / visualIncrementPerPixel));
    }
    columnPixels = std::clamp(columnPixels, kMinColumnPixels, kMaxColumnPixels);
    const int columnCount = std::max(1, pixelLength / columnPixels);
    const double framesPerColumn = visualIncrementPerPixel * columnPixels;
    const float columnWidth = columnPixels * invDevicePixelRatio;

    // Effective visual frame for x
    const double firstXVisualFrame = qRound(firstVisualFrame / framesPerColumn) *
            framesPerColumn;

    const double maxSamplingRange = framesPerColumn / 2.0;

    // Nothing may be drawn before the start of the track (visual frame 0). Skip
    // the columns that only cover the pre-track area instead of sampling the
    // first frame over and over again.
    const int firstSignalColumn = countColumnsBeforeTrackStart(
            firstXVisualFrame, framesPerColumn, maxSamplingRange, columnCount);
    const int numSignalColumns = columnCount - firstSignalColumn;
    double xVisualFrame = firstXVisualFrame +
            firstSignalColumn * framesPerColumn;

    // Per-stem display detail (peak/RMS/bands) with a multi-resolution pyramid
    // that was derived from the stored summary, so nothing is recomputed here
    // while scrolling.
    const bool hasStemColumns = waveform->hasStemColumns();
    int lodLevel = 0;
    if (hasStemColumns) {
        while (lodLevel + 1 < waveform->getStemColumnLevelCount() &&
                Waveform::stemColumnStride(lodLevel + 1) <= framesPerColumn) {
            ++lodLevel;
        }
    }
    const int lodStride = Waveform::stemColumnStride(lodLevel);
    const WaveformStemColumn* pLodColumns =
            hasStemColumns ? waveform->getStemColumnData(lodLevel) : nullptr;
    const int lodColumnCount = hasStemColumns ? waveform->getStemColumnCount(lodLevel) : 0;

    // Reference hue per stem, used to keep each lane recognisable while its
    // spectrum still moves the colour inside that family.
    float familyHue[mixxx::kMaxSupportedStems]{};
    for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
        familyHue[stemIdx] = stemIdx < stemInfo.size()
                ? stemFamilyHue(stemInfo[stemIdx].getLabel(), stemIdx)
                : kStemOtherHue;
    }

    // Per-lane scaling: each stem is normalised by its own 95th percentile, so
    // every lane fills its height. The shared reference the override used to
    // apply is gone on purpose: the faders, not the waveform, now carry the
    // relative loudness of the stems.
    float laneScale[mixxx::kMaxSupportedStems]{};
    for (int stemIdx = 0; stemIdx < mixxx::kMaxSupportedStems; ++stemIdx) {
        laneScale[stemIdx] = 1.f;
    }
    if (m_splitStemTracks) {
        for (int stemIdx = 0; stemIdx < stemCount; ++stemIdx) {
            const float stemPeak = static_cast<float>(waveform->getStemPeak(stemIdx));
            const float denominator = std::max(stemPeak, kMinStemPeak);
            laneScale[stemIdx] = kLaneFill / denominator * m_stemVisualGain[stemIdx];
        }
    }

    const int numVerticesPerLine = 6; // 2 triangles

    // Two layers (soft edge and dense core) per stem and column, plus the axis.
    const int reserved = numVerticesPerLine *
            (stemCount * 2 * numSignalColumns + 1);

    geometry().setDrawingMode(Geometry::DrawingMode::Triangles);
    geometry().allocate(reserved);
    markDirtyGeometry();

    RGBAVertexUpdater vertexUpdater{geometry().vertexDataAs<Geometry::RGBAColoredPoint2D>()};
    vertexUpdater.addRectangle({0.f,
                                       halfBreadth - 0.5f},
            {static_cast<float>(length),
                    m_isSlipRenderer ? halfBreadth : halfBreadth + 0.5f},
            {0.f, 0.f, 0.f, 0.f});

    for (int column = firstSignalColumn; column < columnCount; ++column) {
        // Aggregate the per-stem detail of all summary frames this column covers.
        float peak[mixxx::kMaxSupportedStems]{};
        float rms[mixxx::kMaxSupportedStems]{};
        float bandLow[mixxx::kMaxSupportedStems]{};
        float bandMid[mixxx::kMaxSupportedStems]{};
        float bandHigh[mixxx::kMaxSupportedStems]{};

        if (hasStemColumns) {
            const int frameStart = std::clamp(
                    static_cast<int>(std::floor(xVisualFrame - maxSamplingRange)),
                    0,
                    visualFramesSize);
            const int frameStop = std::clamp(
                    static_cast<int>(std::ceil(xVisualFrame + maxSamplingRange)),
                    frameStart,
                    visualFramesSize);
            const int firstLod = std::min(lodColumnCount, frameStart / lodStride);
            const int lastLod = std::min(lodColumnCount,
                    std::max(firstLod + 1, frameStop / lodStride + 1));
            int divisor = 0;
            for (int idx = firstLod; idx < lastLod; ++idx) {
                const WaveformStemColumn& detail = pLodColumns[idx];
                for (int stemIdx = 0; stemIdx < stemCount; ++stemIdx) {
                    peak[stemIdx] = std::max(peak[stemIdx],
                            static_cast<float>(detail.peak[stemIdx]));
                    rms[stemIdx] = std::max(rms[stemIdx],
                            static_cast<float>(detail.rms[stemIdx]));
                    bandLow[stemIdx] += stemBandField(detail.bands[stemIdx], 0);
                    bandMid[stemIdx] += stemBandField(detail.bands[stemIdx], 1);
                    bandHigh[stemIdx] += stemBandField(detail.bands[stemIdx], 2);
                }
                ++divisor;
            }
            if (divisor > 0) {
                const float invDivisor = 1.f / static_cast<float>(divisor);
                const float invBandMax = invDivisor / static_cast<float>(kStemBandMax);
                for (int stemIdx = 0; stemIdx < stemCount; ++stemIdx) {
                    peak[stemIdx] /= m_maxValue;
                    rms[stemIdx] /= m_maxValue;
                    bandLow[stemIdx] *= invBandMax;
                    bandMid[stemIdx] *= invBandMax;
                    bandHigh[stemIdx] *= invBandMax;
                }
            }
        } else {
            // Summaries written before the per-stem band detail existed only
            // carry the peak per stem. Render them with a flat colour until the
            // track has been re-analysed.
            const int visualFrameStart = std::lround(xVisualFrame - maxSamplingRange);
            const int visualFrameStop = std::lround(xVisualFrame + maxSamplingRange);

            const int visualIndexStart = std::max(visualFrameStart * 2, 0);
            const int visualIndexStop = std::min(
                    std::max(visualFrameStop, visualFrameStart + 1) * 2, dataSize - 1);

            for (int stemIdx = 0; stemIdx < stemCount; ++stemIdx) {
                uchar u8max{};
                for (int chn = 0; chn < 2; ++chn) {
                    // data is interleaved left / right
                    for (int i = visualIndexStart + chn; i < visualIndexStop + chn; i += 2) {
                        u8max = math_max(u8max, data[i].stems[stemIdx]);
                    }
                }
                peak[stemIdx] = static_cast<float>(u8max) / m_maxValue;
                rms[stemIdx] = peak[stemIdx];
            }
        }

        const float fpos = static_cast<float>(column * columnPixels) * invDevicePixelRatio;

        int stemLayer = 0;
        for (int stemIdx : std::as_const(m_stackOrder)) {
            if (stemIdx >= stemCount) {
                continue;
            }
            // Muted (or solo-excluded) stems stay visible as a ghost, so it is
            // obvious which lane was muted and where it sits in the track.
            const bool stemMuted = selectedStems
                    ? !(selectedStems & 1 << stemIdx)
                    : (!m_pStemMute.empty() && m_pStemMute[stemIdx]->toBool());

            const float stemPeak = peak[stemIdx];
            const float stemRms = rms[stemIdx];

            // Height of the loudest point of this column. The floor keeps very
            // short columns on the pixel grid: without it a quiet column could
            // round away completely and leave the lane looking dashed.
            float height = m_splitStemTracks
                    ? halfBreadth * laneScale[stemIdx] * stemPeak * m_maxValue
                    : heightFactor * stemPeak * m_maxValue * allGain;
            height = std::clamp(height, 0.f, halfBreadth);
            if (stemPeak > 0.f) {
                height = std::max(height, 0.75f);
            }

            // The dense core is only as tall as the RMS content of the column,
            // the region between core and peak is the softer peak envelope.
            const float coreRatio = stemPeak > 0.0001f
                    ? std::clamp(stemRms / stemPeak, 0.35f, 1.f)
                    : 1.f;

            // Colour comes from the column's own spectrum, matched to the
            // reference palette: lows amber, mids green, and a high-band share
            // that pulls the hue to magenta. There is no per-stem base colour.
            const float lowEnergy = bandLow[stemIdx];
            const float midEnergy = bandMid[stemIdx];
            const float highEnergy = bandHigh[stemIdx];
            const float bandTotal = lowEnergy + midEnergy + highEnergy;

            const float warmWeight = lowEnergy * kStemLowGain + midEnergy * kStemMidGain;
            float hue = kStemLowHue;
            if (warmWeight > 0.0001f) {
                hue = (kStemLowHue * lowEnergy * kStemLowGain +
                              kStemMidHue * midEnergy * kStemMidGain) /
                        warmWeight;
            }
            const float highShare = bandTotal > 0.0001f ? highEnergy / bandTotal : 0.f;
            hue += (kStemHighHue - hue) *
                    smoothStep(kStemHighMixLow, kStemHighMixHigh, highShare);
            // Keep the lane inside its reference family: the spectrum decides
            // where inside the family the colour sits, the family decides the
            // neighbourhood. Without the clamp a low-heavy drum column would
            // wander into the bass amber, which the reference never does.
            float hueDelta = hue - familyHue[stemIdx];
            if (hueDelta > 0.5f) {
                hueDelta -= 1.f;
            } else if (hueDelta < -0.5f) {
                hueDelta += 1.f;
            }
            hue = familyHue[stemIdx] +
                    std::clamp(hueDelta, -kStemFamilyThrow, kStemFamilyThrow);
            hue -= std::floor(hue);

            // Brightness still follows the level, so loud columns punch and
            // quiet ones recede; the hue itself stays purely spectral.
            const float level = std::clamp(std::sqrt(stemPeak), 0.f, 1.f);
            const float value = math_clamp(0.78f + 0.32f * level, 0.f, 1.f);
            const float saturation = kStemSaturation;

            QColor edgeColor = QColor::fromHsvF(hue, saturation, value * 0.88f);
            QColor coreColor = QColor::fromHsvF(
                    hue, saturation, math_clamp(value * 1.12f, 0.f, 1.f));

            // Solid lanes: the opacity stays constant across the column, so quiet
            // sections keep their colour instead of fading away. The peak
            // envelope is the darker outer shape, the RMS core sits on top of it
            // and reads brighter.
            const float laneAlpha = math_clamp(0.45f + 0.60f * m_opacity, 0.f, 1.f);
            float edgeAlpha = laneAlpha * 0.62f * (0.70f + 0.60f * m_outlineOpacity);
            float coreAlpha = laneAlpha;

            if (stemMuted) {
                edgeColor = QColor::fromHsvF(
                        hue, saturation * 0.25f, value * 0.75f);
                coreColor = QColor::fromHsvF(
                        hue, saturation * 0.25f, math_clamp(value * 0.95f, 0.f, 1.f));
                edgeAlpha *= kGhostedStemOpacity;
                coreAlpha *= kGhostedStemOpacity;
            }
            edgeAlpha = math_clamp(edgeAlpha, 0.f, 1.f);
            coreAlpha = math_clamp(coreAlpha, 0.f, 1.f);

            const int yIndex = m_splitStemTracks ? stemIdx : stemLayer;
            const float centerY = yIndex * stemBreadth + halfBreadth;

            // Soft outer edge: full peak height, low opacity, so the tips fade
            // out instead of ending in a hard clip.
            vertexUpdater.addRectangle(
                    {fpos, centerY - height},
                    {fpos + columnWidth,
                            m_isSlipRenderer ? centerY : centerY + height},
                    {static_cast<float>(edgeColor.redF()),
                            static_cast<float>(edgeColor.greenF()),
                            static_cast<float>(edgeColor.blueF()),
                            edgeAlpha});

            // Dense interior driven by the RMS content of the column.
            const float coreHeight = height * coreRatio;
            vertexUpdater.addRectangle(
                    {fpos, centerY - coreHeight},
                    {fpos + columnWidth,
                            m_isSlipRenderer ? centerY : centerY + coreHeight},
                    {static_cast<float>(coreColor.redF()),
                            static_cast<float>(coreColor.greenF()),
                            static_cast<float>(coreColor.blueF()),
                            coreAlpha});

            stemLayer++;
        }

        xVisualFrame += framesPerColumn;
    }

    DEBUG_ASSERT(reserved == vertexUpdater.index());

    markDirtyMaterial();

    return true;
}

} // namespace allshader
