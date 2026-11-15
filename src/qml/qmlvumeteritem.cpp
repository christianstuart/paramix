#include "qml/qmlvumeteritem.h"

#include <algorithm>
#include <utility>

#include <QDateTime>
#include <QSet>
#include <QTimer>
#include <QtQuick/QSGNode>
#include <QtQuick/QSGSimpleRectNode>
#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QSGTexture>

#include "control/controlproxy.h"
#include "moc_qmlvumeteritem.cpp"

namespace mixxx {
namespace qml {

namespace {

constexpr int kTickIntervalMs = 33;
constexpr int kPeakHoldMs = 500;

/// One shared 30 Hz driver for every meter in the window. The QML version gave
/// each meter its own 10 ms Timer; a single ticker keeps the wake-up count flat
/// as meters are added.
class VuMeterTicker final : public QObject {
  public:
    static VuMeterTicker* instance() {
        static VuMeterTicker s_instance;
        return &s_instance;
    }

    void add(QmlVuMeterItem* pItem) {
        m_items.insert(pItem);
        if (!m_timer.isActive()) {
            m_timer.start();
        }
    }
    void remove(QmlVuMeterItem* pItem) {
        m_items.remove(pItem);
    }

  private:
    VuMeterTicker() {
        m_timer.setInterval(kTickIntervalMs);
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            for (QmlVuMeterItem* pItem : std::as_const(m_items)) {
                pItem->tick();
            }
        });
    }

    QTimer m_timer;
    QSet<QmlVuMeterItem*> m_items;
};

} // namespace

QmlVuMeterItem::QmlVuMeterItem(QQuickItem* pParent)
        : QQuickItem(pParent) {
    setFlag(QQuickItem::ItemHasContents, true);
    VuMeterTicker::instance()->add(this);
}

QmlVuMeterItem::~QmlVuMeterItem() {
    VuMeterTicker::instance()->remove(this);
}

void QmlVuMeterItem::setGroup(const QString& group) {
    if (m_group == group) {
        return;
    }
    m_group = group;
    reconnectControls();
    emit groupChanged();
    emit levelChanged();
    emit clippingChanged();
}

void QmlVuMeterItem::setLevelKey(const QString& key) {
    if (m_levelKey == key) {
        return;
    }
    m_levelKey = key;
    reconnectControls();
    emit levelKeyChanged();
    emit levelChanged();
}

void QmlVuMeterItem::setPeakKey(const QString& key) {
    if (m_peakKey == key) {
        return;
    }
    m_peakKey = key;
    reconnectControls();
    emit peakKeyChanged();
    emit clippingChanged();
}

void QmlVuMeterItem::setLevelHeight(int height) {
    if (m_levelHeight == height) {
        return;
    }
    m_levelHeight = height;
    update();
    emit levelHeightChanged();
}

void QmlVuMeterItem::setClipHeight(int height) {
    if (m_clipHeight == height) {
        return;
    }
    m_clipHeight = height;
    update();
    emit clipHeightChanged();
}

void QmlVuMeterItem::setPeakHoldSize(int size) {
    if (m_peakHoldSize == size) {
        return;
    }
    m_peakHoldSize = size;
    update();
    emit peakHoldSizeChanged();
}

void QmlVuMeterItem::setPeakFallTime(int ms) {
    if (m_peakFallTime == ms) {
        return;
    }
    m_peakFallTime = ms;
    emit peakFallTimeChanged();
}

void QmlVuMeterItem::setDrawGroove(bool drawGroove) {
    if (m_drawGroove == drawGroove) {
        return;
    }
    m_drawGroove = drawGroove;
    update();
    emit drawGrooveChanged();
}

void QmlVuMeterItem::setLevelBackgroundSource(const QUrl& url) {
    if (m_levelBackgroundSource == url) {
        return;
    }
    m_levelBackgroundSource = url;
    m_levelBackgroundImage = loadImage(url);
    m_imagesDirty = true;
    update();
    emit levelBackgroundSourceChanged();
}

void QmlVuMeterItem::setLevelActiveSource(const QUrl& url) {
    if (m_levelActiveSource == url) {
        return;
    }
    m_levelActiveSource = url;
    m_levelActiveImage = loadImage(url);
    m_imagesDirty = true;
    update();
    emit levelActiveSourceChanged();
}

void QmlVuMeterItem::setClipBackgroundSource(const QUrl& url) {
    if (m_clipBackgroundSource == url) {
        return;
    }
    m_clipBackgroundSource = url;
    m_clipBackgroundImage = loadImage(url);
    m_imagesDirty = true;
    update();
    emit clipBackgroundSourceChanged();
}

void QmlVuMeterItem::setClipActiveSource(const QUrl& url) {
    if (m_clipActiveSource == url) {
        return;
    }
    m_clipActiveSource = url;
    m_clipActiveImage = loadImage(url);
    m_imagesDirty = true;
    update();
    emit clipActiveSourceChanged();
}

void QmlVuMeterItem::reconnectControls() {
    if (m_group.isEmpty()) {
        m_pLevelControl.reset();
        m_pPeakControl.reset();
        return;
    }
    m_pLevelControl = std::make_unique<ControlProxy>(
            m_group, m_levelKey, this, ControlFlag::AllowMissingOrInvalid);
    m_pPeakControl = std::make_unique<ControlProxy>(
            m_group, m_peakKey, this, ControlFlag::AllowMissingOrInvalid);
    updateLevel();
}

// static
QImage QmlVuMeterItem::loadImage(const QUrl& url) {
    if (url.isEmpty()) {
        return {};
    }
    if (url.scheme() == QLatin1String("qrc")) {
        return QImage(QStringLiteral(":") + url.path());
    }
    if (url.isLocalFile()) {
        return QImage(url.toLocalFile());
    }
    return QImage(url.toString());
}

void QmlVuMeterItem::updateLevel() {
    if (!m_pLevelControl || !m_pPeakControl) {
        return;
    }
    const qreal level = std::clamp(static_cast<qreal>(m_pLevelControl->get()), 0.0, 1.0);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;

    if (!qFuzzyCompare(1.0 + level, 1.0 + m_level)) {
        m_level = level;
        changed = true;
        emit levelChanged();
    }

    if (m_level <= 0.0) {
        if (m_heldPeak != 0.0) {
            m_heldPeak = 0.0;
            changed = true;
            emit heldPeakChanged();
        }
    } else if (m_level > m_heldPeak) {
        m_heldPeak = m_level;
        m_holdUntilMs = now + kPeakHoldMs;
        changed = true;
        emit heldPeakChanged();
    } else if (now > m_holdUntilMs && m_heldPeak > 0.0) {
        const qreal dt = m_lastTickMs > 0 ? static_cast<qreal>(now - m_lastTickMs) : 0.0;
        const qreal fallRange = static_cast<qreal>(std::max(1, m_peakFallTime * m_levelHeight));
        m_heldPeak = std::max(0.0, m_heldPeak - 2.0 * dt / fallRange);
        changed = true;
        emit heldPeakChanged();
    }
    m_lastTickMs = now;

    const bool clipping = m_pPeakControl->get() > 0.0;
    if (clipping != m_clipping) {
        m_clipping = clipping;
        changed = true;
        emit clippingChanged();
    }

    if (changed && isVisible()) {
        update();
    }
}

void QmlVuMeterItem::tick() {
    if (!isVisible()) {
        return;
    }
    updateLevel();
}

QSGNode* QmlVuMeterItem::updatePaintNode(QSGNode* pOldNode, UpdatePaintNodeData*) {
    if (!window()) {
        delete pOldNode;
        return nullptr;
    }

    if (pOldNode && !m_imagesDirty) {
        updateNodeRects();
        return pOldNode;
    }

    delete pOldNode;
    pOldNode = nullptr;
    m_imagesDirty = false;

    auto* pRoot = new QSGNode;
    const QRectF bounds = boundingRect();
    const qreal centerX = bounds.center().x();

    // Groove behind everything.
    auto* pGroove = new QSGSimpleRectNode(
            QRectF(centerX - 4.0, 0.0, 8.0, bounds.height()), QColor(0x04, 0x04, 0x04));
    pGroove->setFlag(QSGNode::OwnedByParent);
    pGroove->setRect(m_drawGroove
                    ? QRectF(centerX - 4.0, 0.0, 8.0, bounds.height())
                    : QRectF());
    pRoot->appendChildNode(pGroove);

    const auto makeTextureNode = [this, pRoot](const QImage& image) {
        auto* pNode = new QSGSimpleTextureNode;
        pNode->setOwnsTexture(true);
        if (!image.isNull()) {
            QSGTexture* pTexture = window()->createTextureFromImage(image);
            if (pTexture != nullptr) {
                pTexture->setFiltering(QSGTexture::Nearest);
                pNode->setTexture(pTexture);
            }
        }
        pRoot->appendChildNode(pNode);
        return pNode;
    };

    m_pClipBackgroundNode = makeTextureNode(m_clipBackgroundImage);
    m_pClipActiveNode = makeTextureNode(m_clipActiveImage);
    m_pLevelBackgroundNode = makeTextureNode(m_levelBackgroundImage);
    m_pLevelActiveNode = makeTextureNode(m_levelActiveImage);
    m_pPeakNode = makeTextureNode(m_levelActiveImage);

    updateNodeRects();
    return pRoot;
}

void QmlVuMeterItem::updateNodeRects() {
    const QRectF bounds = boundingRect();
    const qreal centerX = bounds.center().x();
    const qreal left = centerX - 3.0;

    if (m_pLevelBackgroundNode != nullptr) {
        const qreal top = bounds.height() - 1.0 - m_levelHeight;
        m_pLevelBackgroundNode->setRect(QRectF(left, top, 6.0, m_levelHeight));
        const QSize textureSize = m_pLevelBackgroundNode->texture()
                ? m_pLevelBackgroundNode->texture()->textureSize()
                : QSize();
        m_pLevelBackgroundNode->setSourceRect(
                QRectF(0.0, 0.0, textureSize.width(), textureSize.height()));
    }
    if (m_pClipBackgroundNode != nullptr) {
        m_pClipBackgroundNode->setRect(QRectF(left, 1.0, 6.0, m_clipHeight));
    }
    if (m_pClipActiveNode != nullptr) {
        m_pClipActiveNode->setRect(m_clipping ? QRectF(left, 1.0, 6.0, m_clipHeight) : QRectF());
    }
    if (m_pLevelActiveNode != nullptr) {
        const qreal height = m_levelHeight * m_level;
        m_pLevelActiveNode->setRect(height > 0.0
                        ? QRectF(left, bounds.height() - 1.0 - height, 6.0, height)
                        : QRectF());
        const QSize textureSize = m_pLevelActiveNode->texture()
                ? m_pLevelActiveNode->texture()->textureSize()
                : QSize();
        m_pLevelActiveNode->setSourceRect(QRectF(0.0,
                textureSize.height() * (1.0 - m_level),
                textureSize.width(),
                textureSize.height() * m_level));
    }
    if (m_pPeakNode != nullptr) {
        const qreal height = std::min<qreal>(m_peakHoldSize, m_levelHeight * m_heldPeak);
        const bool visible = m_heldPeak > m_level && height > 0.0;
        if (visible) {
            m_pPeakNode->setRect(QRectF(left,
                    bounds.height() - 1.0 - m_levelHeight * m_heldPeak,
                    6.0,
                    height));
            const QSize textureSize = m_pPeakNode->texture()
                    ? m_pPeakNode->texture()->textureSize()
                    : QSize();
            const qreal fraction = height / std::max(1, m_levelHeight);
            m_pPeakNode->setSourceRect(QRectF(0.0,
                    textureSize.height() * (1.0 - m_heldPeak),
                    textureSize.width(),
                    textureSize.height() * fraction));
        } else {
            m_pPeakNode->setRect(QRectF());
        }
    }
}

} // namespace qml
} // namespace mixxx
