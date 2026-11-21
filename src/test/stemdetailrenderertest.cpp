#include <gtest/gtest.h>

#include <QFile>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "sources/audiosource.h"
#include "sources/soundsourceproxy.h"
#include "stems/stemcache.h"
#include "test/mixxxtest.h"
#include "track/track.h"
#include "util/samplebuffer.h"
#include "waveform/renderers/allshader/waveformrendererstem.h"
#include "waveform/stemdetail/stemdetailbuilder.h"
#include "waveform/stemdetail/stemdetailcolors.h"
#include "waveform/stemdetail/stemdetailentry.h"
#include "waveform/stemdetail/stemdetailreducer.h"
#include "waveform/stemdetail/stemdetailtrack.h"

namespace {

struct LiveColorRow {
    StemDetailEntry entry;
    unsigned argb;
};

// Live capture triplets from the shipping build (Frida): the four
// WaveformData::Entry floats fed to updateWaveformCachePicture @ 0x10087b600
// and the exact pixel word setPixelUC @ 0x100885ff0 received for that entry at
// the deck detail control (gain 3.5, base #181818, colour mode 3).
constexpr LiveColorRow kLiveRows[] = {
        {{0.0493026748f, 0.00178950524f, 0.00482170237f, 0.000797470741f}, 0xff419f22u},
        {{0.0383871645f, 0.00101363019f, 0.00267787254f, 0.000964225212f}, 0xff2d662cu},
        {{0.137842238f, 0.00714865513f, 0.0156404655f, 0.000503592368f}, 0xffa0ff0fu},
        {{0.0145007931f, 0.000686336367f, 0.00157042651f, 4.82534087e-05f}, 0xff233e0fu},
        {{0.0121248513f, 0.000226905671f, 0.000573566009f, 0.000442956487f}, 0xff16211du},
        {{0.157163307f, 0.0304966085f, 0.0628240779f, 0.00362398708f}, 0xffdaff1du},
        {{0.00932617672f, 0.00179978122f, 0.00410286011f, 0.000143386613f}, 0xff356512u},
        {{0.0195291732f, 0.00352933165f, 0.00714273844f, 0.000238699824f}, 0xff539a12u},
        {{0.0200214814f, 0.000606812071f, 0.00170097081f, 0.000477609807f}, 0xff21441cu},
        {{0.0823679417f, 0.0116546005f, 0.0192836933f, 0.00186319696f}, 0xffb2ff22u},
        {{0.0231958814f, 0.00443759281f, 0.00888089184f, 0.000484945806f}, 0xff5daf15u},
        {{0.0274205562f, 0.00386262452f, 0.00656798109f, 0.00059852656f}, 0xff609a19u},
        {{1.00118232f, 0.000299003208f, 0.000391651789f, 0.00101379945f}, 0xff171e4du},
        {{0.99999994f, 0.00032075186f, 0.000468652579f, 0.00261729793f}, 0xff151faeu},
        {{0.259193331f, 0.0499142334f, 0.099332802f, 0.00753538124f}, 0xffffff28u},
        {{0.316744596f, 0.0491608977f, 0.105705328f, 0.00659099687f}, 0xffffff22u},
        {{0.313928127f, 0.0487655476f, 0.105217233f, 0.00649364665f}, 0xffffff22u},
};

StemDetailConfig makeConfig(int samplesPerColumn = 316) {
    StemDetailConfig config;
    config.sampleRate = 44100;
    config.height = 21;
    config.samplesPerColumn = samplesPerColumn;
    config.magnification = 2.0f;
    config.verticalInset = 1;
    return config;
}

/// Everything the stem picture geometry depends on for one preprocess pass,
/// excluding the render-graph nodes themselves.
struct StemDetailFrameSnapshot {
    int samplesPerColumn = 1;
    double firstSample = 0.0;
    qint64 firstColumn = 0;
    double textureFirstSample = 0.0;
    int textureColumns = 0;
    double textureOffsetColumns = 0.0;

    bool operator==(const StemDetailFrameSnapshot&) const = default;
};

/// One preprocess pass over a fixed viewport, mirroring the layout part of
/// WaveformRendererStem::preprocessInner(). It uses the same shared helpers, so
/// it pins their behavior rather than re-implementing the arithmetic.
StemDetailFrameSnapshot stemDetailFrameSnapshot(double displayedFirstSample,
        int samplesPerColumn,
        int pixelLength,
        int64_t totalSamples,
        bool paused,
        allshader::stemdetail::TextureRange* pTexture,
        bool* pTextureRebuilt) {
    using namespace allshader::stemdetail;
    const double firstSample = paused
            ? snapToColumn(displayedFirstSample, samplesPerColumn)
            : displayedFirstSample;
    const TextureRange wanted = textureRangeFor(firstSample,
            samplesPerColumn,
            pixelLength,
            totalSamples,
            16384);
    const bool rebuild = pTexture->columns == 0 ||
            pTexture->wholeTrack != wanted.wholeTrack ||
            (paused &&
                    !isColumnAligned(pTexture->firstSample, samplesPerColumn)) ||
            (!pTexture->wholeTrack &&
                    !textureRangeCovers(*pTexture,
                            firstSample,
                            samplesPerColumn,
                            pixelLength));
    if (rebuild) {
        *pTexture = wanted;
    }
    if (pTextureRebuilt) {
        *pTextureRebuilt = rebuild;
    }
    StemDetailFrameSnapshot snapshot;
    snapshot.samplesPerColumn = samplesPerColumn;
    snapshot.firstSample = firstSample;
    snapshot.firstColumn = columnIndex(-firstSample, samplesPerColumn);
    snapshot.textureFirstSample = pTexture->firstSample;
    snapshot.textureColumns = pTexture->columns;
    snapshot.textureOffsetColumns =
            (firstSample - pTexture->firstSample) / samplesPerColumn;
    return snapshot;
}

} // namespace

TEST(StemDetailColor, LiveEntryPixelsAreBitExact) {
    const StemDetailColorParams params;
    for (const LiveColorRow& row : kLiveRows) {
        const QRgb pixel = stemDetailEntryColor(row.entry, params);
        EXPECT_EQ(row.argb, static_cast<unsigned>(pixel))
                << "entry " << row.entry.coverage << "/" << row.entry.low << "/"
                << row.entry.mid << "/" << row.entry.high;
    }
}

TEST(StemDetailColor, EmptyAndCoveredSilence) {
    const StemDetailColorParams params;

    // The empty cell of the picture is the zero entry; coverage defaults to 1,
    // so it must be constructed explicitly.
    const StemDetailEntry empty{0.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_EQ(0xff181818u, static_cast<unsigned>(stemDetailEntryColor(empty, params)));

    // A covered, silent cell saturates the brightness and has no colour, i.e.
    // it is black, not the base colour.
    const StemDetailEntry coveredSilence{1.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_EQ(0xff000000u,
            static_cast<unsigned>(stemDetailEntryColor(coveredSilence, params)));
}

class StemDetailLiveFixtureTest : public MixxxTest {};

TEST_F(StemDetailLiveFixtureTest, AllLiveEntryPixelsAreBitExact) {
    QFile file(getTestDir().filePath(
            QStringLiteral("reference_buffers/stem_detail/live_entry_argb.txt")));
    ASSERT_TRUE(file.open(QFile::ReadOnly | QFile::Text));

    const StemDetailColorParams params;
    int total = 0;
    int exact = 0;
    while (!file.atEnd()) {
        const QByteArray rawLine = file.readLine();
        const QString line = QString::fromLatin1(rawLine).trimmed();
        if (!line.startsWith(QStringLiteral("PX "))) {
            continue;
        }
        const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        ASSERT_EQ(6, fields.size()) << line.toStdString();
        StemDetailEntry entry;
        entry.coverage = fields.at(1).toFloat();
        entry.low = fields.at(2).toFloat();
        entry.mid = fields.at(3).toFloat();
        entry.high = fields.at(4).toFloat();
        bool ok = false;
        const unsigned expected = fields.at(5).toUInt(&ok, 16);
        ASSERT_TRUE(ok) << line.toStdString();

        const unsigned actual = static_cast<unsigned>(stemDetailEntryColor(entry, params));
        if (actual == expected) {
            ++exact;
        } else {
            ADD_FAILURE() << line.toStdString() << " -> " << std::hex << actual;
        }
        ++total;
    }

    EXPECT_EQ(30000, total);
    EXPECT_EQ(total, exact);
}

TEST(StemDetailReducer, SilentColumnsFillTheCentreRowOnly) {
    StemDetailReducer reducer(makeConfig());
    const int samplesPerColumn = reducer.config().samplesPerColumn;
    const std::vector<float> silence(static_cast<std::size_t>(samplesPerColumn), 0.0f);

    reducer.processColumn(silence.data(), silence.data());
    const auto& entries = reducer.processColumn(silence.data(), silence.data());

    // The vertical inset shifts the silent centre line one row above the
    // geometric centre of the image (v = (height - 2) * 0.5 at x = 0).
    const int centreRow = (reducer.config().height - 2) / 2;
    EXPECT_NEAR(1.0f, entries[static_cast<std::size_t>(centreRow)].coverage, 1e-6f);
    EXPECT_EQ(0.0f, entries[static_cast<std::size_t>(centreRow)].low);
    EXPECT_EQ(0.0f, entries[static_cast<std::size_t>(centreRow)].mid);
    EXPECT_EQ(0.0f, entries[static_cast<std::size_t>(centreRow)].high);

    // Silence must not produce a covered cell away from the centre line.
    EXPECT_EQ(0.0f, entries.front().coverage);
    EXPECT_EQ(0.0f, entries.back().coverage);
}

class StemDetailBuilderTest : public MixxxTest {};

TEST(StemDetailBuilderCancelTest, CancelBeforeQueuedBuildStartsSkipsWork) {
    StemDetailKey key;
    key.trackId = TrackId(QVariant(7));
    key.sampleRate = 44100;
    key.samplesPerColumn = 64;
    key.height = 21;

    StemDetailBuilder builder;
    QSignalSpy spy(&builder, &StemDetailBuilder::built);

    // A cancel issued before the queued build slot runs must not be cleared at
    // build entry.
    const quint64 staleGeneration = builder.generation();
    builder.cancel();
    builder.build(key, TrackPointer(), staleGeneration);
    EXPECT_EQ(0, spy.count());

    // The current generation still completes (and reports the missing source
    // as a null result).
    builder.build(key, TrackPointer(), builder.generation());
    ASSERT_EQ(1, spy.count());
    EXPECT_EQ(nullptr,
            spy.at(0).at(1).value<std::shared_ptr<const StemDetailTrack>>());
}

namespace {
constexpr uint64_t kGoldenFingerprint = 17855010542173845310ull;
} // namespace

TEST_F(StemDetailBuilderTest, DecodesParamixEightChannelCacheInStemOrder) {
    constexpr int kFrames = 8192;
    constexpr int kSampleRate = 44100;
    constexpr int kSpc = 64;
    constexpr int kHeight = 21;
    constexpr int kStemCount = mixxx::kMaxSupportedStems;

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    config()->setValue(ConfigKey("[Stems]", "CacheDirectory"), tempDir.path());
    mixxx::stems::StemCache::initialize(config());

    // Four deterministic stereo stems. Stem 3 is silent so the silent-lane
    // behaviour is covered as well.
    std::array<std::vector<float>, kStemCount> left;
    std::array<std::vector<float>, kStemCount> right;
    for (int stem = 0; stem < kStemCount; ++stem) {
        left[static_cast<std::size_t>(stem)].resize(kFrames);
        right[static_cast<std::size_t>(stem)].resize(kFrames);
    }
    const std::array<double, kStemCount> frequencies = {200.0, 80.0, 1000.0, 0.0};
    constexpr double kPi = 3.14159265358979323846;
    std::vector<CSAMPLE> interleaved(static_cast<std::size_t>(kFrames) * kStemCount * 2);
    for (int frame = 0; frame < kFrames; ++frame) {
        const double t = static_cast<double>(frame) / kSampleRate;
        for (int stem = 0; stem < kStemCount; ++stem) {
            const float sample = stem == 3
                    ? 0.0f
                    : static_cast<float>(0.3 * std::sin(2.0 * kPi * frequencies[static_cast<std::size_t>(stem)] * t));
            left[static_cast<std::size_t>(stem)][static_cast<std::size_t>(frame)] = sample;
            right[static_cast<std::size_t>(stem)][static_cast<std::size_t>(frame)] =
                    sample * 0.5f;
            interleaved[static_cast<std::size_t>(frame) * kStemCount * 2 + stem * 2] =
                    sample;
            interleaved[static_cast<std::size_t>(frame) * kStemCount * 2 + stem * 2 + 1] =
                    sample * 0.5f;
        }
    }

    const TrackId trackId(QVariant(4242));
    QString error;
    ASSERT_TRUE(mixxx::stems::StemCache::writeCache(trackId,
            interleaved.data(),
            kFrames,
            kSampleRate,
            &error))
            << error.toStdString();

    const TrackPointer pTrack =
            Track::newDummy(QStringLiteral("stem-detail-test.flac"), trackId);

    // Read the cache back exactly as the builder does, so the reference path
    // below sees the same 16-bit-quantised samples.
    mixxx::AudioSource::OpenParams openParams;
    openParams.setChannelCount(mixxx::kAnalysisMaxChannels);
    const mixxx::AudioSourcePointer pSource =
            SoundSourceProxy(pTrack).openAudioSource(openParams);
    ASSERT_NE(nullptr, pSource);
    const int channelCount = pSource->getSignalInfo().getChannelCount();
    ASSERT_EQ(kStemCount * 2, channelCount);
    ASSERT_EQ(kFrames, pSource->frameLength());
    mixxx::SampleBuffer sampleBuffer(
            static_cast<SINT>(static_cast<std::int64_t>(kFrames) * channelCount));
    const auto readable = pSource->readSampleFrames(
            mixxx::WritableSampleFrames(mixxx::IndexRange::forward(0, kFrames),
                    mixxx::SampleBuffer::WritableSlice(sampleBuffer)));
    ASSERT_EQ(kFrames, readable.frameLength());
    for (int frame = 0; frame < kFrames; ++frame) {
        for (int stem = 0; stem < kStemCount; ++stem) {
            const CSAMPLE* pFrame =
                    readable.readableData() +
                    static_cast<std::size_t>(frame) * channelCount;
            left[static_cast<std::size_t>(stem)][static_cast<std::size_t>(frame)] =
                    pFrame[stem * 2];
            right[static_cast<std::size_t>(stem)][static_cast<std::size_t>(frame)] =
                    pFrame[stem * 2 + 1];
        }
    }
    pSource->close();

    StemDetailKey key;
    key.trackId = trackId;
    key.sampleRate = kSampleRate;
    key.samplesPerColumn = kSpc;
    key.height = kHeight;
    key.magnification = 2.0f;

    StemDetailBuilder builder;
    QSignalSpy spy(&builder, &StemDetailBuilder::built);
    builder.build(key, pTrack, builder.generation());
    ASSERT_EQ(1, spy.count());
    const auto pDetail = spy.at(0)
                                 .at(1)
                                 .value<std::shared_ptr<const StemDetailTrack>>();
    ASSERT_NE(nullptr, pDetail);

    EXPECT_EQ(kStemCount, pDetail->laneCount);
    EXPECT_EQ(kFrames, pDetail->totalSamples);
    EXPECT_EQ(kSampleRate, static_cast<int>(pDetail->sampleRate));
    const int expectedColumns = (kFrames + kSpc - 1) / kSpc;

    const StemDetailColorParams params;
    StemDetailConfig config;
    config.sampleRate = kSampleRate;
    config.height = kHeight;
    config.samplesPerColumn = kSpc;
    config.magnification = 2.0f;
    config.verticalInset = 1;

    for (int stem = 0; stem < kStemCount; ++stem) {
        const StemDetailLane& lane = pDetail->lanes[static_cast<std::size_t>(stem)];
        ASSERT_EQ(kSpc, lane.spc);
        ASSERT_EQ(kHeight, lane.height);
        ASSERT_EQ(0, lane.firstSample);
        ASSERT_EQ(expectedColumns, lane.columns.size());

        // Reference path: feed the original stereo stems through the reducer
        // and the colour consumer, then compare every row of every column.
        StemDetailReducer reducer(config);
        std::vector<float> columnLeft(kSpc);
        std::vector<float> columnRight(kSpc);
        for (int column = 0; column < expectedColumns; ++column) {
            for (int i = 0; i < kSpc; ++i) {
                const int frame = column * kSpc + i;
                const bool inRange = frame < kFrames;
                columnLeft[static_cast<std::size_t>(i)] = inRange
                        ? left[static_cast<std::size_t>(stem)]
                              [static_cast<std::size_t>(frame)]
                        : 0.0f;
                columnRight[static_cast<std::size_t>(i)] = inRange
                        ? right[static_cast<std::size_t>(stem)]
                               [static_cast<std::size_t>(frame)]
                        : 0.0f;
            }
            const auto& entries = reducer.processColumn(columnLeft.data(),
                    columnRight.data());
            const StemDetailColumn& compact =
                    lane.columns.at(column);
            for (int row = 0; row < kHeight; ++row) {
                const StemDetailEntry& entry = entries[static_cast<std::size_t>(row)];
                const bool zero = entry.coverage == 0.0f && entry.low == 0.0f &&
                        entry.mid == 0.0f && entry.high == 0.0f;
                const uint32_t expected = zero
                        ? 0u
                        : static_cast<uint32_t>(
                                  stemDetailEntryColor(entry, params) & 0x00ffffffu);
                uint32_t actual = 0;
                const std::size_t rowIndex =
                        static_cast<std::size_t>(column) * kHeight + row;
                if (rowIndex < static_cast<std::size_t>(lane.rowColors.size()) &&
                        lane.rowColors[static_cast<int>(rowIndex)] != 0) {
                    actual = static_cast<uint32_t>(
                            lane.rowColors[static_cast<int>(rowIndex)] & 0x00ffffffu);
                } else if ((compact.mask >> row) & 1u) {
                    actual = compact.rgba;
                }
                ASSERT_EQ(expected, actual)
                        << "stem " << stem << " column " << column << " row " << row;
                EXPECT_EQ(!zero, (compact.mask >> row) & 1u);
            }
        }
    }

    // The silent stem keeps the lane base colour outside its thin centre band
    // and must never produce a full-height black block.
    const StemDetailLane& silentLane = pDetail->lanes[3];
    for (const StemDetailColumn& column : silentLane.columns) {
        EXPECT_LE(std::popcount(column.mask), 2);
    }

    // Golden fingerprint of the whole picture. The input and the 16-bit cache
    // are deterministic; this catches any drift in channel order, mask packing
    // or the compact/row-colour split.
    uint64_t fingerprint = 1469598103934665603ull;
    const auto mix = [&fingerprint](uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            fingerprint ^= (value >> (byte * 8)) & 0xffu;
            fingerprint *= 1099511628211ull;
        }
    };
    for (int stem = 0; stem < pDetail->laneCount; ++stem) {
        const StemDetailLane& lane = pDetail->lanes[static_cast<std::size_t>(stem)];
        for (int column = 0; column < lane.columns.size(); ++column) {
            const StemDetailColumn& compact = lane.columns.at(column);
            mix(compact.mask);
            for (int row = 0; row < kHeight; ++row) {
                uint32_t color = 0;
                const std::size_t rowIndex =
                        static_cast<std::size_t>(column) * kHeight + row;
                if (rowIndex < static_cast<std::size_t>(lane.rowColors.size())) {
                    color = static_cast<uint32_t>(
                            lane.rowColors[static_cast<int>(rowIndex)] & 0x00ffffffu);
                }
                if (color == 0 && ((compact.mask >> row) & 1u)) {
                    color = compact.rgba;
                }
                mix(color);
            }
        }
    }
    EXPECT_EQ(kGoldenFingerprint, fingerprint);
}

// F01: a paused stem deck must render a stable picture. The engine keeps
// republishing the same visual position and the display keeps repainting, so
// every preprocess pass must produce the same column start, texture range, UV
// offset and cache key, and must not rebuild the texture.

TEST(StemDetailColumns, FollowsZoomAndNotTheRoundedViewport) {
    // One detail column is one device pixel. The renderer used to derive it
    // from the rounded viewport width, so a resize or a fractional device
    // pixel ratio could change the cache key (and rebuild the picture) even
    // though the zoom had not changed.
    constexpr double kAudioSamplesPerPixel = 96.0;
    constexpr float kDevicePixelRatio = 1.25f;
    const int expected =
            allshader::stemdetail::samplesPerColumn(kAudioSamplesPerPixel,
                    kDevicePixelRatio);
    ASSERT_EQ(77, expected); // lround(96 / 1.25)

    const int lengths[] = {320, 640, 1000, 1440, 1920, 2560};
    for (int length : lengths) {
        const int pixelLength =
                static_cast<int>(std::lround(length * kDevicePixelRatio));
        // The old, viewport-rounded derivation: the viewport spans
        // length * audioSamplesPerPixel frames, the same span the beat grid
        // and the other waveform renderers draw.
        const int viewportSamplesPerColumn = std::max(1,
                static_cast<int>(std::lround(length * kAudioSamplesPerPixel /
                        pixelLength)));
        // The zoom-derived value has no viewport input at all, so a resize
        // cannot change the cache key.
        EXPECT_EQ(expected,
                allshader::stemdetail::samplesPerColumn(
                        kAudioSamplesPerPixel, kDevicePixelRatio))
                << "length " << length;
        // And it stays within the pixel rounding of the viewport derivation.
        EXPECT_LE(std::abs(viewportSamplesPerColumn - expected), 1)
                << "length " << length;
    }
}

TEST(StemDetailPausedFrame, ConsecutivePreprocessPassesAreIdentical) {
    constexpr int kPixelLength = 640;
    constexpr int64_t kTotalSamples = 44100LL * 240;
    constexpr double kAudioSamplesPerPixel = 96.0;
    constexpr float kDevicePixelRatio = 2.0f;
    const int samplesPerColumn = allshader::stemdetail::samplesPerColumn(
            kAudioSamplesPerPixel, kDevicePixelRatio);
    ASSERT_EQ(48, samplesPerColumn);

    // A visual position on a column boundary, plus the sub-column wobble the
    // raw play position has between frames. Without snapping the wobble moves
    // the first detail column (and every texel offset) from frame to frame.
    const double baseSample = static_cast<double>(kTotalSamples) / 2.0;
    const double wobble = 0.4 * samplesPerColumn;
    EXPECT_EQ(baseSample,
            allshader::stemdetail::snapToColumn(
                    baseSample, samplesPerColumn)); // the base is on the grid
    EXPECT_NE(allshader::stemdetail::columnIndex(
                      -(baseSample - wobble), samplesPerColumn),
            allshader::stemdetail::columnIndex(
                    -(baseSample + wobble), samplesPerColumn))
            << "the raw wobble must cross a column boundary for this test to "
               "mean anything";
    EXPECT_EQ(allshader::stemdetail::snapToColumn(
                      baseSample - wobble, samplesPerColumn),
            allshader::stemdetail::snapToColumn(
                    baseSample + wobble, samplesPerColumn));

    allshader::stemdetail::TextureRange texture;
    StemDetailFrameSnapshot first;
    bool firstRebuilt = false;
    for (int pass = 0; pass < 8; ++pass) {
        const double displayedFirstSample =
                baseSample + ((pass % 2) ? wobble : -wobble);
        bool rebuilt = false;
        const StemDetailFrameSnapshot snapshot = stemDetailFrameSnapshot(
                displayedFirstSample,
                samplesPerColumn,
                kPixelLength,
                kTotalSamples,
                true,
                &texture,
                &rebuilt);
        if (pass == 0) {
            first = snapshot;
            firstRebuilt = rebuilt;
            EXPECT_TRUE(rebuilt) << "the first pass has no texture yet";
            EXPECT_TRUE(allshader::stemdetail::isColumnAligned(
                    snapshot.firstSample, samplesPerColumn));
            EXPECT_TRUE(allshader::stemdetail::isColumnAligned(
                    snapshot.textureFirstSample, samplesPerColumn));
            continue;
        }
        EXPECT_FALSE(rebuilt) << "pass " << pass << " rebuilt the texture";
        EXPECT_EQ(first, snapshot) << "pass " << pass;
        // The snapped geometry is a whole number of texels from the texture
        // origin, so nearest filtering samples exactly one texel per pixel.
        EXPECT_EQ(std::floor(snapshot.textureOffsetColumns),
                snapshot.textureOffsetColumns)
                << "pass " << pass;
    }
    EXPECT_TRUE(firstRebuilt);
    EXPECT_GT(first.textureColumns, kPixelLength);
}

TEST(StemDetailPausedFrame, WholeTrackTextureIsStableWhileScrolling) {
    // A track whose whole picture fits in one texture uses a fixed range, so a
    // paused deck near either end of the track keeps the same texture.
    constexpr int kPixelLength = 512;
    constexpr int64_t kTotalSamples = 44100LL * 25;
    constexpr int kSamplesPerColumn = 96;

    allshader::stemdetail::TextureRange texture;
    const int64_t totalColumns = (kTotalSamples + kSamplesPerColumn - 1) /
            kSamplesPerColumn;
    ASSERT_LT(totalColumns + 2 * kPixelLength, 16384);

    StemDetailFrameSnapshot first;
    for (int pass = 0; pass < 4; ++pass) {
        // Positions inside the track, deliberately not column aligned.
        const double position =
                static_cast<double>(kTotalSamples) * (0.05 + 0.3 * pass) +
                0.3 * kSamplesPerColumn;
        bool rebuilt = false;
        const StemDetailFrameSnapshot snapshot = stemDetailFrameSnapshot(
                position,
                kSamplesPerColumn,
                kPixelLength,
                kTotalSamples,
                true,
                &texture,
                &rebuilt);
        EXPECT_TRUE(texture.wholeTrack);
        if (pass == 0) {
            first = snapshot;
            EXPECT_TRUE(rebuilt);
        } else {
            EXPECT_FALSE(rebuilt) << "pass " << pass;
            EXPECT_EQ(first.textureFirstSample, snapshot.textureFirstSample);
            EXPECT_EQ(first.textureColumns, snapshot.textureColumns);
        }
        EXPECT_EQ(std::floor(snapshot.textureOffsetColumns),
                snapshot.textureOffsetColumns)
                << "pass " << pass;
    }
}

TEST(StemDetailRequestMemo, UnchangedKeyDoesNotAskTheCacheAgain) {
    StemDetailKey key;
    key.trackId = TrackId(QVariant(59));
    key.sampleRate = 44100;
    key.samplesPerColumn = 96;
    key.height = 21;
    key.magnification = 2.0f;

    allshader::stemdetail::DetailRequestMemo memo;
    EXPECT_TRUE(memo.needsRequest(key, false));
    memo.remember(key);
    for (int pass = 0; pass < 8; ++pass) {
        EXPECT_FALSE(memo.needsRequest(key, false)) << "pass " << pass;
    }

    // A completed rebuild invalidates the memo once.
    EXPECT_TRUE(memo.needsRequest(key, true));
    memo.remember(key);
    EXPECT_FALSE(memo.needsRequest(key, false));

    // A zoom or lane-height change asks again.
    StemDetailKey zoomed = key;
    zoomed.samplesPerColumn *= 2;
    EXPECT_TRUE(memo.needsRequest(zoomed, false));
    StemDetailKey taller = key;
    taller.height += 1;
    EXPECT_TRUE(memo.needsRequest(taller, false));

    // A first miss is retried until it is answered.
    memo.forget();
    EXPECT_TRUE(memo.needsRequest(key, false));
}
