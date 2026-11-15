#pragma once

#include <array>
#include <vector>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "util/math.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

class QOpenGLTexture;
class ControlProxy;
struct WaveformData;

namespace allshader {
class WaveformRendererStem;
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
    }
    void setOutlineOpacity(float value) {
        m_outlineOpacity = value;
        markDirtyGeometry();
    }
    void setOpacity(float value) {
        m_opacity = value;
        markDirtyGeometry();
    }
    /// Per-stem visual gain applied on top of the lane fill derived from the
    /// shared reference level (see kLaneFillMin/kLaneFillMax).
    void setStemVisualGain(int stemIdx, float value) {
        if (stemIdx < 0 || stemIdx >= mixxx::kMaxSupportedStems) {
            return;
        }
        m_stemVisualGain[stemIdx] = math_clamp(value, 0.0f, 8.0f);
        markDirtyGeometry();
    }

  private:
    bool m_isSlipRenderer;
    bool m_splitStemTracks;

    bool m_reorderOnChange;
    float m_outlineOpacity;
    float m_opacity;

    std::vector<std::unique_ptr<ControlProxy>> m_pStemGain;
    std::vector<std::unique_ptr<ControlProxy>> m_pStemMute;

    QVarLengthArray<int, mixxx::kMaxSupportedStems> m_stackOrder;

    // Per-stem visual gain on top of the automatic lane fill.
    std::array<float, mixxx::kMaxSupportedStems> m_stemVisualGain{1.f, 1.f, 1.f, 1.f};

    bool preprocessInner();

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererStem);
};
