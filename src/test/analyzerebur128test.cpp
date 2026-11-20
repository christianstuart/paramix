#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "analyzer/analyzerebur128.h"
#include "analyzer/analyzertrack.h"
#include "audio/types.h"
#include "test/mixxxtest.h"
#include "track/replaygain.h"
#include "track/track.h"
#include "util/math.h"
#include "util/sample.h"

namespace {

class AnalyzerEbur128Test : public MixxxTest {};

TEST_F(AnalyzerEbur128Test, StemSourceMeasuresStereoMixdown) {
    constexpr SINT kFrames = 2 * 48000;
    constexpr int kStemChannels = 8;
    const mixxx::audio::SampleRate sampleRate(48000);

    std::vector<CSAMPLE> stemSamples(static_cast<size_t>(kFrames) * kStemChannels);
    const double frequencies[] = {220.0, 440.0, 660.0, 880.0};
    for (int frame = 0; frame < kFrames; ++frame) {
        for (int stem = 0; stem < kStemChannels / 2; ++stem) {
            const double phase = 2.0 * M_PI * frequencies[stem] * frame / 48000.0;
            const auto sample = static_cast<CSAMPLE>(0.1 * std::sin(phase));
            stemSamples[static_cast<size_t>(frame) * kStemChannels + stem * 2] = sample;
            stemSamples[static_cast<size_t>(frame) * kStemChannels + stem * 2 + 1] = sample;
        }
    }
    // The same mixdown the deck outputs from EngineDeck::processStem().
    std::vector<CSAMPLE> mixdown(static_cast<size_t>(kFrames) * 2);
    SampleUtil::mixMultichannelToStereo(mixdown.data(),
            stemSamples.data(),
            kFrames,
            mixxx::audio::ChannelCount::stem());

    const QString trackLocation =
            getTestDir().filePath(QStringLiteral("sine-30.wav"));
    AnalyzerTrack stemTrack(Track::newTemporary(trackLocation));
    AnalyzerTrack mixdownTrack(Track::newTemporary(trackLocation));

    AnalyzerEbur128 stemAnalyzer(config());
    ASSERT_TRUE(stemAnalyzer.initialize(stemTrack,
            sampleRate,
            mixxx::audio::ChannelCount::stem(),
            kFrames));
    ASSERT_TRUE(stemAnalyzer.processSamples(
            stemSamples.data(), static_cast<SINT>(stemSamples.size())));
    stemAnalyzer.storeResults(stemTrack.getTrack());
    stemAnalyzer.cleanup();

    AnalyzerEbur128 mixdownAnalyzer(config());
    ASSERT_TRUE(mixdownAnalyzer.initialize(mixdownTrack,
            sampleRate,
            mixxx::audio::ChannelCount::stereo(),
            kFrames));
    ASSERT_TRUE(mixdownAnalyzer.processSamples(
            mixdown.data(), static_cast<SINT>(mixdown.size())));
    mixdownAnalyzer.storeResults(mixdownTrack.getTrack());
    mixdownAnalyzer.cleanup();

    const double stemRatio = stemTrack.getTrack()->getReplayGain().getRatio();
    const double mixdownRatio = mixdownTrack.getTrack()->getReplayGain().getRatio();
    ASSERT_TRUE(mixxx::ReplayGain::isValidRatio(stemRatio));
    ASSERT_TRUE(mixxx::ReplayGain::isValidRatio(mixdownRatio));

    EXPECT_NEAR(ratio2db(mixdownRatio), ratio2db(stemRatio), 0.5);
}

} // anonymous namespace
