#pragma once

#include <QColor>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "waveform/renderers/waveformrendererabstract.h"

class QDomNode;
class SkinContext;

namespace allshader {
class WaveformRenderBeat;
} // namespace allshader

namespace allshader {
namespace downbeat {

/// Beats per bar assumed when the analysis carries no downbeat metadata.
/// ADR-003: an unknown bar length stays unknown in the data; this value only
/// drives the fallback rendering.
constexpr int kAssumedBeatsPerBar = 4;

/// Bar pattern used to decide which beats start a bar.
struct Pattern {
    int beatsPerBar = kAssumedBeatsPerBar;
    /// Zero-based index of the first downbeat relative to the grid's first
    /// beat. A freshly analyzed Paramix grid has its anchor on the downbeat,
    /// so the phase is 0 there.
    int phase = 0;
    /// True when the pattern was assumed rather than read from the analysis.
    bool assumed = true;
};

/// Resolve the bar pattern from the additive downbeat metadata stored in the
/// beat grid sub-version. Absent or inconsistent metadata falls back to an
/// assumed 4/4 with phase 0 instead of trusting the struct defaults.
inline Pattern patternFor(bool hasDownbeatInfo, int beatsPerBar, int phase) {
    Pattern pattern;
    if (!hasDownbeatInfo || beatsPerBar <= 0 || phase < 0 || phase >= beatsPerBar) {
        return pattern;
    }
    pattern.beatsPerBar = beatsPerBar;
    pattern.phase = phase;
    pattern.assumed = false;
    return pattern;
}

/// True when the beat at `beatIndex`, counted from the grid's first beat,
/// starts a bar. Negative indexes are the pre-roll beats of a grid whose first
/// beat lies before frame 0; they are handled by the same modulo so the bar
/// pattern does not shift when the track start is scrolled into view.
inline bool isDownbeat(int beatIndex, const Pattern& pattern) {
    const int beatsPerBar = pattern.beatsPerBar > 0 ? pattern.beatsPerBar
                                                    : kAssumedBeatsPerBar;
    const int barOffset = (beatIndex - pattern.phase) % beatsPerBar;
    return ((barOffset + beatsPerBar) % beatsPerBar) == 0;
}

} // namespace downbeat
} // namespace allshader

class allshader::WaveformRenderBeat final
        : public QObject,
          public ::WaveformRendererAbstract,
          public rendergraph::GeometryNode {
    Q_OBJECT
  public:
    explicit WaveformRenderBeat(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play);

    // Pure virtual from WaveformRendererAbstract, not used
    void draw(QPainter* painter, QPaintEvent* event) override final;

    void setup(const QDomNode& node, const SkinContext& skinContext) override;

    // Virtuals for rendergraph::Node
    void preprocess() override;

  public slots:
    void setColor(const QColor& color) {
        m_color = color;
    }
    void setDownbeatColor(const QColor& color) {
        m_downbeatColor = color;
    }
    /// Width of a downbeat line in logical pixels. The scene graph scales the
    /// geometry by the device pixel ratio, so the physical width follows it.
    void setDownbeatWidth(float width) {
        m_downbeatWidth = width;
    }

  private:
    QColor m_color;
    QColor m_downbeatColor;
    float m_downbeatWidth{3.0f};
    bool m_isSlipRenderer;

    bool preprocessInner();

    DISALLOW_COPY_AND_ASSIGN(WaveformRenderBeat);
};
