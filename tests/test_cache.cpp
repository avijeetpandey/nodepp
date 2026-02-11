// ═══════════════════════════════════════════════════════════════════
//  test_cache.cpp — Tests for LRU cache and ETag
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/cache.h>
#include <thread>

using namespace nodepp::cache;

TEST(LRUCacheTest, BasicSetAndGet) {
    LRUCache<> cache(100);
    cache.set("key1", "value1");
    auto val = cache.get("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "value1");
}

TEST(LRUCacheTest, MissReturnsNullopt) {
    LRUCache<> cache(100);
    auto val = cache.get("missing");
    EXPECT_FALSE(val.has_value());
}

TEST(LRUCacheTest, EvictsOldEntries) {
    LRUCache<> cache(3);
    cache.set("a", "1");
    cache.set("b", "2");
    cache.set("c", "3");
    cache.set("d", "4"); // Should evict "a"

    EXPECT_FALSE(cache.get("a").has_value());
    EXPECT_TRUE(cache.get("b").has_value());
    EXPECT_TRUE(cache.get("d").has_value());
}

TEST(LRUCacheTest, LRUEvictionOrder) {
    LRUCache<> cache(3);
    cache.set("a", "1");
    cache.set("b", "2");
    cache.set("c", "3");

    // Access "a" to make it most recently used
    cache.get("a");

    cache.set("d", "4"); // Should evict "b" (least recently used)

    EXPECT_TRUE(cache.get("a").has_value());
    EXPECT_FALSE(cache.get("b").has_value());
    EXPECT_TRUE(cache.get("d").has_value());
}

TEST(LRUCacheTest, TTLExpiry) {
    LRUCache<> cache(100, 50); // 50ms default TTL
    cache.set("key", "value");

    auto val = cache.get("key");
    ASSERT_TRUE(val.has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    val = cache.get("key");
    EXPECT_FALSE(val.has_value());
}

TEST(LRUCacheTest, DeleteEntry) {
    LRUCache<> cache(100);
    cache.set("key", "value");
    EXPECT_TRUE(cache.has("key"));

    cache.del("key");
    EXPECT_FALSE(cache.has("key"));
}

TEST(LRUCacheTest, ClearAll) {
    LRUCache<> cache(100);
    cache.set("a", "1");
    cache.set("b", "2");
    EXPECT_EQ(cache.size(), 2u);

    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
}

TEST(LRUCacheTest, OverwriteExisting) {
    LRUCache<> cache(100);
    cache.set("key", "old");
    cache.set("key", "new");

    auto val = cache.get("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "new");
    EXPECT_EQ(cache.size(), 1u);
}

TEST(ETagTest, GeneratesConsistentETags) {
    auto etag1 = generateETag("Hello, World!");
    auto etag2 = generateETag("Hello, World!");
    EXPECT_EQ(etag1, etag2);
}

TEST(ETagTest, DifferentContentDifferentETags) {
    auto etag1 = generateETag("Hello");
    auto etag2 = generateETag("World");
    EXPECT_NE(etag1, etag2);
}

TEST(ETagTest, ETagFormat) {
    auto etag = generateETag("test");
    EXPECT_TRUE(etag.front() == '"');
    EXPECT_TRUE(etag.back() == '"');
}
