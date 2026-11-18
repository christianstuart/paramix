#include <gtest/gtest.h>

#ifdef PARAMIX_ANALYSIS_WITH_MIXXX

#include <cmath>
#include <vector>

#include "analyzer/plugins/analyzerparamixbeats.h"
#include "audio/frame.h"
#include "audio/types.h"
#include "track/beats.h"

namespace {

std::vector<float> makeClickTrack(int rate, double seconds, double bpm) {
    constexpr double kPi = 3.14159265358979323846;
    const int frames = static_cast<int>(std::floor(seconds * rate));
    std::vector<float> samples(static_cast<std::size_t>(frames), 0.0f);
    const double period = 60.0 / bpm;
    int beat = 0;
    for (double time = 0.25; time < seconds - 0.1; time += period, ++beat) {
        const int start = static_cast<int>(std::floor(time * rate));
        const int clickLength = static_cast<int>(std::floor(rate * 0.006));
        for (int i = 0; i < clickLength && start + i < frames; ++i) {
            const double envelope = std::exp(-i / (rate * 0.0015));
            samples[static_cast<std::size_t>(start + i)] +=
                    static_cast<float>(envelope *
                            std::sin(2.0 * kPi * 2400.0 * i / rate));
        }
        if (beat % 4 == 0) {
            const int lowLength = static_cast<int>(std::floor(rate * 0.03));
            for (int i = 0; i < lowLength && start + i < frames; ++i) {
                const double envelope = std::exp(-i / (rate * 0.008));
                samples[static_cast<std::size_t>(start + i)] +=
                        static_cast<float>(0.9 * envelope *
                                std::sin(2.0 * kPi * 55.0 * i / rate));
            }
        }
    }
    return samples;
}

} // namespace

TEST(MixxxIntegration, PluginProducesGridAndDownbeatInfo) {
    constexpr int kRate = 22050;
    const std::vector<float> samples = makeClickTrack(kRate, 12.0, 140.0);

    mixxx::AnalyzerParamixBeats plugin;
    ASSERT_TRUE(plugin.initialize(mixxx::audio::SampleRate(kRate),
            mixxx::audio::ChannelCount::mono(),
            static_cast<SINT>(samples.size())));
    ASSERT_TRUE(plugin.processSamples(samples.data(),
            static_cast<SINT>(samples.size())));
    ASSERT_TRUE(plugin.finalize());

    const mixxx::Bpm bpm = plugin.getBpm();
    ASSERT_TRUE(bpm.isValid());
    EXPECT_NEAR(bpm.value(), 140.0, 0.01);

    const mixxx::audio::FramePos firstBeat = plugin.getFirstBeat();
    EXPECT_GE(firstBeat.value(), 0.0);
    EXPECT_NEAR(firstBeat.value() / kRate, 0.25, 0.01);

    const mixxx::DownbeatInfo info = plugin.getDownbeatInfo();
    EXPECT_TRUE(info.valid);
    EXPECT_EQ(info.beatsPerBar, 4);
    EXPECT_EQ(info.phase, 0);
    EXPECT_GT(info.confidence, 0.0f);
    EXPECT_LE(info.confidence, 1.0f);
    EXPECT_EQ(info.source.toStdString(), plugin.pluginInfo().id().toStdString());

    // The same fragments that AnalyzerBeats stores must survive construction
    // and be readable through the additive Beat accessor.
    const QString subVersion = QStringLiteral(
            "rounding=V4|vamp_plugin_id=%1|db=1|dbbar=%2|dbphase=%3|dbconf=%4|dbsrc=%5")
                                       .arg(plugin.pluginInfo().id())
                                       .arg(info.beatsPerBar)
                                       .arg(info.phase)
                                       .arg(QString::number(
                                               static_cast<double>(info.confidence),
                                               'f',
                                               3))
                                       .arg(info.source);
    const mixxx::BeatsPointer grid = mixxx::Beats::fromConstTempo(
            mixxx::audio::SampleRate(kRate), firstBeat, bpm, subVersion);
    ASSERT_TRUE(grid);
    const mixxx::DownbeatInfo stored = grid->getDownbeatInfo();
    EXPECT_TRUE(stored.valid);
    EXPECT_EQ(stored.beatsPerBar, 4);
    EXPECT_EQ(stored.phase, 0);
    EXPECT_EQ(stored.source.toStdString(), plugin.pluginInfo().id().toStdString());
    EXPECT_NEAR(stored.confidence, info.confidence, 0.001f);

    // The grid iterates monotonically from the anchor.
    double previous = -1.0;
    for (int n = 1; n <= 8; ++n) {
        const mixxx::audio::FramePos beat =
                grid->findNthBeat(mixxx::audio::kStartFramePos, n);
        ASSERT_TRUE(beat.isValid()) << "beat " << n;
        EXPECT_GT(beat.value(), previous) << "beat " << n;
        previous = beat.value();
    }
}

#endif // PARAMIX_ANALYSIS_WITH_MIXXX
