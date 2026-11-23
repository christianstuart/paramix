#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <vector>

#include "util/performancetimer.h"
#include "waveform/isynctimeprovider.h"
#include "waveform/visualplayposition.h"

namespace {

constexpr int kBufferMicros = 21333;
constexpr int kCallbackEntryToDacMicros = 42616;

/// Produces a scripted back-to-back "time to next vsync" for each buffer the
/// lookup inspects. Each entry corresponds to one older buffer, mirroring a
/// display frame whose timestamp is older than the newest audio callback.
class ScriptedVSyncTimeProvider : public VSyncTimeProvider {
  public:
    explicit ScriptedVSyncTimeProvider(std::vector<int> micros)
            : m_micros(std::move(micros)) {
    }

    std::chrono::microseconds fromTimerToNextSync(
            const PerformanceTimer&) override {
        const std::size_t index = std::min(m_index, m_micros.size() - 1);
        ++m_index;
        return std::chrono::microseconds(m_micros[index]);
    }

    std::chrono::microseconds getSyncInterval() const override {
        return std::chrono::microseconds(16667);
    }

  private:
    std::vector<int> m_micros;
    std::size_t m_index{0};
};

void setCallbackEntryToDac(int micros) {
    PerformanceTimer now;
    now.start();
    VisualPlayPosition::setCallbackEntryToDacSecs(
            static_cast<double>(micros) / 1000000.0, now);
}

void pushPosition(VisualPlayPosition* pVisualPlayPosition,
        double playPosition,
        double slipPosition,
        SlipModeState slipModeState = SlipModeState::Disabled) {
    pVisualPlayPosition->set(playPosition,
            0.0,
            0.0,
            slipPosition,
            0.0,
            slipModeState,
            false,
            false,
            false,
            0.0,
            0.0,
            0.0,
            kBufferMicros);
}

TEST(VisualPlayPositionTest, EmptyRingReportsNoPosition) {
    VisualPlayPosition visualPlayPosition;
    ScriptedVSyncTimeProvider syncProvider({0});

    double playPosition = 7.0;
    double slipPosition = 8.0;
    EXPECT_FALSE(visualPlayPosition.getPlaySlipAtNextVSync(
            &syncProvider, &playPosition, &slipPosition));

    // A failed lookup must not touch the caller's values.
    EXPECT_DOUBLE_EQ(7.0, playPosition);
    EXPECT_DOUBLE_EQ(8.0, slipPosition);
}

TEST(VisualPlayPositionTest, LooksPastThreeBuffersWhenLatencyIsHigh) {
    // A stale frame timestamp pushes the newest buffers before the DAC window.
    // With ~2 buffers of callback-to-DAC latency the DAC buffer is the fourth
    // one in the ring, which the old three-buffer lookback never reached. The
    // lookup then reported failure and the waveform rendered at sample 0.
    setCallbackEntryToDac(kCallbackEntryToDacMicros);

    VisualPlayPosition visualPlayPosition;
    pushPosition(&visualPlayPosition, 0.1, 0.1);
    pushPosition(&visualPlayPosition, 0.2, 0.2);
    pushPosition(&visualPlayPosition, 0.3, 0.3);
    pushPosition(&visualPlayPosition, 0.4, 0.4);

    ScriptedVSyncTimeProvider syncProvider(
            {-24500, -3200, 18100, 39400});

    double playPosition = -1.0;
    double slipPosition = -1.0;
    ASSERT_TRUE(visualPlayPosition.getPlaySlipAtNextVSync(
            &syncProvider, &playPosition, &slipPosition));
    // getAt(3) is the oldest pushed value and the first one inside the window.
    EXPECT_DOUBLE_EQ(0.1, playPosition);
    EXPECT_DOUBLE_EQ(0.1, slipPosition);
}

TEST(VisualPlayPositionTest, FallsBackToNewestRawPosition) {
    // Even when the whole ring is outside the DAC window (for example after a
    // long display stall), the callers must receive the latest engine position
    // instead of a failure that would render the track start.
    setCallbackEntryToDac(kCallbackEntryToDacMicros);

    VisualPlayPosition visualPlayPosition;
    pushPosition(&visualPlayPosition, 0.6, 0.9);
    pushPosition(&visualPlayPosition, 0.5, 0.8);
    pushPosition(&visualPlayPosition, 0.4, 0.7, SlipModeState::Running);

    ScriptedVSyncTimeProvider syncProvider({-200000});

    double playPosition = -1.0;
    double slipPosition = -1.0;
    ASSERT_TRUE(visualPlayPosition.getPlaySlipAtNextVSync(
            &syncProvider, &playPosition, &slipPosition));
    EXPECT_DOUBLE_EQ(0.4, playPosition);
    // The newest sample is slipping, so the raw slip position is returned.
    EXPECT_DOUBLE_EQ(0.7, slipPosition);
}

} // namespace
