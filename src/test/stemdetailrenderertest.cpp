#include <gtest/gtest.h>

#include <QFile>
#include <QStringList>
#include <array>
#include <cmath>
#include <vector>

#include "test/mixxxtest.h"
#include "waveform/stemdetail/stemdetailcolors.h"
#include "waveform/stemdetail/stemdetailentry.h"
#include "waveform/stemdetail/stemdetailreducer.h"

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
