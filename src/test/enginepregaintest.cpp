#include <gtest/gtest.h>

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

} // namespace

class EnginePregainTest : public MixxxTest {};

TEST_F(EnginePregainTest, SeedsReplayGainControlsFromStoredSettings) {
    ReplayGainSettings settings(config());
    settings.setInitialReplayGainBoost(-3);
    settings.setInitialDefaultBoost(-9);
    settings.setReplayGainEnabled(false);

    // Constructing the first deck initializes the shared [ReplayGain]
    // controls. This must happen before the Preferences dialog is opened.
    const EnginePregain pregain(QStringLiteral("[Channel1]"), config());

    const ControlObject* pBoost = replayGainControl(QStringLiteral("ReplayGainBoost"));
    const ControlObject* pDefaultBoost = replayGainControl(QStringLiteral("DefaultBoost"));
    const ControlObject* pEnabled = replayGainControl(QStringLiteral("ReplayGainEnabled"));
    ASSERT_NE(nullptr, pBoost);
    ASSERT_NE(nullptr, pDefaultBoost);
    ASSERT_NE(nullptr, pEnabled);

    EXPECT_DOUBLE_EQ(db2ratio(-3.0), pBoost->get());
    EXPECT_DOUBLE_EQ(db2ratio(-9.0), pDefaultBoost->get());
    EXPECT_DOUBLE_EQ(0.0, pEnabled->get());
}
