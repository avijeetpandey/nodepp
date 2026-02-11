# Node++

**A high-performance C++ web framework with the simplicity of Node.js/Express.**

Node++ brings the developer experience of Node.js to C++20. Build web servers, REST APIs, GraphQL endpoints, and full-stack applications with an API that feels like Express — but runs at native speed.

```cpp
#include "nodepp/http.h"
using namespace nodepp;

int main() {
    auto app = http::createServer();
    app.get("/", [](auto& req, auto& res) {
        res.json({{"message", "Hello from Node++!"}});
    });
    app.listen(3000, []{ console::log("Server on :3000"); });
}
```

---

## Features

| Feature | Description |
|---|---|
| **Express-like API** | `app.get()`, `app.post()`, `app.use()`, `req.body`, `res.json()` |
| **Zero-Config JSON** | Auto-detect `application/json` and parse bodies. Send any C++ type as JSON. |
| **NODE_SERIALIZE** | One-line macro to make any struct JSON-serializable |
| **C++20 Concepts** | Type-safe auto-serialization using `JsonSerializable` concept |
| **Middleware Chain** | `app.use()` with `next()` callback, just like Express |
| **Route Parameters** | `/users/:id` with `req.params["id"]` |
| **Event Emitter** | Thread-safe Node.js-style event system |
| **GraphQL Engine** | Built-in schema, resolvers, and query execution |
| **File Serving** | `sendFile()`, `download()`, Range requests, MIME auto-detect |
| **Multipart Parser** | File upload support with size/type validation |
| **Crypto Module** | SHA-256, HMAC, AES, Base64, UUID, random bytes |
| **JWT Auth** | Sign, verify, decode tokens + auth middleware |
| **Sessions** | Session management with pluggable stores |
| **WebSocket** | Rooms, broadcasting, client management |
| **Server-Sent Events** | Real-time event streaming |
| **Compression** | Gzip compress/decompress with middleware |
| **TLS/HTTPS** | HTTPS configuration and redirect middleware |
| **HTTP Client** | `fetch::get()`, `fetch::post()` — like browser fetch API |
| **Validation** | Schema-based request validation with fluent API |
| **Template Engine** | Mustache-like templates with sections, loops, partials |
| **LRU Cache** | In-memory cache with TTL, ETag support, response caching |
| **Scheduler** | `setTimeout`, `setInterval`, cron expressions |
| **Observability** | Request ID, Prometheus metrics, health check, JSON logging |
| **Database** | SQLite driver, query builder, transactions |
| **Lifecycle** | Graceful shutdown, signal handling |
| **Testing** | TestClient (supertest-equivalent), mock factories |
| **OpenAPI** | Auto-generate Swagger/OpenAPI spec from routes |
| **Performance** | Arena allocator, object pools, zero-copy parsing |

---

## Quick Start

### Prerequisites

- C++20 compiler (GCC 11+, Clang 14+, MSVC 19.29+)
- CMake 3.20+
- Boost (headers only — Beast + Asio)
- nlohmann/json
- OpenSSL 3.x
- zlib
- SQLite3

### Build

```bash
# Install dependencies (macOS)
brew install cmake boost nlohmann-json openssl sqlite

# Build
cmake -B build
cmake --build build

# Run tests (20 test suites, 150+ test cases)
cd build && ctest --output-on-failure
```

### Using with FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(nodepp
    GIT_REPOSITORY https://github.com/avijeetpandey/nodepp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(nodepp)
target_link_libraries(your_app PRIVATE nodepp::nodepp)
```

---

## API Reference

### HTTP Server (Express-style)

```cpp
#include "nodepp/http.h"
using namespace nodepp;

auto app = http::createServer();

// Route handlers
app.get("/users",         handler);
app.post("/users",        handler);
app.put("/users/:id",     handler);
app.patch("/users/:id",   handler);
app.del("/users/:id",     handler);
app.options("/users",     handler);
app.all("/catchall",      handler);

// Middleware
app.use([](auto& req, auto& res, auto next) {
    console::log(req.method, req.path);
    next();
});

app.listen(3000, []{ console::log("Ready"); });
```

### Request & Response

```cpp
// Request
req.method              // "GET", "POST", etc.
req.path                // "/users/42"
req.params["id"]        // Route parameters
req.query["page"]       // Query parameters
req.body["name"]        // Auto-parsed JSON body
req.header("auth")      // Get header (case-insensitive)
req.cookies["sid"]      // Parsed cookies

// Response
res.status(201).json(data);
res.send("Hello");
res.redirect("/login");
res.set("X-Custom", "value");
res.type("text/html");
res.sendStatus(404);
```

### JSON Serialization

```cpp
#include "nodepp/json_utils.h"

struct User {
    std::string name;
    int age;
    NODE_SERIALIZE(User, name, age)
};

// Auto-serialize any type
res.json(User{"Alice", 30});
res.json(std::vector<int>{1, 2, 3});
res.json(std::map<std::string, int>{{"a", 1}});
res.json({{"key", "value"}, {"count", 42}});
```

### File Serving & Download

```cpp
#include "nodepp/sendfile.h"

app.get("/file", [](auto& req, auto& res) {
    sendfile::sendFile(req, res, "public/photo.jpg");
    // Auto MIME type, Range requests, Last-Modified
});

app.get("/dl", [](auto& req, auto& res) {
    sendfile::download(req, res, "data.csv", "report.csv");
    // Sets Content-Disposition: attachment
});
```

### Multipart File Upload

```cpp
#include "nodepp/multipart.h"

app.use(multipart::upload({
    .maxFileSize = 5 * 1024 * 1024,  // 5MB
    .maxFiles = 3,
    .allowedTypes = {"image/"}
}));

app.post("/upload", [](auto& req, auto& res) {
    auto files = req.body["_files"];
    res.json({{"uploaded", files.size()}});
});
```

### Crypto Module

```cpp
#include "nodepp/crypto.h"

crypto::sha256("hello");           // Hash
crypto::hmacSha256Hex(key, data);  // HMAC
crypto::base64Encode(data);        // Base64
crypto::base64UrlEncode(data);     // URL-safe Base64
crypto::randomHex(32);             // Random hex string
crypto::uuid();                    // UUID v4
crypto::timingSafeEqual(a, b);     // Timing-safe comparison
```

### JWT Authentication

```cpp
#include "nodepp/jwt.h"

// Sign
auto token = jwt::sign({{"userId", 123}}, "secret", {.expiresInSec = 3600});

// Verify
auto decoded = jwt::verify(token, "secret");
if (decoded.valid) {
    auto userId = decoded.payload["userId"];
}

// Middleware (protects routes)
app.use(jwt::auth("secret"));
// Decoded payload available in req.body["_user"]
```

### Session Management

```cpp
#include "nodepp/session.h"

app.use(session::session({
    .cookieName = "myapp.sid",
    .maxAge = 3600000,
    .httpOnly = true,
    .secure = true
}));
```

### WebSocket Server

```cpp
#include "nodepp/websocket.h"

ws::WebSocketServer wsServer;

wsServer.onConnection([](auto client) {
    console::log("Connected:", client->id());
});

wsServer.onMessage([&](auto client, const std::string& msg) {
    wsServer.room("chat").broadcast(msg, client->id());
});

wsServer.joinRoom(clientId, "chat");
wsServer.broadcast(nlohmann::json{{"type", "alert"}});
```

### Server-Sent Events

```cpp
#include "nodepp/sse.h"

app.get("/events", sse::createEndpoint([](auto& req, auto& writer) {
    writer.send("Hello", "greeting");
    writer.send("Update data", "update", "evt-1");
    writer.comment("keepalive");
}));
```

### Compression

```cpp
#include "nodepp/compress.h"

// Middleware
app.use(compress::compression({.threshold = 1024}));

// Manual
auto compressed = compress::gzipCompress(data);
auto original = compress::gzipDecompress(compressed);
```

### HTTP Client (Fetch)

```cpp
#include "nodepp/fetch.h"

auto resp = fetch::get("http://api.example.com/data");
if (resp.ok()) {
    auto data = resp.json();
}

auto resp2 = fetch::post("http://api.example.com/users",
    {{"name", "Alice"}, {"age", 30}});
```

### Request Validation

```cpp
#include "nodepp/validator.h"

validator::Schema userSchema;
userSchema.field("name").required().isString().minLength(2).maxLength(50);
userSchema.field("email").required().isString().email();
userSchema.field("age").required().isNumber().min(0).max(150);
userSchema.field("role").required().isString().oneOf({"admin", "user"});
userSchema.field("code").required().isString().custom([](const auto& val) {
    return val.get<std::string>().size() != 6
        ? std::optional<std::string>("Must be 6 chars") : std::nullopt;
});

// Use as middleware — auto-returns 400 on validation failure
app.use(validator::validate(userSchema));
```

### Template Engine

```cpp
#include "nodepp/template_engine.h"

auto html = tmpl::render(R"(
    <h1>Hello, {{name}}!</h1>
    {{#items}}<li>{{.}}</li>{{/items}}
    {{^items}}<p>No items</p>{{/items}}
)", {{"name", "Alice"}, {"items", {"A", "B", "C"}}});

// File-based templates with engine
tmpl::Engine engine;
engine.setViewsDir("views");
engine.registerPartial("header", "<h1>{{title}}</h1>");
auto page = engine.render("index", {{"title", "Home"}});
```

### LRU Cache

```cpp
#include "nodepp/cache.h"

// Standalone cache
cache::LRUCache<> myCache(1000, 60000); // 1000 entries, 60s TTL
myCache.set("key", "value");
auto val = myCache.get("key"); // std::optional<std::string>

// Response caching middleware
app.use(cache::responseCache({.ttlMs = 30000, .etag = true}));
```

### Scheduler

```cpp
#include "nodepp/scheduler.h"

// One-shot timer
auto timer = scheduler::setTimeout([]{ doSomething(); }, 5000);

// Recurring timer
auto interval = scheduler::setInterval([]{ ping(); }, 10000);

// Cancel
scheduler::clearTimeout(timer);
scheduler::clearInterval(interval);

// Cron job (every 5 minutes)
auto job = scheduler::cron("*/5 * * * *", []{ cleanup(); });
```

### Observability

```cpp
#include "nodepp/observability.h"

app.use(observability::requestId());   // X-Request-Id header
app.use(observability::metrics());     // Track request metrics
app.use(observability::jsonLogger());  // Structured JSON logs

// Prometheus-compatible metrics endpoint
app.get("/metrics", observability::metricsEndpoint());

// Health check
app.get("/health", observability::healthCheck({
    .healthy = true,
    .version = "1.0.0",
    .checks = {{"db", true}, {"cache", true}}
}));
```

### Database (SQLite)

```cpp
#include "nodepp/database.h"

db::Database database("app.db"); // or ":memory:"

database.execMulti(
    "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT);"
);

// Parameterized queries
database.exec("INSERT INTO users (name, email) VALUES (?, ?)",
    {"Alice", "alice@example.com"});

auto result = database.exec("SELECT * FROM users WHERE id = ?", {"1"});
auto json = result.toJson();

// Query builder
auto users = db::query(database)
    .table("users").select("name, email")
    .where("age > ?", "18")
    .orderBy("name").limit(10).run();

// Transactions
database.transaction([&]() {
    database.exec("UPDATE accounts SET balance = balance - 100 WHERE id = ?", {"1"});
    database.exec("UPDATE accounts SET balance = balance + 100 WHERE id = ?", {"2"});
    return true;
});
```

### Graceful Shutdown

```cpp
#include "nodepp/lifecycle.h"

lifecycle::enableGracefulShutdown(app);

lifecycle::onShutdown([](int signal) {
    console::log("Cleaning up...");
    database.close();
});
```

### Testing (Supertest-style)

```cpp
#include "nodepp/testing.h"

auto app = http::createServer();
app.get("/hello", handler);

testing::TestClient client(app);

// Fluent test API
auto result = client.get("/hello").expect(200);
auto body = result.json();

auto result2 = client.post("/users")
    .set("Authorization", "Bearer token")
    .send({{"name", "Alice"}})
    .exec();

// Mock factories
auto req = testing::createRequest("GET", "/test");
auto res = testing::createResponse();
```

### OpenAPI / Swagger

```cpp
#include "nodepp/openapi.h"

openapi::Document doc;
doc.title("My API").version("1.0.0")
   .server("http://localhost:3000");

doc.route({
    .method = "GET", .path = "/users",
    .summary = "List all users",
    .tags = {"Users"}
});
doc.route({
    .method = "POST", .path = "/users",
    .summary = "Create a user",
    .requestBodyType = "application/json",
    .requestSchema = {{"type","object"}, {"properties", {{"name",{{"type","string"}}}}}}
});

app.get("/api-docs", doc.serveSpec());
```

### Performance Utilities

```cpp
#include "nodepp/perf.h"

// Arena allocator (no-free bump allocator)
perf::Arena arena(4096);
auto* str = arena.allocString("hello");
auto* val = arena.create<int>(42);
arena.reset(); // Reclaim all at once

// Object pool
perf::ObjectPool<ExpensiveObject> pool(16);
auto obj = pool.acquire();
pool.release(std::move(obj));

// Zero-copy parsing
auto parts = perf::parse::split("a/b/c", '/');
auto kv = perf::parse::parseKeyValue("name=Alice");
int val;
perf::parse::parseInt("42", val);
```

### GraphQL

```cpp
#include "nodepp/graphql.h"

graphql::Schema schema;
schema.addQuery("user", {
    {"id", graphql::FieldType::Int}
}, [](const auto& args) {
    return nlohmann::json{{"id", 1}, {"name", "Alice"}};
});

app.post("/graphql", [&](auto& req, auto& res) {
    auto query = req.body["query"].get<std::string>();
    auto result = schema.execute(query);
    res.json(result);
});
```

---

## Test Suite

Node++ ships with 20 test suites covering every module:

| Test Suite | Tests | Module |
|---|---|---|
| `test_json` | JSON serialization, NODE_SERIALIZE, JsonValue | `json_utils.h` |
| `test_http` | Request/Response, routing, middleware, CORS | `http.h` |
| `test_graphql` | Schema, queries, mutations, variables | `graphql.h` |
| `test_sendfile` | File serving, MIME types, Range requests | `sendfile.h` |
| `test_multipart` | Form-data parsing, file uploads | `multipart.h` |
| `test_crypto` | SHA-256/512, MD5, HMAC, Base64, UUID | `crypto.h` |
| `test_jwt` | Sign, verify, decode, expiration | `jwt.h` |
| `test_session` | Memory store, TTL, cookie building | `session.h` |
| `test_websocket` | Clients, rooms, broadcasting | `websocket.h` |
| `test_sse` | Event serialization, writer | `sse.h` |
| `test_compress` | Gzip compress/decompress, levels | `compress.h` |
| `test_validator` | Required, types, email, range, custom | `validator.h` |
| `test_template` | Variables, sections, loops, escaping | `template_engine.h` |
| `test_cache` | LRU eviction, TTL, ETag generation | `cache.h` |
| `test_scheduler` | setTimeout, setInterval, cron parser | `scheduler.h` |
| `test_observability` | Metrics, request ID, health check | `observability.h` |
| `test_database` | CRUD, transactions, query builder | `database.h` |
| `test_testing` | TestClient, mock factories | `testing.h` |
| `test_openapi` | Spec generation, path params, schemas | `openapi.h` |
| `test_perf` | Arena, object pool, zero-copy parsing | `perf.h` |

Run all tests:
```bash
cd build && ctest --output-on-failure
```

---

## Examples

| Example | File | Description |
|---|---|---|
| Hello World | `examples/hello_world.cpp` | Minimal server in 5 lines |
| REST API | `examples/rest_api.cpp` | Full CRUD with middleware |
| GraphQL | `examples/graphql_server.cpp` | GraphQL queries & mutations |
| File Server | `examples/file_server.cpp` | File serving & downloads |
| JWT Auth | `examples/jwt_auth.cpp` | Login, token-protected routes |
| Validated API | `examples/validated_api.cpp` | Schema-based validation |
| WebSocket Chat | `examples/websocket_chat.cpp` | Rooms & broadcasting |
| Database API | `examples/database_api.cpp` | SQLite CRUD with query builder |
| Observable Server | `examples/observable_server.cpp` | Metrics, health, logging |

---

## Project Structure

```
nodepp/
├── include/nodepp/
│   ├── http.h              # Express-style server, Request, Response
│   ├── json_utils.h        # JSON serialization, JsonValue, NODE_SERIALIZE
│   ├── events.h            # EventEmitter
│   ├── console.h           # Colored logging
│   ├── graphql.h           # GraphQL engine
│   ├── sendfile.h          # File serving & downloads
│   ├── multipart.h         # Multipart form-data parser
│   ├── crypto.h            # Hashing, HMAC, Base64, UUID
│   ├── jwt.h               # JSON Web Tokens
│   ├── session.h           # Session management
│   ├── websocket.h         # WebSocket server
│   ├── sse.h               # Server-Sent Events
│   ├── compress.h          # Gzip compression
│   ├── tls.h               # TLS/HTTPS configuration
│   ├── fetch.h             # HTTP client
│   ├── validator.h         # Request validation
│   ├── template_engine.h   # Mustache-like templates
│   ├── cache.h             # LRU cache & response caching
│   ├── scheduler.h         # Timers & cron
│   ├── observability.h     # Metrics, health, logging
│   ├── database.h          # SQLite driver & query builder
│   ├── lifecycle.h         # Graceful shutdown
│   ├── testing.h           # TestClient & mocks
│   ├── openapi.h           # OpenAPI spec generation
│   └── perf.h              # Memory pools & fast parsing
├── src/
│   ├── http.cpp            # Boost.Beast HTTP server
│   ├── graphql.cpp         # GraphQL implementation
│   └── database.cpp        # SQLite implementation
├── tests/                  # 20 test suites (GTest)
├── examples/               # 9 example programs
├── CMakeLists.txt
├── vcpkg.json
└── README.md
```

---

## Dependencies

| Library | Purpose | Required |
|---|---|---|
| Boost (Beast + Asio) | HTTP server & networking | Yes |
| nlohmann/json | JSON serialization | Yes |
| OpenSSL 3.x | Crypto, HMAC, JWT | Yes |
| zlib | Gzip compression | Yes |
| SQLite3 | Database module | Yes |
| Google Test | Unit testing | Dev only |

---

## License

MIT
