// ═══════════════════════════════════════════════════════════════════
//  test_openapi.cpp — Tests for OpenAPI spec generation
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/openapi.h>

using namespace nodepp::openapi;

TEST(OpenAPITest, BasicDocumentGeneration) {
    Document doc;
    doc.title("Test API")
       .description("A test API")
       .version("1.0.0");

    auto spec = doc.generate();
    EXPECT_EQ(spec["openapi"], "3.0.3");
    EXPECT_EQ(spec["info"]["title"], "Test API");
    EXPECT_EQ(spec["info"]["version"], "1.0.0");
}

TEST(OpenAPITest, ServerConfiguration) {
    Document doc;
    doc.server("http://localhost:3000", "Development")
       .server("https://api.example.com", "Production");

    auto spec = doc.generate();
    ASSERT_TRUE(spec.contains("servers"));
    EXPECT_EQ(spec["servers"].size(), 2u);
    EXPECT_EQ(spec["servers"][0]["url"], "http://localhost:3000");
}

TEST(OpenAPITest, RouteDocumentation) {
    Document doc;
    doc.route({
        .method = "GET",
        .path = "/users",
        .summary = "List all users",
        .tags = {"Users"},
        .successStatus = 200
    });

    auto spec = doc.generate();
    ASSERT_TRUE(spec["paths"].contains("/users"));
    EXPECT_EQ(spec["paths"]["/users"]["get"]["summary"], "List all users");
}

TEST(OpenAPITest, PathParameterConversion) {
    Document doc;
    doc.route({
        .method = "GET",
        .path = "/users/:id",
        .summary = "Get user by ID"
    });

    auto spec = doc.generate();
    EXPECT_TRUE(spec["paths"].contains("/users/{id}"));
    auto params = spec["paths"]["/users/{id}"]["get"]["parameters"];
    ASSERT_EQ(params.size(), 1u);
    EXPECT_EQ(params[0]["name"], "id");
    EXPECT_EQ(params[0]["in"], "path");
}

TEST(OpenAPITest, RequestBody) {
    Document doc;
    doc.route({
        .method = "POST",
        .path = "/users",
        .summary = "Create user",
        .requestBodyType = "application/json",
        .requestSchema = {
            {"type", "object"},
            {"properties", {
                {"name", {{"type", "string"}}},
                {"email", {{"type", "string"}}}
            }}
        }
    });

    auto spec = doc.generate();
    auto op = spec["paths"]["/users"]["post"];
    EXPECT_TRUE(op.contains("requestBody"));
    EXPECT_TRUE(op["requestBody"]["content"].contains("application/json"));
}

TEST(OpenAPITest, ResponseSchema) {
    Document doc;
    doc.route({
        .method = "GET",
        .path = "/users",
        .summary = "List users",
        .successStatus = 200,
        .responseSchema = {
            {"type", "array"},
            {"items", {{"type", "object"}}}
        }
    });

    auto spec = doc.generate();
    auto responses = spec["paths"]["/users"]["get"]["responses"];
    EXPECT_TRUE(responses.contains("200"));
    EXPECT_TRUE(responses["200"].contains("content"));
}

TEST(OpenAPITest, MultipleMethods) {
    Document doc;
    doc.route({.method = "GET", .path = "/users", .summary = "List"})
       .route({.method = "POST", .path = "/users", .summary = "Create"});

    auto spec = doc.generate();
    EXPECT_TRUE(spec["paths"]["/users"].contains("get"));
    EXPECT_TRUE(spec["paths"]["/users"].contains("post"));
}

TEST(OpenAPITest, Tags) {
    Document doc;
    doc.route({
        .method = "GET",
        .path = "/users",
        .summary = "List users",
        .tags = {"Users", "Admin"}
    });

    auto spec = doc.generate();
    auto tags = spec["paths"]["/users"]["get"]["tags"];
    EXPECT_EQ(tags.size(), 2u);
    EXPECT_EQ(tags[0], "Users");
}
