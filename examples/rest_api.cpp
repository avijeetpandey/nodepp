// ═══════════════════════════════════════════════════════════════════
//  rest_api.cpp — Full REST API with middleware, auto-JSON, structs
// ═══════════════════════════════════════════════════════════════════
//
//  This example demonstrates:
//    • bodyParser() auto-parsing JSON request bodies
//    • CORS, Rate Limiting, and Security headers
//    • Route parameters (:id)
//    • Automatic serialization of C++ structs to JSON
//    • res.json() accepting std::map, std::vector, and custom structs
//
// ═══════════════════════════════════════════════════════════════════

#include "nodepp/nodepp.h"
#include <vector>
#include <map>

using namespace nodepp;

// ── Define your data models ──
struct User {
    std::string name;
    int id;
    std::string email;
    NODE_SERIALIZE(User, name, id, email)
};

// ── In-memory "database" ──
static std::vector<User> users = {
    {"Alice", 1, "alice@example.com"},
    {"Bob",   2, "bob@example.com"},
    {"Eve",   3, "eve@example.com"},
};
static int nextId = 4;

int main() {
    auto app = http::createServer();

    // ── Register Middleware ──
    app.use(middleware::requestLogger());                     // Log all requests
    app.use(middleware::helmet());                            // Security headers
    app.use(middleware::cors());                              // CORS support
    app.use(middleware::rateLimiter({.windowMs = 60000, .max = 100}));  // Rate limiting
    app.use(middleware::bodyParser());                        // Auto-parse JSON bodies

    // ── GET /users — List all users ──
    app.get("/users", [](auto& req, auto& res) {
        res.json(users);  // Automatically serializes vector<User> → JSON array
    });

    // ── GET /users/:id — Get a specific user ──
    app.get("/users/:id", [](auto& req, auto& res) {
        int id = std::stoi(req.params["id"]);

        for (auto& user : users) {
            if (user.id == id) {
                res.json(user);  // Automatically serializes User → JSON object
                return;
            }
        }

        res.status(404).json({{"error", "User not found"}});
    });

    // ── POST /users — Create a new user ──
    //    req.body is ALREADY parsed — zero manual JSON work!
    app.post("/users", [](auto& req, auto& res) {
        std::string name  = req.body["name"];
        std::string email = req.body["email"];

        User newUser{name, nextId++, email};
        users.push_back(newUser);

        res.status(201).json(newUser);
    });

    // ── PUT /users/:id — Update a user ──
    app.put("/users/:id", [](auto& req, auto& res) {
        int id = std::stoi(req.params["id"]);

        for (auto& user : users) {
            if (user.id == id) {
                if (req.body.has("name"))  user.name  = std::string(req.body["name"]);
                if (req.body.has("email")) user.email = std::string(req.body["email"]);
                res.json(user);
                return;
            }
        }

        res.status(404).json({{"error", "User not found"}});
    });

    // ── DELETE /users/:id — Delete a user ──
    app.del("/users/:id", [](auto& req, auto& res) {
        int id = std::stoi(req.params["id"]);
        auto it = std::remove_if(users.begin(), users.end(),
            [id](const User& u) { return u.id == id; });

        if (it != users.end()) {
            users.erase(it, users.end());
            res.json({{"deleted", true}});
        } else {
            res.status(404).json({{"error", "User not found"}});
        }
    });

    // ── GET /stats — Return aggregate data (demonstrates std::map) ──
    app.get("/stats", [](auto& req, auto& res) {
        std::map<std::string, int> stats = {
            {"totalUsers", static_cast<int>(users.size())},
            {"nextId", nextId}
        };
        res.json(stats);  // std::map → JSON object automatically
    });

    // ── Start the server ──
    app.listen(3000, [] {
        console::log("REST API running on http://localhost:3000");
        console::info("Try: curl http://localhost:3000/users");
        console::info("Try: curl -X POST -H 'Content-Type: application/json' "
                       "-d '{\"name\":\"Dave\",\"email\":\"dave@test.com\"}' "
                       "http://localhost:3000/users");
    });
}
