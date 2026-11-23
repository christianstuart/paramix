#include "qml/qmlspinnyposition.h"

#include <QQuickWindow>
#include <QSGNode>
#include <algorithm>
#include <cmath>

#include "control/controlproxy.h"
#include "moc_qmlspinnyposition.cpp"
#include "waveform/visualplayposition.h"

namespace mixxx {
namespace qml {

namespace {

constexpr auto kMinimumSyncInterval = std::chrono::microseconds(1000);
constexpr auto kMaximumSyncInterval = std::chrono::microseconds(100000);

} // namespace

QmlSpinnyPosition::QmlSpinnyPosition(QQuickItem* parent)
        : QQuickItem(parent) {
    setFlag(QQuickItem::ItemHasContents, true);

    connect(this,
            &QQuickItem::windowChanged,
            this,
            &QmlSpinnyPosition::slotWindowChanged,
            Qt::DirectConnection);
    connect(this, &QQuickItem::visibleChanged, this, [this]() {
        if (isVisible()) {
            resetFrameTiming();
            refresh();
            update();
        }
    });
    slotWindowChanged(window());
}

QmlSpinnyPosition::~QmlSpinnyPosition() {
    QObject::disconnect(m_frameConnection);
}

void QmlSpinnyPosition::setGroup(const QString& group) {
    if (m_group == group) {
        return;
    }

    m_group = group;
    m_pPlayControl.reset();
    m_pScratchEnableControl.reset();
    if (m_group.isEmpty()) {
        m_visualPlayPosition.reset();
    } else {
        m_visualPlayPosition = VisualPlayPosition::getVisualPlayPosition(m_group);
        m_pPlayControl = std::make_unique<ControlProxy>(
                m_group, QStringLiteral("play"), this);
        m_pScratchEnableControl = std::make_unique<ControlProxy>(
                m_group, QStringLiteral("scratch_position_enable"), this);
        // A state change must produce a frame even while the paused position
        // is not changing.
        m_pPlayControl->connectValueChanged(this, [this](double) {
            update();
        });
        m_pScratchEnableControl->connectValueChanged(this, [this](double) {
            update();
        });
    }
    resetPositions();
    emit groupChanged(m_group);
    refresh();
    update();
}

void QmlSpinnyPosition::slotWindowChanged(QQuickWindow* window) {
    QObject::disconnect(m_frameConnection);
    if (window) {
        m_frameConnection = connect(window,
                &QQuickWindow::afterFrameEnd,
                this,
                &QmlSpinnyPosition::slotFrameSwapped);
        if (isVisible()) {
            update();
        }
    }
    resetFrameTiming();
}

void QmlSpinnyPosition::slotFrameSwapped() {
    if (!isVisible()) {
        return;
    }

    const auto frameInterval = std::chrono::microseconds(
            m_timer.restart().toIntegerMicros());
    if (m_haveFrameInterval && frameInterval >= kMinimumSyncInterval &&
            frameInterval <= kMaximumSyncInterval) {
        m_syncInterval = frameInterval;
    }
    m_haveFrameInterval = true;
    refresh();
    // A paused platter does not move. Refresh the QML angle binding so a seek
    // is picked up when another frame happens, but do not request a new frame
    // ourselves; play/scratch state changes wake the loop.
    const bool transportActive =
            (m_pPlayControl && m_pPlayControl->toBool()) ||
            (m_pScratchEnableControl && m_pScratchEnableControl->toBool());
    if (transportActive) {
        update();
    }
}

std::chrono::microseconds QmlSpinnyPosition::fromTimerToNextSync(
        const PerformanceTimer& timer) {
    if (!m_timer.running()) {
        return kDefaultSyncInterval;
    }
    // A paused platter does not request frames, so the last frame time can be
    // far older than the audio reference. Clamp to the next real vsync instead
    // of returning a negative interval, which would make the play position
    // lookup reject its buffers.
    return std::clamp(m_syncInterval +
                    std::chrono::microseconds(
                            m_timer.difference(timer).toIntegerMicros()),
            std::chrono::microseconds(0),
            m_syncInterval);
}

std::chrono::microseconds QmlSpinnyPosition::getSyncInterval() const {
    return m_syncInterval;
}

void QmlSpinnyPosition::resetFrameTiming() {
    m_timer.restart();
    m_syncInterval = kDefaultSyncInterval;
    m_haveFrameInterval = false;
}

void QmlSpinnyPosition::refresh() {
    updatePositions();
}

void QmlSpinnyPosition::resetPositions() {
    const bool wasValid = m_valid;
    const bool playPositionWasChanged = m_playPosition != 0.0;
    const bool slipPositionWasChanged = m_slipPosition != 0.0;
    m_valid = false;
    m_playPosition = 0.0;
    m_slipPosition = 0.0;
    if (wasValid) {
        emit validChanged(false);
    }
    if (playPositionWasChanged) {
        emit playPositionChanged(0.0);
    }
    if (slipPositionWasChanged) {
        emit slipPositionChanged(0.0);
    }
}

void QmlSpinnyPosition::updatePositions() {
    if (m_visualPlayPosition.isNull()) {
        resetPositions();
        return;
    }

    double playPosition = 0.0;
    double slipPosition = 0.0;
    if (!m_visualPlayPosition->getPlaySlipAtNextVSync(this, &playPosition, &slipPosition)) {
        resetPositions();
        return;
    }

    const bool playPositionWasChanged = m_playPosition != playPosition;
    const bool slipPositionWasChanged = m_slipPosition != slipPosition;
    const bool wasValid = m_valid;
    m_playPosition = playPosition;
    m_slipPosition = slipPosition;
    m_valid = true;
    if (!wasValid) {
        emit validChanged(true);
    }
    if (playPositionWasChanged) {
        emit playPositionChanged(m_playPosition);
    }
    if (slipPositionWasChanged) {
        emit slipPositionChanged(m_slipPosition);
    }
}

QSGNode* QmlSpinnyPosition::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    return oldNode;
}

} // namespace qml
} // namespace mixxx
