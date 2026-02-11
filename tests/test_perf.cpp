// ═══════════════════════════════════════════════════════════════════
//  test_perf.cpp — Tests for performance utilities
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/perf.h>
#include <cstring>

using namespace nodepp::perf;

TEST(ArenaTest, BasicAllocation) {
    Arena arena;
    auto* ptr = arena.allocate(100);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(arena.totalAllocated(), 100u);
}

TEST(ArenaTest, MultipleAllocations) {
    Arena arena;
    arena.allocate(100);
    arena.allocate(200);
    arena.allocate(300);
    EXPECT_EQ(arena.totalAllocated(), 600u);
}

TEST(ArenaTest, CreateObject) {
    Arena arena;
    auto* val = arena.create<int>(42);
    EXPECT_NE(val, nullptr);
    EXPECT_EQ(*val, 42);
}

TEST(ArenaTest, AllocString) {
    Arena arena;
    auto* str = arena.allocString("Hello, World!");
    EXPECT_STREQ(str, "Hello, World!");
}

TEST(ArenaTest, Reset) {
    Arena arena;
    arena.allocate(1000);
    arena.allocate(2000);

    arena.reset();
    EXPECT_EQ(arena.totalAllocated(), 0u);
}

TEST(ArenaTest, LargeAllocation) {
    Arena arena(64); // Small block size
    auto* ptr = arena.allocate(1000); // Larger than block
    EXPECT_NE(ptr, nullptr);
}

TEST(ObjectPoolTest, AcquireAndRelease) {
    ObjectPool<std::string> pool(5);
    EXPECT_EQ(pool.available(), 5u);

    auto obj = pool.acquire();
    EXPECT_EQ(pool.available(), 4u);

    pool.release(std::move(obj));
    EXPECT_EQ(pool.available(), 5u);
}

TEST(ObjectPoolTest, AcquireWhenEmpty) {
    ObjectPool<std::string> pool(0);
    auto obj = pool.acquire(); // Should create new
    EXPECT_NE(obj, nullptr);
}

TEST(ObjectPoolTest, MultipleAcquire) {
    ObjectPool<int> pool(3);
    auto a = pool.acquire();
    auto b = pool.acquire();
    auto c = pool.acquire();
    EXPECT_EQ(pool.available(), 0u);

    auto d = pool.acquire(); // Creates new
    EXPECT_NE(d, nullptr);
}

TEST(ParseTest, SplitString) {
    auto parts = parse::split("a/b/c/d", '/');
    ASSERT_EQ(parts.size(), 4u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[3], "d");
}

TEST(ParseTest, SplitEmpty) {
    auto parts = parse::split("", '/');
    EXPECT_TRUE(parts.empty());
}

TEST(ParseTest, TrimWhitespace) {
    EXPECT_EQ(parse::trim("  hello  "), "hello");
    EXPECT_EQ(parse::trim("\t\ntest\r\n"), "test");
    EXPECT_EQ(parse::trim("none"), "none");
}

TEST(ParseTest, ParseKeyValue) {
    auto kv = parse::parseKeyValue("name=Alice");
    EXPECT_EQ(kv.key, "name");
    EXPECT_EQ(kv.value, "Alice");
}

TEST(ParseTest, ParseQueryString) {
    auto params = parse::parseQueryString("name=Alice&age=30&city=NYC");
    ASSERT_EQ(params.size(), 3u);
    EXPECT_EQ(params[0].key, "name");
    EXPECT_EQ(params[0].value, "Alice");
    EXPECT_EQ(params[1].key, "age");
    EXPECT_EQ(params[1].value, "30");
}

TEST(ParseTest, FastParseInt) {
    int val;
    EXPECT_TRUE(parse::parseInt("12345", val));
    EXPECT_EQ(val, 12345);

    EXPECT_TRUE(parse::parseInt("-42", val));
    EXPECT_EQ(val, -42);

    EXPECT_FALSE(parse::parseInt("abc", val));
    EXPECT_FALSE(parse::parseInt("", val));
}
