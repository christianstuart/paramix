#pragma once

#include <QImage>
#include <QQuickItem>
#include <QUrl>
#include <memory>

class ControlProxy;
class QSGNode;
class QSGSimpleRectNode;
class QSGSimpleTextureNode;
class QTimer;

namespace mixxx {
namespace qml {

/// Scene-graph VU meter with peak hold, replacing the binding-driven QML
/// implementation used by the LateNight skin.
///
/// The QML version ran a repeating 10 ms Timer per instance and drove the
/// meter through bindings (level -> clipped item height -> anchors), so 13
/// meters meant ~1300 binding/layout updates per second. Here a single shared
/// 30 Hz ticker decays the peak in C++ and the meter is drawn with a handful
/// of textured quads, so a level change costs one `update()` instead of a
/// QML binding storm.
class QmlVuMeterItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(VuMeter)
    Q_PROPERTY(bool clipping READ isClipping NOTIFY clippingChanged)
    Q_PROPERTY(int clipHeight READ clipHeight WRITE setClipHeight NOTIFY clipHeightChanged)
    Q_PROPERTY(QUrl clipActiveSource READ clipActiveSource WRITE setClipActiveSource NOTIFY
                    clipActiveSourceChanged)
    Q_PROPERTY(QUrl clipBackgroundSource READ clipBackgroundSource WRITE
                    setClipBackgroundSource NOTIFY clipBackgroundSourceChanged)
    Q_PROPERTY(bool drawGroove READ drawGroove WRITE setDrawGroove NOTIFY drawGrooveChanged)
    Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
    Q_PROPERTY(qreal heldPeak READ heldPeak NOTIFY heldPeakChanged)
    Q_PROPERTY(qreal level READ level NOTIFY levelChanged)
    Q_PROPERTY(QUrl levelActiveSource READ levelActiveSource WRITE setLevelActiveSource NOTIFY
                    levelActiveSourceChanged)
    Q_PROPERTY(QUrl levelBackgroundSource READ levelBackgroundSource WRITE
                    setLevelBackgroundSource NOTIFY levelBackgroundSourceChanged)
    Q_PROPERTY(int levelHeight READ levelHeight WRITE setLevelHeight NOTIFY levelHeightChanged)
    Q_PROPERTY(QString levelKey READ levelKey WRITE setLevelKey NOTIFY levelKeyChanged)
    Q_PROPERTY(int peakFallTime READ peakFallTime WRITE setPeakFallTime NOTIFY peakFallTimeChanged)
    Q_PROPERTY(int peakHoldSize READ peakHoldSize WRITE setPeakHoldSize NOTIFY peakHoldSizeChanged)
    Q_PROPERTY(QString peakKey READ peakKey WRITE setPeakKey NOTIFY peakKeyChanged)

  public:
    explicit QmlVuMeterItem(QQuickItem* pParent = nullptr);
    ~QmlVuMeterItem() override;

    bool isClipping() const {
        return m_clipping;
    }
    int clipHeight() const {
        return m_clipHeight;
    }
    const QUrl& clipActiveSource() const {
        return m_clipActiveSource;
    }
    const QUrl& clipBackgroundSource() const {
        return m_clipBackgroundSource;
    }
    bool drawGroove() const {
        return m_drawGroove;
    }
    const QString& group() const {
        return m_group;
    }
    qreal heldPeak() const {
        return m_heldPeak;
    }
    qreal level() const {
        return m_level;
    }
    const QUrl& levelActiveSource() const {
        return m_levelActiveSource;
    }
    const QUrl& levelBackgroundSource() const {
        return m_levelBackgroundSource;
    }
    int levelHeight() const {
        return m_levelHeight;
    }
    const QString& levelKey() const {
        return m_levelKey;
    }
    int peakFallTime() const {
        return m_peakFallTime;
    }
    int peakHoldSize() const {
        return m_peakHoldSize;
    }
    const QString& peakKey() const {
        return m_peakKey;
    }

    void setClipActiveSource(const QUrl& url);
    void setClipBackgroundSource(const QUrl& url);
    void setClipHeight(int height);
    void setDrawGroove(bool drawGroove);
    void setGroup(const QString& group);
    void setLevelActiveSource(const QUrl& url);
    void setLevelBackgroundSource(const QUrl& url);
    void setLevelHeight(int height);
    void setLevelKey(const QString& key);
    void setPeakFallTime(int ms);
    void setPeakHoldSize(int size);
    void setPeakKey(const QString& key);

    /// Called by the shared ticker at ~30 Hz.
    void tick();

  signals:
    void clippingChanged();
    void clipHeightChanged();
    void clipActiveSourceChanged();
    void clipBackgroundSourceChanged();
    void drawGrooveChanged();
    void groupChanged();
    void heldPeakChanged();
    void levelChanged();
    void levelActiveSourceChanged();
    void levelBackgroundSourceChanged();
    void levelHeightChanged();
    void levelKeyChanged();
    void peakFallTimeChanged();
    void peakHoldSizeChanged();
    void peakKeyChanged();

  protected:
    QSGNode* updatePaintNode(QSGNode* pOldNode, UpdatePaintNodeData*) override;

  private:
    void updateLevel();
    void updateNodeRects();
    void reconnectControls();
    static QImage loadImage(const QUrl& url);

    QString m_group;
    QString m_levelKey{QStringLiteral("vu_meter")};
    QString m_peakKey{QStringLiteral("peak_indicator")};

    int m_levelHeight{81};
    int m_clipHeight{11};
    int m_peakHoldSize{4};
    int m_peakFallTime{10};
    bool m_drawGroove{true};

    QUrl m_levelBackgroundSource;
    QUrl m_levelActiveSource;
    QUrl m_clipBackgroundSource;
    QUrl m_clipActiveSource;

    QImage m_levelBackgroundImage;
    QImage m_levelActiveImage;
    QImage m_clipBackgroundImage;
    QImage m_clipActiveImage;
    bool m_imagesDirty{true};

    std::unique_ptr<ControlProxy> m_pLevelControl;
    std::unique_ptr<ControlProxy> m_pPeakControl;

    qreal m_level{0.0};
    qreal m_heldPeak{0.0};
    bool m_clipping{false};
    qint64 m_holdUntilMs{0};
    qint64 m_lastTickMs{0};

    // Scene-graph nodes, owned by the item's root node and only touched from
    // updatePaintNode().
    QSGSimpleTextureNode* m_pClipBackgroundNode{nullptr};
    QSGSimpleTextureNode* m_pClipActiveNode{nullptr};
    QSGSimpleTextureNode* m_pLevelBackgroundNode{nullptr};
    QSGSimpleTextureNode* m_pLevelActiveNode{nullptr};
    QSGSimpleTextureNode* m_pPeakNode{nullptr};
};

} // namespace qml
} // namespace mixxx
