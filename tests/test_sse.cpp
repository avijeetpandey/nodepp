// ═══════════════════════════════════════════════════════════════════
//  test_sse.cpp — Tests for Server-Sent Events
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/sse.h>

using namespace nodepp::sse;

TEST(SSEEventTest, BasicEventSerialization) {
    Event evt{"Hello, World!"};
    auto serialized = evt.serialize();
    EXPECT_EQ(serialized, "data: Hello, World!\n\n");
}

TEST(SSEEventTest, EventWithName) {
    Event evt{"payload", "update"};
    auto serialized = evt.serialize();
    EXPECT_TRUE(serialized.find("event: update\n") != std::string::npos);
    EXPECT_TRUE(serialized.find("data: payload\n") != std::string::npos);
}

TEST(SSEEventTest, EventWithId) {
    Event evt{"data", "msg", "42"};
    auto serialized = evt.serialize();
    EXPECT_TRUE(serialized.find("id: 42\n") != std::string::npos);
}

TEST(SSEEventTest, EventWithRetry) {
    Event evt;
    evt.data = "data";
    evt.retry = 5000;
    auto serialized = evt.serialize();
    EXPECT_TRUE(serialized.find("retry: 5000\n") != std::string::npos);
}

TEST(SSEEventTest, MultiLineData) {
    Event evt{"line1\nline2\nline3"};
    auto serialized = evt.serialize();
    EXPECT_TRUE(serialized.find("data: line1\n") != std::string::npos);
    EXPECT_TRUE(serialized.find("data: line2\n") != std::string::npos);
    EXPECT_TRUE(serialized.find("data: line3\n") != std::string::npos);
}

TEST(SSEWriterTest, SendEvents) {
    std::string output;
    Writer writer([&output](const std::string& chunk) { output += chunk; });

    writer.send("Hello");
    writer.send("World", "greeting");

    EXPECT_TRUE(output.find("data: Hello\n") != std::string::npos);
    EXPECT_TRUE(output.find("event: greeting\n") != std::string::npos);
    EXPECT_TRUE(output.find("data: World\n") != std::string::npos);
}

TEST(SSEWriterTest, SendComment) {
    std::string output;
    Writer writer([&output](const std::string& chunk) { output += chunk; });

    writer.comment("keepalive");
    EXPECT_EQ(output, ": keepalive\n\n");
}

TEST(SSEWriterTest, CloseWriter) {
    Writer writer([](const std::string&) {});
    EXPECT_FALSE(writer.isClosed());

    writer.close();
    EXPECT_TRUE(writer.isClosed());
}
