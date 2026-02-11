# Node++ 

**A high-performance C++ web framework with the simplicity of Node.js/Express.**

Node++ brings the developer experience of Node.js to C++20. Build web servers, REST APIs, and GraphQL endpoints with an API that feels like Express — but runs at native speed.

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
| **Built-in Security** | CORS, Rate Limiting, Helmet (security headers), Cookie parsing |
| **GraphQL** | Built-in GraphQL server with schema, resolvers, and query parsing |
| **Node.js Modules** | `console`, `fs`, `path`, `events` — familiar APIs |
| **Boost.Beast** | Production-grade HTTP on top of Boost.Asio |

---

## Quick Start

### Prerequisites

- **C++20** compiler (GCC 12+, Clang 15+, MSVC 2022+)
- **CMake** 3.20+
- **vcpkg** (recommended) or system-installed Boost + nlohmann-json

### Option 1: Clone & Build

```bash
git clone https://github.com/nodepp/nodepp.git
cd nodepp

# Using vcpkg (recommended)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build

# Run the example
./build/examples/hello_world
```

### Option 2: CMake FetchContent

Add to your project's `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    nodepp
    GIT_REPOSITORY https://github.com/nodepp/nodepp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(nodepp)

target_link_libraries(your_app PRIVATE nodepp::nodepp)
```

### Option 3: vcpkg Manifest

Add to your `vcpkg.json`:

```json
{
  "dependencies": ["nodepp"]
}
```

---

## API Reference

### HTTP Server

```cpp
#include "nodepp/http.h"
using namespace nodepp;

auto app = http::createServer();

// Route handlers
app.get("/path", [](auto& req, auto& res) { /* ... */ });
app.post("/path", handler);
app.put("/path", handler);
app.patch("/path", handler);
app.del("/path", handler);
app.options("/path", handler);
app.all("/path", handler);  // Match any method

// Route parameters
app.get("/users/:id", [](auto& req, auto& res) {
    std::string id = req.params["id"];
    res.json({{"id", id}});
});

// Start server
app.listen(3000, []{ console::log("Ready!"); });
app.listen("0.0.0.0", 8080);  // Specify host
```

### Request Object

```cpp
req.method     // "GET", "POST", etc.
req.path       // "/users/42"
req.url        // "/users/42?sort=name"
req.params     // Route params: { "id": "42" }
req.query      // Query string: { "sort": "name" }
req.headers    // All headers (lowercase keys)
req.body       // Auto-parsed JSON body (JsonValue)
req.rawBody    // Raw body string
req.ip         // Client IP
req.cookies    // Parsed cookies (with cookieParser middleware)

req.header("Content-Type")  // Get header (case-insensitive)
req.get("Authorization")    // Alias for header()
req.is("json")              // Check Content-Type
req.accepts("html")         // Check Accept header
```

### Response Object

```cpp
res.status(201)                         // Set status (chainable)
res.set("X-Custom", "value")           // Set header (chainable)
res.header("X-Custom", "value")        // Alias for set()
res.type("application/json")           // Set Content-Type

res.send("Hello")                      // Send string
res.json({{"key", "value"}})           // Send JSON (initializer list)
res.json(myStruct)                     // Send any serializable type
res.json(myVector)                     // Send std::vector as JSON array
res.json(myMap)                        // Send std::map as JSON object

res.sendStatus(204)                    // Send status code only
res.redirect("/new-url")               // 302 redirect
res.redirect(301, "/permanent")        // Custom redirect code
res.end()                              // End response
```

### Auto-Serialization

Any C++ type that `nlohmann::json` understands is automatically serializable:

```cpp
// Standard containers — just work
std::vector<int> nums = {1, 2, 3};
res.json(nums);  // → [1, 2, 3]

std::map<std::string, double> data = {{"pi", 3.14}};
res.json(data);  // → {"pi": 3.14}

// Custom structs — add NODE_SERIALIZE
struct User {
    std::string name;
    int id;
    NODE_SERIALIZE(User, name, id)
};

User u{"Alice", 1};
res.json(u);  // → {"name": "Alice", "id": 1}

// Nested structures — also just work
std::vector<User> users = {{"Alice", 1}, {"Bob", 2}};
res.json(users);  // → [{"name":"Alice","id":1}, {"name":"Bob","id":2}]
```

### Middleware

```cpp
#include "nodepp/middleware.h"

// Body Parser — auto-parse JSON request bodies
app.use(middleware::bodyParser());

// CORS
app.use(middleware::cors());
app.use(middleware::cors({.origin = "https://myapp.com", .credentials = true}));

// Rate Limiting
app.use(middleware::rateLimiter({.windowMs = 60000, .max = 100}));

// Security Headers (Helmet)
app.use(middleware::helmet());

// Request Logging
app.use(middleware::requestLogger());

// Static Files
app.use(middleware::staticFiles("./public"));

// Cookie Parser
app.use(middleware::cookieParser());

// Custom Middleware
app.use([](auto& req, auto& res, auto next) {
    // Authentication check
    if (req.header("Authorization").empty()) {
        res.status(401).json({{"error", "Unauthorized"}});
        return;  // Don't call next() to stop the chain
    }
    next();  // Continue to next middleware / route handler
});
```

### GraphQL

```cpp
#include "nodepp/graphql.h"

auto schema = std::make_shared<graphql::Schema>();

// Define queries
schema->query("user", [](JsonValue args, JsonValue ctx) -> JsonValue {
    int id = args["id"];
    return JsonValue({{"name", "Alice"}, {"id", id}});
});

// Define mutations
schema->mutation("createUser", [](JsonValue args, JsonValue ctx) -> JsonValue {
    std::string name = args["name"];
    return JsonValue({{"name", name}, {"id", 42}});
});

// Mount on server
app.post("/graphql", graphql::createHandler(schema));

// Clients can query:
// { user(id: 1) { name } }
// mutation { createUser(name: "Bob") { id name } }
```

### Console

```cpp
#include "nodepp/console.h"

console::log("Server started on port", 3000);     // [12:00:00.000] Server started on port 3000
console::info("Request received");                  // [12:00:00.001] i Request received
console::warn("Deprecated endpoint");               // [12:00:00.002] ⚠ Deprecated endpoint  
console::error("Connection failed");                // [12:00:00.003] ✖ Connection failed
console::success("User created");                   // [12:00:00.004] ✔ User created
console::debug("Variable x =", 42);                 // [12:00:00.005] ● Variable x = 42

console::time("db-query");
// ... do work ...
console::timeEnd("db-query");                       // db-query: 12.5ms
```

### File System

```cpp
#include "nodepp/fs.h"

// Synchronous
std::string content = fs::readFileSync("config.json");
fs::writeFileSync("output.txt", "Hello");
fs::appendFileSync("log.txt", "New entry\n");
bool exists = fs::existsSync("file.txt");
fs::mkdirSync("data", true);  // recursive
auto files = fs::readdirSync(".");
auto stats = fs::statSync("file.txt");

// Asynchronous (callback style)
fs::readFile("large.txt", [](std::error_code ec, std::string data) {
    if (!ec) console::log("Read", data.size(), "bytes");
});
```

### Path

```cpp
#include "nodepp/path.h"

path::join("src", "components", "App.tsx")  // "src/components/App.tsx"
path::resolve("./file.txt")                 // "/absolute/path/to/file.txt"
path::basename("/foo/bar/baz.html")         // "baz.html"
path::dirname("/foo/bar/baz.html")          // "/foo/bar"
path::extname("index.html")                 // ".html"
path::normalize("/foo/bar/../baz")          // "/foo/baz"
path::isAbsolute("/usr/local")              // true
```

### EventEmitter

```cpp
#include "nodepp/events.h"

EventEmitter emitter;

emitter.on("data", [](const std::vector<std::any>& args) {
    auto value = std::any_cast<std::string>(args[0]);
    console::log("Received:", value);
});

emitter.emit("data", std::string("hello"));

// One-time listener
emitter.once("ready", [](const auto&) {
    console::log("System ready!");
});
```

---

## Project Structure

```
nodepp/
├── CMakeLists.txt              # Build configuration
├── vcpkg.json                  # Dependency manifest
├── include/nodepp/
│   ├── nodepp.h                # Umbrella header (include everything)
│   ├── http.h                  # Server, Request, Response
│   ├── middleware.h            # bodyParser, cors, rateLimiter, helmet
│   ├── graphql.h               # GraphQL schema, parser, handler
│   ├── json_utils.h            # JsonValue, NODE_SERIALIZE, concepts
│   ├── console.h               # Colorful logging
│   ├── events.h                # EventEmitter
│   ├── fs.h                    # File system operations
│   ├── path.h                  # Path utilities
│   └── security.h              # Security config types
├── src/
│   ├── http.cpp                # Boost.Beast server implementation
│   └── graphql.cpp             # GraphQL compilation unit
├── tests/
│   ├── test_json.cpp           # Serialization tests
│   ├── test_http.cpp           # Routing & middleware tests
│   └── test_graphql.cpp        # GraphQL tests
└── examples/
    ├── hello_world.cpp         # Minimal server
    ├── rest_api.cpp            # Full CRUD API
    └── graphql_server.cpp      # GraphQL endpoint
```

---

## Running Tests

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
cd build && ctest --output-on-failure
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.
