#pragma once

#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include <QImage>

#include "rendergraph/geometrynode.h"
#include "util/class.h"
#include "util/math.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"
#include "waveform/stemdetail/stemdetailtrack.h"

class QOpenGLTexture;
class ControlProxy;

namespace allshader {
class WaveformRendererStem;
class StemDetailTextureNode;
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

    std::vector<std::unique_ptr<ControlProxy>> m_pStemGain;
    std::vector<std::unique_ptr<ControlProxy>> m_pStemMute;
    std::unique_ptr<ControlProxy> m_pPregain;

    QVarLengthArray<int, mixxx::kMaxSupportedStems> m_stackOrder;

    bool m_hasGeometry;
    StemDetailTextureNode* m_pTextureNode;
    QImage m_textureImage;
    std::shared_ptr<const StemDetailTrack> m_pDetail;
    GeometryKey m_geometryKey;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererStem);
};
