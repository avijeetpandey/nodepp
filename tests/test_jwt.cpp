// ═══════════════════════════════════════════════════════════════════
//  test_jwt.cpp — Tests for JWT sign/verify/decode
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/jwt.h>

using namespace nodepp::jwt;

const std::string SECRET = "super-secret-key-for-testing";

TEST(JWTTest, SignAndVerify) {
    nlohmann::json payload = {{"userId", 123}, {"role", "admin"}};
    auto token = sign(payload, SECRET);

    EXPECT_FALSE(token.empty());
    // Token should have 3 parts separated by dots
    int dotCount = 0;
    for (char c : token) if (c == '.') dotCount++;
    EXPECT_EQ(dotCount, 2);

    auto decoded = verify(token, SECRET);
    EXPECT_TRUE(decoded.valid);
    EXPECT_EQ(decoded.payload["userId"], 123);
    EXPECT_EQ(decoded.payload["role"], "admin");
}

TEST(JWTTest, DecodeWithoutVerification) {
    nlohmann::json payload = {{"name", "Alice"}};
    auto token = sign(payload, SECRET);

    auto decoded = decode(token);
    EXPECT_TRUE(decoded.error.empty());
    EXPECT_EQ(decoded.header["alg"], "HS256");
    EXPECT_EQ(decoded.payload["name"], "Alice");
}

TEST(JWTTest, InvalidSignatureRejected) {
    nlohmann::json payload = {{"test", true}};
    auto token = sign(payload, SECRET);

    auto decoded = verify(token, "wrong-secret");
    EXPECT_FALSE(decoded.valid);
    EXPECT_EQ(decoded.error, "Invalid signature");
}

TEST(JWTTest, ExpiredTokenRejected) {
    nlohmann::json payload = {{"test", true}};
    SignOptions opts;
    opts.expiresInSec = -1; // Already expired
    auto token = sign(payload, SECRET, opts);

    auto decoded = verify(token, SECRET);
    EXPECT_FALSE(decoded.valid);
    EXPECT_EQ(decoded.error, "Token expired");
}

TEST(JWTTest, TokenContainsIssuerAndSubject) {
    nlohmann::json payload = {{"data", "test"}};
    SignOptions opts;
    opts.issuer = "nodepp";
    opts.subject = "user123";
    auto token = sign(payload, SECRET, opts);

    auto decoded = verify(token, SECRET);
    EXPECT_TRUE(decoded.valid);
    EXPECT_EQ(decoded.payload["iss"], "nodepp");
    EXPECT_EQ(decoded.payload["sub"], "user123");
}

TEST(JWTTest, InvalidTokenFormat) {
    auto decoded = decode("not-a-valid-token");
    EXPECT_FALSE(decoded.error.empty());
}

TEST(JWTTest, TokenContainsIssuedAt) {
    nlohmann::json payload = {{"x", 1}};
    auto token = sign(payload, SECRET);
    auto decoded = verify(token, SECRET);
    EXPECT_TRUE(decoded.valid);
    EXPECT_TRUE(decoded.payload.contains("iat"));
}
