// ═══════════════════════════════════════════════════════════════════
//  test_crypto.cpp — Tests for crypto module
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/crypto.h>

using namespace nodepp::crypto;

TEST(CryptoHashTest, SHA256) {
    auto hash = sha256("hello");
    EXPECT_EQ(hash, "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST(CryptoHashTest, SHA256Empty) {
    auto hash = sha256("");
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(CryptoHashTest, SHA512) {
    auto hash = sha512("hello");
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.size(), 128u); // 64 bytes = 128 hex chars
}

TEST(CryptoHashTest, MD5) {
    auto hash = md5("hello");
    EXPECT_EQ(hash, "5d41402abc4b2a76b9719d911017c592");
}

TEST(CryptoHMACTest, HmacSha256) {
    auto hmac = hmacSha256Hex("secret", "hello");
    EXPECT_FALSE(hmac.empty());
    EXPECT_EQ(hmac.size(), 64u); // 32 bytes = 64 hex chars
}

TEST(CryptoHMACTest, ConsistentResults) {
    auto h1 = hmacSha256Hex("key", "data");
    auto h2 = hmacSha256Hex("key", "data");
    EXPECT_EQ(h1, h2);
}

TEST(CryptoHMACTest, DifferentKeysProduceDifferentResults) {
    auto h1 = hmacSha256Hex("key1", "data");
    auto h2 = hmacSha256Hex("key2", "data");
    EXPECT_NE(h1, h2);
}

TEST(Base64Test, EncodeAndDecode) {
    auto encoded = base64Encode("Hello, World!");
    EXPECT_EQ(encoded, "SGVsbG8sIFdvcmxkIQ==");

    auto decoded = base64Decode(encoded);
    EXPECT_EQ(decoded, "Hello, World!");
}

TEST(Base64Test, EmptyString) {
    EXPECT_EQ(base64Encode(""), "");
    EXPECT_EQ(base64Decode(""), "");
}

TEST(Base64Test, UrlSafeEncoding) {
    auto encoded = base64UrlEncode("test?data+more");
    EXPECT_TRUE(encoded.find('+') == std::string::npos);
    EXPECT_TRUE(encoded.find('/') == std::string::npos);
    EXPECT_TRUE(encoded.find('=') == std::string::npos);

    auto decoded = base64UrlDecode(encoded);
    EXPECT_EQ(decoded, "test?data+more");
}

TEST(CryptoRandomTest, RandomBytesLength) {
    auto bytes = randomBytes(32);
    EXPECT_EQ(bytes.size(), 32u);
}

TEST(CryptoRandomTest, RandomBytesAreRandom) {
    auto b1 = randomHex(16);
    auto b2 = randomHex(16);
    EXPECT_NE(b1, b2); // Extremely unlikely to collide
}

TEST(CryptoRandomTest, RandomHexFormat) {
    auto hex = randomHex(16);
    EXPECT_EQ(hex.size(), 32u); // 16 bytes = 32 hex chars
    for (char c : hex) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

TEST(UUIDTest, FormatIsCorrect) {
    auto id = uuid();
    EXPECT_EQ(id.size(), 36u); // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    EXPECT_EQ(id[8], '-');
    EXPECT_EQ(id[13], '-');
    EXPECT_EQ(id[18], '-');
    EXPECT_EQ(id[23], '-');
}

TEST(UUIDTest, Version4) {
    auto id = uuid();
    EXPECT_EQ(id[14], '4'); // Version nibble
}

TEST(UUIDTest, Uniqueness) {
    auto id1 = uuid();
    auto id2 = uuid();
    EXPECT_NE(id1, id2);
}

TEST(TimingSafeTest, EqualStrings) {
    EXPECT_TRUE(timingSafeEqual("hello", "hello"));
}

TEST(TimingSafeTest, DifferentStrings) {
    EXPECT_FALSE(timingSafeEqual("hello", "world"));
}

TEST(TimingSafeTest, DifferentLengths) {
    EXPECT_FALSE(timingSafeEqual("short", "longer string"));
}
