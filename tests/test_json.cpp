// ═══════════════════════════════════════════════════════════════════
//  test_json.cpp — Verify automatic JSON serialization engine
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include "nodepp/json_utils.h"
#include "nodepp/http.h"
#include <map>
#include <vector>
#include <string>

using namespace nodepp;

// ── Test struct with NODE_SERIALIZE ──
struct User {
    std::string name;
    int id;
    NODE_SERIALIZE(User, name, id)
};

struct Post {
    std::string title;
    std::string content;
    int authorId;
    NODE_SERIALIZE(Post, title, content, authorId)
};

// ═══════════════════════════════════════════
//  JsonValue Tests
// ═══════════════════════════════════════════

TEST(JsonValueTest, DefaultConstructor) {
    JsonValue val;
    EXPECT_TRUE(val.isObject());
    EXPECT_EQ(val.size(), 0);
}

TEST(JsonValueTest, FromNlohmannJson) {
    nlohmann::json j = {{"name", "Alice"}, {"age", 30}};
    JsonValue val(j);

    EXPECT_TRUE(val.isObject());
    EXPECT_EQ(val["name"].get<std::string>(), "Alice");
    EXPECT_EQ(val["age"].get<int>(), 30);
}

TEST(JsonValueTest, SubscriptOperator) {
    JsonValue val(nlohmann::json{{"key", "value"}, {"num", 42}});

    // String conversion
    std::string s = val["key"];
    EXPECT_EQ(s, "value");

    // Int conversion
    int n = val["num"];
    EXPECT_EQ(n, 42);

    // Non-existent key
    EXPECT_TRUE(val["missing"].isNull());
}

TEST(JsonValueTest, ArrayAccess) {
    nlohmann::json arr = {1, 2, 3, 4, 5};
    JsonValue val(arr);

    EXPECT_TRUE(val.isArray());
    EXPECT_EQ(val.size(), 5);
    EXPECT_EQ(val[0].get<int>(), 1);
    EXPECT_EQ(val[4].get<int>(), 5);
}

TEST(JsonValueTest, TypedGetWithDefault) {
    JsonValue val(nlohmann::json{{"name", "Bob"}});

    EXPECT_EQ(val.get<std::string>("name", "default"), "Bob");
    EXPECT_EQ(val.get<std::string>("missing", "default"), "default");
    EXPECT_EQ(val.get<int>("missing", 42), 42);
}

TEST(JsonValueTest, HasMethod) {
    JsonValue val(nlohmann::json{{"x", 1}});
    EXPECT_TRUE(val.has("x"));
    EXPECT_FALSE(val.has("y"));
}

TEST(JsonValueTest, DumpMethod) {
    JsonValue val(nlohmann::json{{"a", 1}});
    std::string dumped = val.dump();
    EXPECT_NE(dumped.find("\"a\""), std::string::npos);
    EXPECT_NE(dumped.find("1"), std::string::npos);
}

// ═══════════════════════════════════════════
//  Struct Serialization Tests
// ═══════════════════════════════════════════

TEST(SerializationTest, StructToJson) {
    User user{"Alice", 42};
    nlohmann::json j = user;

    EXPECT_EQ(j["name"], "Alice");
    EXPECT_EQ(j["id"], 42);
}

TEST(SerializationTest, JsonToStruct) {
    nlohmann::json j = {{"name", "Bob"}, {"id", 7}};
    User user = j.get<User>();

    EXPECT_EQ(user.name, "Bob");
    EXPECT_EQ(user.id, 7);
}

TEST(SerializationTest, NestedStructSerialization) {
    Post post{"Hello World", "This is a post", 1};
    nlohmann::json j = post;

    EXPECT_EQ(j["title"], "Hello World");
    EXPECT_EQ(j["content"], "This is a post");
    EXPECT_EQ(j["authorId"], 1);
}

// ═══════════════════════════════════════════
//  res.json() Auto-Serialization Tests
//  Verifies that sending any C++ type produces valid JSON
// ═══════════════════════════════════════════

TEST(ResponseJsonTest, MapToJson) {
    std::string sentBody;
    int sentStatus = 0;

    http::Response res([&](int status, const auto& headers, const std::string& body) {
        sentStatus = status;
        sentBody = body;
    });

    std::map<std::string, int> data = {{"a", 1}, {"b", 2}, {"c", 3}};
    res.json(data);

    // Parse the sent body back as JSON
    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_EQ(parsed["a"], 1);
    EXPECT_EQ(parsed["b"], 2);
    EXPECT_EQ(parsed["c"], 3);
    EXPECT_EQ(sentStatus, 200);
}

TEST(ResponseJsonTest, VectorToJson) {
    std::string sentBody;

    http::Response res([&](int, const auto&, const std::string& body) {
        sentBody = body;
    });

    std::vector<int> data = {10, 20, 30};
    res.json(data);

    auto parsed = nlohmann::json::parse(sentBody);
    ASSERT_TRUE(parsed.is_array());
    EXPECT_EQ(parsed.size(), 3);
    EXPECT_EQ(parsed[0], 10);
}

TEST(ResponseJsonTest, StructToJson) {
    std::string sentBody;

    http::Response res([&](int, const auto&, const std::string& body) {
        sentBody = body;
    });

    User user{"Charlie", 99};
    res.json(user);

    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_EQ(parsed["name"], "Charlie");
    EXPECT_EQ(parsed["id"], 99);
}

TEST(ResponseJsonTest, InitializerListToJson) {
    std::string sentBody;

    http::Response res([&](int, const auto&, const std::string& body) {
        sentBody = body;
    });

    res.json({{"status", "ok"}, {"count", 5}});

    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_EQ(parsed["status"], "ok");
    EXPECT_EQ(parsed["count"], 5);
}

TEST(ResponseJsonTest, NestedMapToJson) {
    std::string sentBody;

    http::Response res([&](int, const auto&, const std::string& body) {
        sentBody = body;
    });

    std::map<std::string, std::vector<int>> data = {
        {"numbers", {1, 2, 3}},
        {"more", {4, 5}}
    };
    res.json(data);

    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_EQ(parsed["numbers"].size(), 3);
    EXPECT_EQ(parsed["numbers"][0], 1);
    EXPECT_EQ(parsed["more"].size(), 2);
}

TEST(ResponseJsonTest, ResponseStatus) {
    std::string sentBody;
    int sentStatus = 0;

    http::Response res([&](int status, const auto&, const std::string& body) {
        sentStatus = status;
        sentBody = body;
    });

    res.status(201).json({{"created", true}});

    EXPECT_EQ(sentStatus, 201);
    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_TRUE(parsed["created"]);
}

TEST(ResponseJsonTest, HeadersSetCorrectly) {
    std::unordered_map<std::string, std::string> sentHeaders;

    http::Response res([&](int, const auto& headers, const std::string&) {
        sentHeaders = headers;
    });

    res.json({{"test", true}});

    EXPECT_EQ(sentHeaders["Content-Type"], "application/json; charset=utf-8");
}

// ═══════════════════════════════════════════
//  Concept Verification
// ═══════════════════════════════════════════

TEST(ConceptTest, JsonSerializableConcept) {
    // These should all satisfy JsonSerializable
    static_assert(JsonSerializable<int>);
    static_assert(JsonSerializable<double>);
    static_assert(JsonSerializable<std::string>);
    static_assert(JsonSerializable<std::vector<int>>);
    static_assert(JsonSerializable<std::map<std::string, int>>);
    static_assert(JsonSerializable<User>);
    static_assert(JsonSerializable<nlohmann::json>);
}
