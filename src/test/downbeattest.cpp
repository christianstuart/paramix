#include <gtest/gtest.h>

#include <QString>
#include <QStringList>
#include <limits>
#include <vector>

#include "audio/types.h"
#include "track/beats.h"
#include "track/bpm.h"
#include "waveform/renderers/allshader/waveformrenderbeat.h"

namespace {

using allshader::downbeat::isDownbeat;
using allshader::downbeat::Pattern;
using allshader::downbeat::patternFor;
using mixxx::Beats;
using mixxx::BeatsPointer;
using mixxx::Bpm;
using mixxx::audio::FramePos;

constexpr auto kSampleRate = mixxx::audio::SampleRate(48000);
constexpr auto kBpm = Bpm(120.0);
/// 120 BPM at 48 kHz is exactly 24000 frames per beat, so every expected
/// downbeat frame below is an integer without rounding.
constexpr double kBeatFrames = 24000.0;

QString downbeatSubVersion(int beatsPerBar, int phase) {
    QString subVersion = QStringLiteral("paramix-analysis-engine:1");
    subVersion += QStringLiteral("|db=1|dbbar=%1|dbphase=%2|dbconf=0.9|dbsrc=test")
                          .arg(beatsPerBar)
                          .arg(phase);
    return subVersion;
}

BeatsPointer makeGrid(FramePos firstBeat, const QString& subVersion) {
    return Beats::fromConstTempo(kSampleRate, firstBeat, kBpm, subVersion);
}

Pattern patternOf(const Beats& beats) {
    const mixxx::DownbeatInfo& info = beats.getDownbeatInfo();
    return patternFor(info.valid, info.beatsPerBar, info.phase);
}

/// Index of the first beat at or after `position`, counted from the grid's
/// first beat. Mirrors the derivation in the beat renderer.
int beatIndexAt(const Beats& beats, FramePos position) {
    const auto it = beats.iteratorFrom(position);
    if (it == beats.cend()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(it - beats.cfirstmarker());
}

/// Simulates the renderer: enter the grid at `viewportStart`, then walk to
/// `beatPosition` and return the index the loop would hold there.
int indexFromViewportStart(const Beats& beats, FramePos viewportStart, FramePos beatPosition) {
    auto it = beats.iteratorFrom(viewportStart);
    int index = (it == beats.cend())
            ? 0
            : static_cast<int>(it - beats.cfirstmarker());
    for (;
            it != beats.cend() && *it < beatPosition;
            ++it, ++index) {
    }
    return index;
}

/// Downbeat frame positions in [start, end], using the same iteration and
/// index arithmetic as the renderer.
std::vector<double> downbeatFrames(const Beats& beats,
        FramePos start,
        FramePos end,
        const Pattern& pattern) {
    std::vector<double> frames;
    auto it = beats.iteratorFrom(start);
    int index = (it == beats.cend())
            ? 0
            : static_cast<int>(it - beats.cfirstmarker());
    for (;
            it != beats.cend() && *it <= end;
            ++it, ++index) {
        if (isDownbeat(index, pattern)) {
            frames.push_back(it->value());
        }
    }
    return frames;
}

} // namespace

TEST(DownbeatTest, ConstantGridPhaseSelectsExpectedFrames) {
    constexpr FramePos kFirstBeat(0.0);
    constexpr FramePos kRangeEnd(11.0 * kBeatFrames);

    for (int phase = 0; phase < 4; ++phase) {
        SCOPED_TRACE(phase);
        const BeatsPointer pBeats = makeGrid(kFirstBeat, downbeatSubVersion(4, phase));
        ASSERT_TRUE(pBeats);

        const Pattern pattern = patternOf(*pBeats);
        EXPECT_FALSE(pattern.assumed);
        EXPECT_EQ(4, pattern.beatsPerBar);
        EXPECT_EQ(phase, pattern.phase);

        std::vector<double> expected;
        for (int beat = 0; beat < 12; ++beat) {
            if ((beat - phase) % 4 == 0) {
                expected.push_back(beat * kBeatFrames);
            }
        }
        // Beats 0..11 span frames 0..264000.
        EXPECT_EQ(expected, downbeatFrames(*pBeats, FramePos(0.0), kRangeEnd, pattern));
    }
}

TEST(DownbeatTest, ThreeFourGridUsesItsOwnBarLength) {
    const BeatsPointer pBeats = makeGrid(FramePos(0.0), downbeatSubVersion(3, 1));
    ASSERT_TRUE(pBeats);

    const Pattern pattern = patternOf(*pBeats);
    EXPECT_FALSE(pattern.assumed);
    EXPECT_EQ(3, pattern.beatsPerBar);
    EXPECT_EQ(1, pattern.phase);

    // Downbeats are beats 1, 4, 7 and 10 of a 3/4 grid with phase 1.
    const std::vector<double> expected{
            1 * kBeatFrames,
            4 * kBeatFrames,
            7 * kBeatFrames,
            10 * kBeatFrames};
    EXPECT_EQ(expected,
            downbeatFrames(*pBeats,
                    FramePos(0.0),
                    FramePos(11.0 * kBeatFrames),
                    pattern));
}

TEST(DownbeatTest, GridStartingBeforeFrameZeroKeepsTheGridOrigin) {
    // Beat 0 sits two beats before the track start, so frame 0 is beat 2.
    constexpr double kFirstBeatFrames = -2.0 * kBeatFrames;
    const BeatsPointer pBeats = makeGrid(
            FramePos(kFirstBeatFrames), downbeatSubVersion(4, 0));
    ASSERT_TRUE(pBeats);

    EXPECT_EQ(2, beatIndexAt(*pBeats, FramePos(0.0)));

    // The first drawn downbeat is beat 4 at -48000 + 96000 = 48000 frames.
    // A counter restarted at the first visible beat would mark beat 2
    // instead, which is exactly the bar-line drift this test guards against.
    const std::vector<double> expected{
            4 * kBeatFrames + kFirstBeatFrames,
            8 * kBeatFrames + kFirstBeatFrames};
    EXPECT_EQ(expected,
            downbeatFrames(*pBeats,
                    FramePos(0.0),
                    FramePos(9.0 * kBeatFrames),
                    patternOf(*pBeats)));
}

TEST(DownbeatTest, NegativeBeatIndexUsesTheSameModulo) {
    const Pattern pattern = patternFor(true, 4, 2);
    EXPECT_TRUE(isDownbeat(-2, pattern));
    EXPECT_FALSE(isDownbeat(-1, pattern));
    EXPECT_TRUE(isDownbeat(2, pattern));
    EXPECT_TRUE(isDownbeat(6, pattern));
}

TEST(DownbeatTest, MissingInfoFallsBackToAssumedFourFour) {
    const BeatsPointer pBeats = makeGrid(FramePos(0.0), QString());
    ASSERT_TRUE(pBeats);

    const Pattern pattern = patternOf(*pBeats);
    EXPECT_TRUE(pattern.assumed);
    EXPECT_EQ(4, pattern.beatsPerBar);
    EXPECT_EQ(0, pattern.phase);

    const std::vector<double> expected{
            0 * kBeatFrames,
            4 * kBeatFrames,
            8 * kBeatFrames};
    EXPECT_EQ(expected,
            downbeatFrames(*pBeats,
                    FramePos(0.0),
                    FramePos(11.0 * kBeatFrames),
                    pattern));
}

TEST(DownbeatTest, InconsistentMetadataFallsBackToAssumedFourFour) {
    const QStringList subVersions{
            // Explicitly disabled.
            QStringLiteral("paramix-analysis-engine:1|db=0|dbbar=4|dbphase=1"),
            // Bar length must be positive.
            QStringLiteral("paramix-analysis-engine:1|db=1|dbbar=0|dbphase=0"),
            // Phase must be inside the bar.
            QStringLiteral("paramix-analysis-engine:1|db=1|dbbar=4|dbphase=4"),
            QStringLiteral("paramix-analysis-engine:1|db=1|dbbar=4|dbphase=-1"),
    };
    for (const QString& subVersion : subVersions) {
        SCOPED_TRACE(subVersion.toStdString());
        const BeatsPointer pBeats = makeGrid(FramePos(0.0), subVersion);
        ASSERT_TRUE(pBeats);
        const Pattern pattern = patternOf(*pBeats);
        EXPECT_TRUE(pattern.assumed);
        EXPECT_EQ(4, pattern.beatsPerBar);
        EXPECT_EQ(0, pattern.phase);
    }
}

TEST(DownbeatTest, BeatIndexIsStableAcrossViewportStarts) {
    const BeatsPointer pBeats = makeGrid(FramePos(0.0), downbeatSubVersion(4, 1));
    ASSERT_TRUE(pBeats);

    // A beat mid-track, deliberately not on a bar start (phase 1 puts the
    // downbeats on beats 1, 5, 9, ... so 18 is an ordinary beat).
    constexpr int kBeat = 18;
    const FramePos beatPosition(kBeat * kBeatFrames);

    // Whatever the viewport starts at, walking to the same beat must yield the
    // same grid index — the renderer's downbeat decision depends on it.
    EXPECT_EQ(kBeat, indexFromViewportStart(*pBeats, FramePos(0.0), beatPosition));
    EXPECT_EQ(kBeat,
            indexFromViewportStart(*pBeats,
                    FramePos(9.5 * kBeatFrames),
                    beatPosition));
    EXPECT_EQ(kBeat,
            indexFromViewportStart(*pBeats,
                    beatPosition - 1.0,
                    beatPosition));
    EXPECT_EQ(kBeat, beatIndexAt(*pBeats, beatPosition));

    // ... and that index decides the bar line the same way in every window.
    const Pattern pattern = patternOf(*pBeats);
    EXPECT_FALSE(isDownbeat(kBeat, pattern));
    EXPECT_TRUE(isDownbeat(kBeat + 3, pattern));
}
