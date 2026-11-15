#pragma once

#include <QFlags>
#include <cmath>
#include <limits>

#include "rendergraph/node.h"
#include "util/class.h"
#include "waveform/renderers/waveformrenderersignalbase.h"

class WaveformWidgetRenderer;

namespace allshader {
class WaveformRendererSignalBase;
} // namespace allshader

/// Number of leading horizontal pixels of a waveform display that lie entirely
/// before the start of the track (visual frame 0).
///
/// The displayed frame range starts before the track when the play position is
/// close to the beginning (or when the play marker is not at the left edge), so
/// the first pixels of such a display would otherwise sample the first frame
/// repeatedly and paint a clamped copy of it in the pre-track area. Renderers
/// must skip these pixels and keep that area free of signal geometry.
///
/// @param firstXVisualFrame visual frame of the first pixel (pos 0)
/// @param visualIncrementPerPixel visual frames covered by one pixel
/// @param maxSamplingRange half the sampling range of a pixel, in frames
///        (i.e. visualIncrementPerPixel / 2); the pixel is drawable as soon as
///        its sampling window reaches frame 0 (matching std::lround, which
///        rounds -0.5 away from zero)
/// @param columns total number of columns (pixels or strips) of the display
inline int countColumnsBeforeTrackStart(double firstXVisualFrame,
        double visualIncrementPerPixel,
        double maxSamplingRange,
        int columns) {
    int pos = 0;
    while (pos < columns &&
            std::lround(firstXVisualFrame + maxSamplingRange) < 0) {
        ++pos;
        firstXVisualFrame += visualIncrementPerPixel;
    }
    return pos;
}

class allshader::WaveformRendererSignalBase : public ::WaveformRendererSignalBase {
  public:
    void draw(QPainter* painter, QPaintEvent* event) override final;

    static constexpr float m_maxValue{static_cast<float>(std::numeric_limits<uint8_t>::max())};

    explicit WaveformRendererSignalBase(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererSignalBase::Options options);

    virtual bool supportsSlip() const {
        return false;
    }

  public slots:
    void setAxesColor(const QColor& axesColor);
    void setColor(const QColor& lowColor);
    void setLowColor(const QColor& lowColor);
    void setMidColor(const QColor& midColor);
    void setHighColor(const QColor& highColor);

    void setIgnoreStem(bool value) {
        m_ignoreStem = value;
    }

  protected:
    bool m_ignoreStem;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererSignalBase);
};
