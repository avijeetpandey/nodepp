// ═══════════════════════════════════════════════════════════════════
//  test_testing.cpp — Tests for TestClient and mock factories
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/testing.h>

using namespace nodepp;
using namespace nodepp::testing;

class TestClientTest : public ::testing::Test {
protected:
    http::Server app;

    void SetUp() override {
        app.get("/hello", [](http::Request&, http::Response& res) {
            res.json({{"message", "Hello, World!"}});
        });

        app.post("/echo", [](http::Request& req, http::Response& res) {
            res.json(req.body.raw());
        });

        app.get("/users/:id", [](http::Request& req, http::Response& res) {
            res.json({{"id", req.params["id"]}});
        });

        app.get("/status", [](http::Request&, http::Response& res) {
            res.status(201).json({{"created", true}});
        });
    }
};

TEST_F(TestClientTest, SimpleGet) {
    TestClient client(app);
    auto result = client.get("/hello").exec();

    EXPECT_EQ(result.status, 200);
    auto j = result.json();
    EXPECT_EQ(j["message"], "Hello, World!");
}

TEST_F(TestClientTest, PostWithJsonBody) {
    TestClient client(app);
    auto result = client.post("/echo")
        .send(nlohmann::json{{"name", "Alice"}})
        .exec();

    EXPECT_EQ(result.status, 200);
    auto j = result.json();
    EXPECT_EQ(j["name"], "Alice");
}

TEST_F(TestClientTest, ExpectStatus) {
    TestClient client(app);
    auto result = client.get("/hello").expect(200);
    EXPECT_EQ(result.status, 200);
}

TEST_F(TestClientTest, ExpectStatusThrowsOnMismatch) {
    TestClient client(app);
    EXPECT_THROW(client.get("/hello").expect(404), std::runtime_error);
}

TEST_F(TestClientTest, SetCustomHeaders) {
    TestClient client(app);
    auto result = client.get("/hello")
        .set("X-Custom", "value")
        .exec();
    EXPECT_EQ(result.status, 200);
}

TEST(MockFactoryTest, CreateRequest) {
    auto req = createRequest("POST", "/api/test", "{\"x\":1}",
                             {{"Content-Type", "application/json"}});

    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.path, "/api/test");
    EXPECT_EQ(req.rawBody, "{\"x\":1}");
}

TEST(MockFactoryTest, CreateResponse) {
    auto res = createResponse();
    res.status(201).json({{"ok", true}});

    EXPECT_EQ(res.getStatusCode(), 201);
    EXPECT_FALSE(res.getBody().empty());
}
