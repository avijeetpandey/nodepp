// ═══════════════════════════════════════════════════════════════════
//  test_http.cpp — Unit tests for HTTP Request, Response, and routing
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include "nodepp/http.h"
#include "nodepp/middleware.h"
#include "nodepp/json_utils.h"

using namespace nodepp;
using namespace nodepp::http;

// ═══════════════════════════════════════════
//  Request Tests
// ═══════════════════════════════════════════

TEST(RequestTest, HeaderLookupCaseInsensitive) {
    Request req;
    req.headers["content-type"] = "application/json";
    req.headers["x-custom-header"] = "test-value";

    EXPECT_EQ(req.header("Content-Type"), "application/json");
    EXPECT_EQ(req.header("content-type"), "application/json");
    EXPECT_EQ(req.get("X-Custom-Header"), "test-value");
    EXPECT_EQ(req.header("nonexistent"), "");
}

TEST(RequestTest, AcceptsCheck) {
    Request req;
    req.headers["accept"] = "application/json, text/html";

    EXPECT_TRUE(req.accepts("json"));
    EXPECT_TRUE(req.accepts("html"));
    EXPECT_FALSE(req.accepts("xml"));
}

TEST(RequestTest, ContentTypeCheck) {
    Request req;
    req.headers["content-type"] = "application/json; charset=utf-8";

    EXPECT_TRUE(req.is("json"));
    EXPECT_TRUE(req.is("application/json"));
    EXPECT_FALSE(req.is("text/html"));
}

// ═══════════════════════════════════════════
//  Response Tests
// ═══════════════════════════════════════════

TEST(ResponseTest, DefaultStatus200) {
    int sentStatus = 0;
    Response res([&](int s, const auto&, const auto&) { sentStatus = s; });

    res.send("OK");
    EXPECT_EQ(sentStatus, 200);
}

TEST(ResponseTest, StatusChaining) {
    int sentStatus = 0;
    Response res([&](int s, const auto&, const auto&) { sentStatus = s; });

    res.status(404).send("Not Found");
    EXPECT_EQ(sentStatus, 404);
}

TEST(ResponseTest, HeaderChaining) {
    std::unordered_map<std::string, std::string> sentHeaders;
    Response res([&](int, const auto& h, const auto&) { sentHeaders = h; });

    res.set("X-Custom", "value1")
       .header("X-Another", "value2")
       .send("test");

    EXPECT_EQ(sentHeaders["X-Custom"], "value1");
    EXPECT_EQ(sentHeaders["X-Another"], "value2");
}

TEST(ResponseTest, SendOnlyOnce) {
    int callCount = 0;
    Response res([&](int, const auto&, const auto&) { callCount++; });

    res.send("first");
    res.send("second"); // Should be ignored

    EXPECT_EQ(callCount, 1);
    EXPECT_TRUE(res.headersSent());
}

TEST(ResponseTest, RedirectSetsLocationHeader) {
    int sentStatus = 0;
    std::unordered_map<std::string, std::string> sentHeaders;
    Response res([&](int s, const auto& h, const auto&) {
        sentStatus = s;
        sentHeaders = h;
    });

    res.redirect("/new-location");

    EXPECT_EQ(sentStatus, 302);
    EXPECT_EQ(sentHeaders["Location"], "/new-location");
}

TEST(ResponseTest, SendStatusShorthand) {
    int sentStatus = 0;
    std::string sentBody;
    Response res([&](int s, const auto&, const std::string& b) {
        sentStatus = s;
        sentBody = b;
    });

    res.sendStatus(204);
    EXPECT_EQ(sentStatus, 204);
}

// ═══════════════════════════════════════════
//  Middleware Tests
// ═══════════════════════════════════════════

TEST(MiddlewareTest, BodyParserParsesJson) {
    Request req;
    req.headers["content-type"] = "application/json";
    req.rawBody = R"({"name": "Alice", "age": 30})";

    bool nextCalled = false;
    Response res; // Default constructor (no send callback)

    auto bp = middleware::bodyParser();
    bp(req, res, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_FALSE(req.body.isNull());

    std::string name = req.body["name"];
    EXPECT_EQ(name, "Alice");
    EXPECT_EQ(req.body["age"].get<int>(), 30);
}

TEST(MiddlewareTest, BodyParserRejectsInvalidJson) {
    Request req;
    req.headers["content-type"] = "application/json";
    req.rawBody = "{ invalid json }";

    int sentStatus = 0;
    Response res([&](int s, const auto&, const auto&) { sentStatus = s; });

    bool nextCalled = false;
    auto bp = middleware::bodyParser();
    bp(req, res, [&]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled); // Chain should stop
    EXPECT_EQ(sentStatus, 400);
}

TEST(MiddlewareTest, BodyParserSkipsNonJson) {
    Request req;
    req.headers["content-type"] = "text/plain";
    req.rawBody = "just plain text";

    Response res;

    bool nextCalled = false;
    auto bp = middleware::bodyParser();
    bp(req, res, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_TRUE(req.body.isObject()); // Default empty object
}

TEST(MiddlewareTest, BodyParserParsesFormData) {
    Request req;
    req.headers["content-type"] = "application/x-www-form-urlencoded";
    req.rawBody = "name=Alice&age=30&city=NYC";

    Response res;
    bool nextCalled = false;

    auto bp = middleware::bodyParser();
    bp(req, res, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    std::string name = req.body["name"];
    EXPECT_EQ(name, "Alice");
}

TEST(MiddlewareTest, CorsSetHeaders) {
    Request req;
    req.method = "GET";

    std::unordered_map<std::string, std::string> sentHeaders;
    Response res([&](int, const auto& h, const auto&) { sentHeaders = h; });

    bool nextCalled = false;
    auto corsMiddleware = middleware::cors();
    corsMiddleware(req, res, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(res.getHeaders().at("Access-Control-Allow-Origin"), "*");
}

TEST(MiddlewareTest, CorsHandlesPreflight) {
    Request req;
    req.method = "OPTIONS";

    int sentStatus = 0;
    Response res([&](int s, const auto&, const auto&) { sentStatus = s; });

    bool nextCalled = false;
    auto corsMiddleware = middleware::cors();
    corsMiddleware(req, res, [&]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled); // Should not continue for OPTIONS
    EXPECT_EQ(sentStatus, 204);
}

TEST(MiddlewareTest, HelmetSetSecurityHeaders) {
    Request req;
    Response res;

    bool nextCalled = false;
    auto helmetMiddleware = middleware::helmet();
    helmetMiddleware(req, res, [&]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(res.getHeaders().at("X-Content-Type-Options"), "nosniff");
    EXPECT_EQ(res.getHeaders().at("X-Frame-Options"), "DENY");
    EXPECT_EQ(res.getHeaders().at("X-XSS-Protection"), "1; mode=block");
}

// ═══════════════════════════════════════════
//  Server Routing Tests (using handleRequest)
// ═══════════════════════════════════════════

TEST(ServerTest, BasicRouting) {
    auto app = createServer();

    app.get("/hello", [](auto& req, auto& res) {
        res.json({{"message", "Hello, World!"}});
    });

    // Simulate a request
    Request req;
    req.method = "GET";
    req.path = "/hello";

    std::string sentBody;
    Response res([&](int, const auto&, const std::string& b) { sentBody = b; });

    app.handleRequest(req, res);

    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_EQ(parsed["message"], "Hello, World!");
}

TEST(ServerTest, RouteParameters) {
    auto app = createServer();

    app.get("/users/:id", [](auto& req, auto& res) {
        res.json({{"userId", req.params["id"]}});
    });

    Request req;
    req.method = "GET";
    req.path = "/users/42";

    std::string sentBody;
    Response res([&](int, const auto&, const std::string& b) { sentBody = b; });

    app.handleRequest(req, res);

    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_EQ(parsed["userId"], "42");
}

TEST(ServerTest, MultipleRouteParams) {
    auto app = createServer();

    app.get("/users/:userId/posts/:postId", [](auto& req, auto& res) {
        res.json({
            {"userId", req.params["userId"]},
            {"postId", req.params["postId"]}
        });
    });

    Request req;
    req.method = "GET";
    req.path = "/users/5/posts/99";

    std::string sentBody;
    Response res([&](int, const auto&, const std::string& b) { sentBody = b; });

    app.handleRequest(req, res);

    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_EQ(parsed["userId"], "5");
    EXPECT_EQ(parsed["postId"], "99");
}

TEST(ServerTest, MethodMatching) {
    auto app = createServer();

    app.get("/data", [](auto& req, auto& res) {
        res.json({{"method", "GET"}});
    });

    app.post("/data", [](auto& req, auto& res) {
        res.json({{"method", "POST"}});
    });

    // Test GET
    {
        Request req;
        req.method = "GET";
        req.path = "/data";
        std::string sentBody;
        Response res([&](int, const auto&, const std::string& b) { sentBody = b; });
        app.handleRequest(req, res);
        EXPECT_EQ(nlohmann::json::parse(sentBody)["method"], "GET");
    }

    // Test POST
    {
        Request req;
        req.method = "POST";
        req.path = "/data";
        std::string sentBody;
        Response res([&](int, const auto&, const std::string& b) { sentBody = b; });
        app.handleRequest(req, res);
        EXPECT_EQ(nlohmann::json::parse(sentBody)["method"], "POST");
    }
}

TEST(ServerTest, Returns404ForUnknownRoute) {
    auto app = createServer();

    Request req;
    req.method = "GET";
    req.path = "/nonexistent";

    int sentStatus = 0;
    Response res([&](int s, const auto&, const auto&) { sentStatus = s; });

    app.handleRequest(req, res);
    EXPECT_EQ(sentStatus, 404);
}

TEST(ServerTest, MiddlewareExecutionOrder) {
    auto app = createServer();
    std::vector<int> order;

    app.use([&](auto& req, auto& res, auto next) {
        order.push_back(1);
        next();
    });

    app.use([&](auto& req, auto& res, auto next) {
        order.push_back(2);
        next();
    });

    app.get("/test", [&](auto& req, auto& res) {
        order.push_back(3);
        res.send("done");
    });

    Request req;
    req.method = "GET";
    req.path = "/test";
    std::string sentBody;
    Response res([&](int, const auto&, const std::string& b) { sentBody = b; });

    app.handleRequest(req, res);

    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(ServerTest, MiddlewareCanShortCircuit) {
    auto app = createServer();

    app.use([](auto& req, auto& res, auto next) {
        // Auth check — reject everything
        res.status(401).json({{"error", "Unauthorized"}});
        // Do NOT call next()
    });

    app.get("/protected", [](auto& req, auto& res) {
        res.send("secret"); // Should never reach here
    });

    Request req;
    req.method = "GET";
    req.path = "/protected";
    int sentStatus = 0;
    std::string sentBody;
    Response res([&](int s, const auto&, const std::string& b) {
        sentStatus = s;
        sentBody = b;
    });

    app.handleRequest(req, res);

    EXPECT_EQ(sentStatus, 401);
    EXPECT_NE(sentBody.find("Unauthorized"), std::string::npos);
}

// ═══════════════════════════════════════════
//  End-to-End: bodyParser + Route
// ═══════════════════════════════════════════

struct UserPayload {
    std::string name;
    int age;
    NODE_SERIALIZE(UserPayload, name, age)
};

TEST(EndToEndTest, JsonBodyParsingAndResponse) {
    auto app = createServer();

    app.use(middleware::bodyParser());

    app.post("/users", [](auto& req, auto& res) {
        std::string name = req.body["name"];
        int age = req.body["age"];

        res.status(201).json({
            {"created", true},
            {"user", {{"name", name}, {"age", age}}}
        });
    });

    Request req;
    req.method = "POST";
    req.path = "/users";
    req.headers["content-type"] = "application/json";
    req.rawBody = R"({"name": "Eve", "age": 25})";

    int sentStatus = 0;
    std::string sentBody;
    Response res([&](int s, const auto&, const std::string& b) {
        sentStatus = s;
        sentBody = b;
    });

    app.handleRequest(req, res);

    EXPECT_EQ(sentStatus, 201);
    auto parsed = nlohmann::json::parse(sentBody);
    EXPECT_TRUE(parsed["created"]);
    EXPECT_EQ(parsed["user"]["name"], "Eve");
    EXPECT_EQ(parsed["user"]["age"], 25);
}
