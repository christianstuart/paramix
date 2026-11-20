#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>

#include "control/controlobject.h"
#include "engine/enginepregain.h"
#include "preferences/replaygainsettings.h"
#include "test/mixxxtest.h"
#include "util/math.h"

namespace {

ControlObject* replayGainControl(const QString& key) {
    return ControlObject::getControl(
            ConfigKey(QStringLiteral("[ReplayGain]"), key));
}

ControlObject* channelControl(const QString& group, const QString& key) {
    return ControlObject::getControl(ConfigKey(group, key));
}

} // namespace

class EnginePregainTest : public MixxxTest {};

TEST_F(EnginePregainTest, SeedsReplayGainControlsFromStoredSettings) {
    ReplayGainSettings settings(config());
    settings.setInitialReplayGainBoost(-3);
    settings.setInitialDefaultBoost(-9);
    settings.setReplayGainEnabled(true);

    // EnginePregain::process() reads the deck's passthrough control.
    const ControlObject passthrough(
            ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("passthrough")));

    // Constructing the first deck initializes the shared [ReplayGain]
    // controls. This must happen before the Preferences dialog is opened.
    EnginePregain pregain(QStringLiteral("[Channel1]"), config());

    const ControlObject* pBoost =
            replayGainControl(QStringLiteral("ReplayGainBoost"));
    const ControlObject* pDefaultBoost =
            replayGainControl(QStringLiteral("DefaultBoost"));
    const ControlObject* pEnabled =
            replayGainControl(QStringLiteral("ReplayGainEnabled"));
    ASSERT_NE(nullptr, pBoost);
    ASSERT_NE(nullptr, pDefaultBoost);
    ASSERT_NE(nullptr, pEnabled);

    EXPECT_NEAR(db2ratio(-3.0), pBoost->get(), 1e-9);
    EXPECT_NEAR(db2ratio(-9.0), pDefaultBoost->get(), 1e-9);
    EXPECT_NEAR(1.0, pEnabled->get(), 1e-9);

    // A track that has not been analyzed yet has replaygain == 0, so process()
    // has to fall back to the seeded default boost. -9 dB is configured above;
    // -6 dB is the built-in default (ReplayGainSettings uses InitialDefaultBoost
    // when the key is absent).
    pregain.setSpeedAndScratching(1.0, false);
    CSAMPLE samples[64];
    std::fill(std::begin(samples), std::end(samples), 1.0f);
    pregain.process(samples, std::size(samples));

    const ControlObject* pTotalGain =
            channelControl(QStringLiteral("[Channel1]"), QStringLiteral("total_gain"));
    ASSERT_NE(nullptr, pTotalGain);
    EXPECT_NEAR(db2ratio(-9.0), pTotalGain->get(), 1e-6);
}
