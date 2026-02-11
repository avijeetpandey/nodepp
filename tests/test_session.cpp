// ═══════════════════════════════════════════════════════════════════
//  test_session.cpp — Tests for session management
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/session.h>

using namespace nodepp;
using namespace nodepp::session;

TEST(SessionStoreTest, MemoryStoreSetAndGet) {
    MemoryStore store;
    JsonValue data(nlohmann::json{{"user", "Alice"}, {"role", "admin"}});
    store.set("sid1", data);

    auto retrieved = store.get("sid1");
    EXPECT_FALSE(retrieved.isNull());
    EXPECT_EQ(retrieved["user"].get<std::string>(), "Alice");
}

TEST(SessionStoreTest, MemoryStoreDestroy) {
    MemoryStore store;
    store.set("sid1", JsonValue(nlohmann::json{{"x", 1}}));
    EXPECT_FALSE(store.get("sid1").isNull());

    store.destroy("sid1");
    EXPECT_TRUE(store.get("sid1").isNull());
}

TEST(SessionStoreTest, MemoryStoreSize) {
    MemoryStore store;
    EXPECT_EQ(store.size(), 0u);

    store.set("a", JsonValue(nlohmann::json{{"x", 1}}));
    store.set("b", JsonValue(nlohmann::json{{"y", 2}}));
    EXPECT_EQ(store.size(), 2u);
}

TEST(SessionStoreTest, NonExistentSessionReturnsNull) {
    MemoryStore store;
    auto data = store.get("nonexistent");
    EXPECT_TRUE(data.isNull());
}

TEST(SessionDetailTest, GeneratesSid) {
    auto sid1 = detail::generateSid();
    auto sid2 = detail::generateSid();
    EXPECT_FALSE(sid1.empty());
    EXPECT_NE(sid1, sid2);
}

TEST(SessionDetailTest, BuildsSetCookie) {
    SessionOptions opts;
    opts.cookieName = "sid";
    opts.maxAge = 3600000;
    opts.httpOnly = true;
    opts.secure = false;
    opts.sameSite = "Lax";

    auto cookie = detail::buildSetCookie("sid", "abc123", opts);
    EXPECT_TRUE(cookie.find("sid=abc123") != std::string::npos);
    EXPECT_TRUE(cookie.find("HttpOnly") != std::string::npos);
    EXPECT_TRUE(cookie.find("SameSite=Lax") != std::string::npos);
    EXPECT_TRUE(cookie.find("Max-Age=3600") != std::string::npos);
}
