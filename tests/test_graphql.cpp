// ═══════════════════════════════════════════════════════════════════
//  test_graphql.cpp — GraphQL parsing and execution tests
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include "nodepp/graphql.h"
#include "nodepp/json_utils.h"

using namespace nodepp;
using namespace nodepp::graphql;

// ═══════════════════════════════════════════
//  Parser Tests
// ═══════════════════════════════════════════

TEST(GraphQLParserTest, SimpleQuery) {
    detail::GraphQLParser parser("{ user }");
    auto parsed = parser.parse();

    EXPECT_EQ(parsed.operationType, "query");
    ASSERT_EQ(parsed.selections.size(), 1);
    EXPECT_EQ(parsed.selections[0].name, "user");
}

TEST(GraphQLParserTest, ExplicitQueryKeyword) {
    detail::GraphQLParser parser("query { user posts }");
    auto parsed = parser.parse();

    EXPECT_EQ(parsed.operationType, "query");
    ASSERT_EQ(parsed.selections.size(), 2);
    EXPECT_EQ(parsed.selections[0].name, "user");
    EXPECT_EQ(parsed.selections[1].name, "posts");
}

TEST(GraphQLParserTest, MutationKeyword) {
    detail::GraphQLParser parser("mutation { createUser }");
    auto parsed = parser.parse();

    EXPECT_EQ(parsed.operationType, "mutation");
    ASSERT_EQ(parsed.selections.size(), 1);
    EXPECT_EQ(parsed.selections[0].name, "createUser");
}

TEST(GraphQLParserTest, QueryWithArguments) {
    detail::GraphQLParser parser(R"({ user(id: 42) })");
    auto parsed = parser.parse();

    ASSERT_EQ(parsed.selections.size(), 1);
    EXPECT_EQ(parsed.selections[0].arguments["id"], 42);
}

TEST(GraphQLParserTest, QueryWithStringArgument) {
    detail::GraphQLParser parser(R"({ user(name: "Alice") })");
    auto parsed = parser.parse();

    ASSERT_EQ(parsed.selections.size(), 1);
    EXPECT_EQ(parsed.selections[0].arguments["name"], "Alice");
}

TEST(GraphQLParserTest, NestedSelections) {
    detail::GraphQLParser parser(R"({
        user(id: 1) {
            name
            email
            posts {
                title
            }
        }
    })");
    auto parsed = parser.parse();

    ASSERT_EQ(parsed.selections.size(), 1);
    auto& user = parsed.selections[0];
    EXPECT_EQ(user.name, "user");
    ASSERT_EQ(user.selections.size(), 3);
    EXPECT_EQ(user.selections[0].name, "name");
    EXPECT_EQ(user.selections[1].name, "email");
    EXPECT_EQ(user.selections[2].name, "posts");
    ASSERT_EQ(user.selections[2].selections.size(), 1);
    EXPECT_EQ(user.selections[2].selections[0].name, "title");
}

TEST(GraphQLParserTest, AliasSupport) {
    detail::GraphQLParser parser(R"({
        myUser: user(id: 1) { name }
    })");
    auto parsed = parser.parse();

    ASSERT_EQ(parsed.selections.size(), 1);
    EXPECT_EQ(parsed.selections[0].alias, "myUser");
    EXPECT_EQ(parsed.selections[0].name, "user");
}

// ═══════════════════════════════════════════
//  Schema & Execution Tests
// ═══════════════════════════════════════════

TEST(SchemaTest, SimpleQueryExecution) {
    Schema schema;
    schema.query("hello", [](JsonValue args, JsonValue ctx) -> JsonValue {
        return JsonValue(nlohmann::json("world"));
    });

    auto result = schema.execute("{ hello }");

    EXPECT_EQ(result["data"]["hello"], "world");
    EXPECT_FALSE(result.contains("errors"));
}

TEST(SchemaTest, QueryWithArguments) {
    Schema schema;
    schema.query("user", [](JsonValue args, JsonValue ctx) -> JsonValue {
        int id = args["id"];
        return JsonValue(nlohmann::json{{"name", "User" + std::to_string(id)}, {"id", id}});
    });

    auto result = schema.execute(R"({ user(id: 42) { name id } })");

    EXPECT_EQ(result["data"]["user"]["name"], "User42");
    EXPECT_EQ(result["data"]["user"]["id"], 42);
}

TEST(SchemaTest, MultipleFieldsInQuery) {
    Schema schema;
    schema.query("name", [](JsonValue, JsonValue) -> JsonValue {
        return JsonValue(nlohmann::json("Alice"));
    });
    schema.query("age", [](JsonValue, JsonValue) -> JsonValue {
        return JsonValue(nlohmann::json(30));
    });

    auto result = schema.execute("{ name age }");

    EXPECT_EQ(result["data"]["name"], "Alice");
    EXPECT_EQ(result["data"]["age"], 30);
}

TEST(SchemaTest, MutationExecution) {
    Schema schema;
    schema.mutation("createUser", [](JsonValue args, JsonValue ctx) -> JsonValue {
        std::string name = args["name"];
        return JsonValue(nlohmann::json{{"id", 1}, {"name", name}});
    });

    auto result = schema.execute(R"(mutation { createUser(name: "Bob") { id name } })");

    EXPECT_EQ(result["data"]["createUser"]["id"], 1);
    EXPECT_EQ(result["data"]["createUser"]["name"], "Bob");
}

TEST(SchemaTest, UnknownFieldReturnsError) {
    Schema schema;
    auto result = schema.execute("{ unknownField }");

    EXPECT_TRUE(result.contains("errors"));
    EXPECT_TRUE(result["errors"].is_array());
    EXPECT_GT(result["errors"].size(), 0);
}

TEST(SchemaTest, FieldSelectionFiltering) {
    Schema schema;
    schema.query("user", [](JsonValue args, JsonValue ctx) -> JsonValue {
        return JsonValue(nlohmann::json{
            {"name", "Alice"},
            {"email", "alice@example.com"},
            {"age", 30},
            {"secret", "should-not-appear"}
        });
    });

    auto result = schema.execute(R"({ user { name email } })");

    auto user = result["data"]["user"];
    EXPECT_TRUE(user.contains("name"));
    EXPECT_TRUE(user.contains("email"));
    EXPECT_FALSE(user.contains("secret"));
    EXPECT_FALSE(user.contains("age"));
}

TEST(SchemaTest, AliasInExecution) {
    Schema schema;
    schema.query("user", [](JsonValue args, JsonValue ctx) -> JsonValue {
        int id = args["id"];
        return JsonValue(nlohmann::json{{"name", "User" + std::to_string(id)}, {"id", id}});
    });

    auto result = schema.execute(R"({
        alice: user(id: 1) { name }
        bob: user(id: 2) { name }
    })");

    EXPECT_EQ(result["data"]["alice"]["name"], "User1");
    EXPECT_EQ(result["data"]["bob"]["name"], "User2");
}

TEST(SchemaTest, ResolverErrorHandling) {
    Schema schema;
    schema.query("failing", [](JsonValue, JsonValue) -> JsonValue {
        throw std::runtime_error("Something went wrong");
    });

    auto result = schema.execute("{ failing }");

    EXPECT_TRUE(result["data"]["failing"].is_null());
    EXPECT_TRUE(result.contains("errors"));
    EXPECT_GT(result["errors"].size(), 0);
}

TEST(SchemaTest, ParseErrorReturnsError) {
    Schema schema;
    auto result = schema.execute("this is not valid graphql {{{");

    EXPECT_TRUE(result["data"].is_null());
    EXPECT_TRUE(result.contains("errors"));
}
