#include <gtest/gtest.h>

#include <QImage>
#include <QTest>
#include <memory>

#include "test/mixxxtest.h"
#include "waveform/renderers/allshader/waveformrendererstem.h"
#include "waveform/stemdetail/stemdetailtrack.h"
#include "waveform/stemdetail/stempagebuilder.h"
#include "waveform/stemdetail/stempagecache.h"
#include "waveform/stemdetail/stempagekey.h"

namespace {

StemPageKey makeKey(int pageIndex = 0) {
    StemPageKey key;
    key.trackId = TrackId(QVariant(77));
    key.sampleRate = 44100;
    key.detailSamplesPerColumn = 4;
    key.detailLaneHeight = 3;
    key.samplesPerColumn = 4;
    key.laneDeviceHeight = 6;
    key.laneCount = 2;
    key.gapDevicePx = 1;
    key.pageWidth = 4;
    key.pageIndex = pageIndex;
    key.magnification = 1.0f;
    return key;
}

StemDetailTrack makeDetail() {
    StemDetailTrack detail;
    detail.trackId = TrackId(QVariant(77));
    detail.sampleRate = 44100;
    detail.totalSamples = 32;
    detail.laneCount = 2;
    detail.magnification = 1.0f;

    for (int lane = 0; lane < 2; ++lane) {
        detail.lanes[static_cast<std::size_t>(lane)].spc = 4;
        detail.lanes[static_cast<std::size_t>(lane)].height = 3;
        detail.lanes[static_cast<std::size_t>(lane)].columns.resize(8);
    }

    StemDetailColumn& red = detail.lanes[0].columns[0];
    red.wave = true;
    red.mask = 0b111;
    red.rgba = 0x00ff0000; // red

    StemDetailColumn& blue = detail.lanes[1].columns[1];
    blue.wave = true;
    blue.mask = 0b010;      // middle row
    blue.rgba = 0x000000ff; // blue
    return detail;
}

} // namespace

TEST(StemPageIndex, PageCoversWholeColumns) {
    using namespace allshader::stemdetail;
    EXPECT_EQ(0, pageIndexForColumn(0.0, 4096));
    EXPECT_EQ(0, pageIndexForColumn(4095.9, 4096));
    EXPECT_EQ(1, pageIndexForColumn(4096.0, 4096));
    EXPECT_EQ(-1, pageIndexForColumn(-0.1, 4096));
    EXPECT_EQ(8192, pageFirstColumn(2, 4096));
}

TEST(StemPageBuilder, MapsColumnsLanesAndRows) {
    const QImage image = buildStemPageImage(makeDetail(), makeKey());
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(QImage::Format_ARGB32_Premultiplied, image.format());
    EXPECT_EQ(4, image.width());
    EXPECT_EQ(13, image.height()); // 2 lanes * 6 px + 1 px gap

    // Column 0 has a full-height red lane 0 and nothing in lane 1.
    for (int y = 0; y < 6; ++y) {
        EXPECT_EQ(0xffff0000u, image.pixel(0, y)) << "lane 0 row " << y;
    }
    EXPECT_EQ(0u, image.pixel(0, 7));
    EXPECT_EQ(0u, image.pixel(0, 12));

    // Column 1 only has lane 1's middle detail row, expanded to device rows
    // 9 and 10.
    EXPECT_EQ(0u, image.pixel(1, 7));
    EXPECT_EQ(0xff0000ffu, image.pixel(1, 9));
    EXPECT_EQ(0xff0000ffu, image.pixel(1, 10));
    EXPECT_EQ(0u, image.pixel(1, 11));
}

TEST(StemPageBuilder, PageIndexSelectsDetailColumnRange) {
    // Page 1 starts at output column 4, which is detail column 4 at spc=4.
    StemDetailTrack detail = makeDetail();
    for (int lane = 0; lane < 2; ++lane) {
        for (auto& column : detail.lanes[static_cast<std::size_t>(lane)].columns) {
            column = StemDetailColumn();
        }
    }
    detail.lanes[0].columns[4] = {0x0000ff00, 0b001, true}; // green, top row

    const QImage image = buildStemPageImage(detail, makeKey(1));
    ASSERT_FALSE(image.isNull());
    // x=0 of page 1 maps to detail column 4.
    EXPECT_EQ(0xff00ff00u, image.pixel(0, 0));
    EXPECT_EQ(0u, image.pixel(0, 5));
}

class StemPageCacheTest : public MixxxTest {};

TEST_F(StemPageCacheTest, BuildsPageOnWorkerThreadAndCachesIt) {
    StemPageCache cache;
    const StemPageKey key = makeKey();
    auto pDetail = std::make_shared<const StemDetailTrack>(makeDetail());

    EXPECT_FALSE(cache.lookup(key));
    cache.request(key, pDetail);

    ASSERT_TRUE(QTest::qWaitFor(
            [&cache, &key]() { return cache.lookup(key) != nullptr; }, 5000));
    const auto pImage = cache.lookup(key);
    ASSERT_TRUE(pImage);
    EXPECT_EQ(4, pImage->width());
    EXPECT_EQ(0xffff0000u, pImage->pixel(0, 0));

    const StemPageCache::Stats stats = cache.stats();
    EXPECT_GE(stats.hits, 2u);
    EXPECT_GE(stats.builds, 1u);
}
