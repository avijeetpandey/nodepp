// ═══════════════════════════════════════════════════════════════════
//  test_compress.cpp — Tests for gzip compression
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/compress.h>

using namespace nodepp::compress;

TEST(CompressTest, GzipCompressAndDecompress) {
    std::string original = "Hello, this is a test string for compression. "
                           "It should be compressible because it has repetitive content. "
                           "Hello, this is a test string for compression.";

    auto compressed = gzipCompress(original);
    EXPECT_FALSE(compressed.empty());
    EXPECT_LT(compressed.size(), original.size()); // Should be smaller

    auto decompressed = gzipDecompress(compressed);
    EXPECT_EQ(decompressed, original);
}

TEST(CompressTest, EmptyInput) {
    auto compressed = gzipCompress("");
    EXPECT_FALSE(compressed.empty()); // gzip header still exists

    auto decompressed = gzipDecompress(compressed);
    EXPECT_EQ(decompressed, "");
}

TEST(CompressTest, BinaryData) {
    std::string binary;
    for (int i = 0; i < 256; i++) {
        binary += static_cast<char>(i);
    }
    binary += binary; // Repeat for compressibility

    auto compressed = gzipCompress(binary);
    auto decompressed = gzipDecompress(compressed);
    EXPECT_EQ(decompressed, binary);
}

TEST(CompressTest, LargeData) {
    std::string large(100000, 'A');
    auto compressed = gzipCompress(large);
    EXPECT_LT(compressed.size(), large.size());

    auto decompressed = gzipDecompress(compressed);
    EXPECT_EQ(decompressed, large);
}

TEST(CompressTest, CompressionLevels) {
    std::string data(10000, 'X');

    auto fast = gzipCompress(data, 1);
    auto best = gzipCompress(data, 9);

    // Both should decompress correctly
    EXPECT_EQ(gzipDecompress(fast), data);
    EXPECT_EQ(gzipDecompress(best), data);

    // Best compression should be <= fast compression
    EXPECT_LE(best.size(), fast.size());
}
