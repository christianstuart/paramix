#include <gtest/gtest.h>

#include "waveform/waveform.h"

namespace {

constexpr int kStemCount = mixxx::kMaxSupportedStems;

// Fills a waveform with a deterministic per-stem pattern: every column gets a
// different peak, RMS, band mix and attack value.
void fillWaveform(Waveform* pWaveform) {
    const int dataSize = pWaveform->getDataSize();
    ASSERT_GT(dataSize, 8);
    WaveformData* pData = pWaveform->data();
    for (int i = 0; i < dataSize; ++i) {
        for (int stemIdx = 0; stemIdx < kStemCount; ++stemIdx) {
            pData[i].stems[stemIdx] = static_cast<unsigned char>(
                    (i * 7 + stemIdx * 31) % 256);
        }
    }

    ASSERT_TRUE(pWaveform->hasStemColumns());
    WaveformStemColumn* pColumns = pWaveform->stemColumnDataForWrite();
    ASSERT_NE(pColumns, nullptr);
    const int columnCount = pWaveform->getStemColumnCount(0);
    for (int column = 0; column < columnCount; ++column) {
        for (int stemIdx = 0; stemIdx < kStemCount; ++stemIdx) {
            // The peak is the max of both channels of the stem value that is
            // also stored in the waveform data, exactly like the analyzer.
            pColumns[column].peak[stemIdx] = std::max(pData[column * 2].stems[stemIdx],
                    pData[column * 2 + 1].stems[stemIdx]);
            pColumns[column].rms[stemIdx] = static_cast<unsigned char>(
                    (column * 5 + stemIdx * 11) % 256);
            pColumns[column].bands[stemIdx] = packStemBands(
                    static_cast<unsigned char>(column % 4),
                    static_cast<unsigned char>((column + 1) % 4),
                    static_cast<unsigned char>((column + 2) % 4),
                    static_cast<unsigned char>((column + stemIdx) % 4));
        }
    }
    pWaveform->buildStemColumnLevels();
}

TEST(WaveformStemDetail, RoundTripPreservesPerStemDetail) {
    Waveform waveform(44100, 8192, 441, -1, kStemCount);
    fillWaveform(&waveform);
    ASSERT_TRUE(waveform.hasStemColumns());

    const int columnCount = waveform.getStemColumnCount(0);
    ASSERT_GT(columnCount, 1);
    ASSERT_GT(waveform.getStemColumnLevelCount(), 1);
    ASSERT_GT(waveform.getStemPeakReference(), 0);

    const QByteArray blob = waveform.toByteArray();
    ASSERT_FALSE(blob.isEmpty());

    // Reading a stored summary must restore the per-stem detail, the derived
    // aggregation levels and the cached whole-track peaks.
    Waveform restored(blob);
    ASSERT_EQ(restored.getDataSize(), waveform.getDataSize());
    ASSERT_TRUE(restored.hasStemColumns());
    ASSERT_EQ(restored.getStemColumnCount(0), columnCount);
    ASSERT_EQ(restored.getStemColumnLevelCount(),
            waveform.getStemColumnLevelCount());

    const WaveformStemColumn* pExpected = waveform.getStemColumnData(0);
    const WaveformStemColumn* pActual = restored.getStemColumnData(0);
    for (int column = 0; column < columnCount; ++column) {
        for (int stemIdx = 0; stemIdx < kStemCount; ++stemIdx) {
            EXPECT_EQ(pExpected[column].peak[stemIdx], pActual[column].peak[stemIdx]);
            EXPECT_EQ(pExpected[column].rms[stemIdx], pActual[column].rms[stemIdx]);
            EXPECT_EQ(pExpected[column].bands[stemIdx], pActual[column].bands[stemIdx]);
        }
    }

    for (int stemIdx = 0; stemIdx < kStemCount; ++stemIdx) {
        EXPECT_EQ(restored.getStemPeak(stemIdx), waveform.getStemPeak(stemIdx));
    }
    EXPECT_EQ(restored.getStemPeakReference(), waveform.getStemPeakReference());

    // Aggregation must preserve the loudest peak of the covered columns.
    const WaveformStemColumn* pLevel = restored.getStemColumnData(1);
    ASSERT_NE(pLevel, nullptr);
    EXPECT_EQ(restored.getStemColumnCount(1), (columnCount + 1) / 2);
    for (int stemIdx = 0; stemIdx < kStemCount; ++stemIdx) {
        unsigned char expectedPeak = 0;
        for (int column = 0; column < 2 && column < columnCount; ++column) {
            expectedPeak = std::max(expectedPeak, pActual[column].peak[stemIdx]);
        }
        EXPECT_EQ(expectedPeak, pLevel[0].peak[stemIdx]);
    }
}

TEST(WaveformStemDetail, SummaryWithoutStemsHasNoStemColumns) {
    Waveform waveform(44100, 8192, 441, -1, 0);
    const int dataSize = waveform.getDataSize();
    ASSERT_GT(dataSize, 0);
    for (int i = 0; i < dataSize; ++i) {
        waveform.data()[i].filtered.all = static_cast<unsigned char>(i % 256);
    }
    waveform.buildStemColumnLevels();
    EXPECT_FALSE(waveform.hasStemColumns());

    Waveform restored(waveform.toByteArray());
    EXPECT_FALSE(restored.hasStemColumns());
    EXPECT_EQ(restored.getStemColumnLevelCount(), 0);
    EXPECT_EQ(restored.getStemPeakReference(), 0);
}

TEST(WaveformStemDetail, BandFieldsRoundTripThroughPacking) {
    for (int low = 0; low <= kStemBandMax; ++low) {
        for (int mid = 0; mid <= kStemBandMax; mid += 3) {
            for (int high = 0; high <= kStemBandMax; ++high) {
                for (int attack = 0; attack <= kStemBandMax; ++attack) {
                    const unsigned char packed = packStemBands(
                            static_cast<unsigned char>(low),
                            static_cast<unsigned char>(mid),
                            static_cast<unsigned char>(high),
                            static_cast<unsigned char>(attack));
                    EXPECT_EQ(stemBandField(packed, 0), low);
                    EXPECT_EQ(stemBandField(packed, 1), mid);
                    EXPECT_EQ(stemBandField(packed, 2), high);
                    EXPECT_EQ(stemBandField(packed, 3), attack);
                }
            }
        }
    }
}

} // namespace
