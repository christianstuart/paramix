#include <gtest/gtest.h>

#include <QFile>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

#include "sources/audiosource.h"
#include "sources/soundsourceproxy.h"
#include "stems/stemcache.h"
#include "test/mixxxtest.h"
#include "track/track.h"
#include "util/samplebuffer.h"
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
    builder.build(key, pTrack);
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
