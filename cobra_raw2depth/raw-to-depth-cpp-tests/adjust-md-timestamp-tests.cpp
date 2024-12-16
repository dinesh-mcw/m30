#include <gtest/gtest.h>
#include "RtdMetadata.h"

TEST(MetadataTests, adjust_metadata_time_test) {
    struct Metadata_t mtd {};

    // NOLINTBEGIN(readability-magic-numbers)
    // Test: add zero, which leaves everything alone
    mtd.timestamp0 = 0x0125;
    mtd.timestamp1 = 0x3456;
    mtd.timestamp2 = 0x6787;
    mtd.timestamp3 = 0x9ab8;
    mtd.timestamp4 = 0xcde9;
    mtd.timestamp5 = 0xf01a;
    mtd.timestamp6 = 0x234b;

    RtdMetadata::adjustTimestamp((uint8_t *)&mtd, 0);

    EXPECT_EQ(mtd.timestamp0, 0x0125);
    EXPECT_EQ(mtd.timestamp1, 0x3456);
    EXPECT_EQ(mtd.timestamp2, 0x6787);
    EXPECT_EQ(mtd.timestamp3, 0x9ab8);
    EXPECT_EQ(mtd.timestamp4, 0xcde9);
    EXPECT_EQ(mtd.timestamp5, 0xf01a);
    EXPECT_EQ(mtd.timestamp6, 0x234b);

    // Test: add 1, which clears lower nybbles in the seconds
    mtd.timestamp0 = 0x0125;
    mtd.timestamp1 = 0x3456;
    mtd.timestamp2 = 0x6787;
    mtd.timestamp3 = 0x9ab8;
    mtd.timestamp4 = 0xcde9;
    mtd.timestamp5 = 0xf01a;
    mtd.timestamp6 = 0x234b;

    RtdMetadata::adjustTimestamp((uint8_t *)&mtd, 1);

    EXPECT_EQ(mtd.timestamp0, 0x0125);
    EXPECT_EQ(mtd.timestamp1, 0x3456);
    EXPECT_EQ(mtd.timestamp2, 0x7780);
    EXPECT_EQ(mtd.timestamp3, 0x9ab0);
    EXPECT_EQ(mtd.timestamp4, 0xcde0);
    EXPECT_EQ(mtd.timestamp5, 0xf010);
    EXPECT_EQ(mtd.timestamp6, 0x2340);

    // Test: add a big number
    mtd.timestamp0 = 0x0125;
    mtd.timestamp1 = 0x3456;
    mtd.timestamp2 = 0x6787;
    mtd.timestamp3 = 0x9ab8;
    mtd.timestamp4 = 0xcde9;
    mtd.timestamp5 = 0xf01a;
    mtd.timestamp6 = 0x234b;

    RtdMetadata::adjustTimestamp((uint8_t *)&mtd, 0xabbacddceffeaULL);

    EXPECT_EQ(mtd.timestamp0, 0x0125);
    EXPECT_EQ(mtd.timestamp1, 0x3456);
    EXPECT_EQ(mtd.timestamp2, 0x0780);
    EXPECT_EQ(mtd.timestamp3, 0x9aa0);
    EXPECT_EQ(mtd.timestamp4, 0xaad0);
    EXPECT_EQ(mtd.timestamp5, 0x9cf0);
    EXPECT_EQ(mtd.timestamp6, 0xcf00);

    // Test: add a big number that overflows the seconds
    mtd.timestamp0 = 0xfedc;
    mtd.timestamp1 = 0xba98;
    mtd.timestamp2 = 0x7654;
    mtd.timestamp3 = 0x3210;
    mtd.timestamp4 = 0xcdef;
    mtd.timestamp5 = 0x89ab;
    mtd.timestamp6 = 0xcdef;

    RtdMetadata::adjustTimestamp((uint8_t *)&mtd, 0x89abcdef01234ULL);

    EXPECT_EQ(mtd.timestamp0, 0xfedc);
    EXPECT_EQ(mtd.timestamp1, 0xba98);
    EXPECT_EQ(mtd.timestamp2, 0xb650);
    EXPECT_EQ(mtd.timestamp3, 0x4440);
    EXPECT_EQ(mtd.timestamp4, 0xbce0);
    EXPECT_EQ(mtd.timestamp5, 0x4680);
    EXPECT_EQ(mtd.timestamp6, 0x5790);

    // NOLINTEND(readability-magic-numbers)
}
