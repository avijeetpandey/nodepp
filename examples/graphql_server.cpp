// ═══════════════════════════════════════════════════════════════════
//  graphql_server.cpp — GraphQL API with Node++
// ═══════════════════════════════════════════════════════════════════
//
//  This example demonstrates:
//    • Defining a GraphQL schema with resolvers
//    • Query arguments and field selection
//    • Mutation support
//    • Integration with the HTTP server
//
// ═══════════════════════════════════════════════════════════════════

#include "nodepp/nodepp.h"
#include <vector>

using namespace nodepp;

struct User {
    std::string name;
    int id;
    std::string email;
    NODE_SERIALIZE(User, name, id, email)
};

static std::vector<User> users = {
    {"Alice", 1, "alice@example.com"},
    {"Bob",   2, "bob@example.com"},
};
static int nextId = 3;

int main() {
    auto app = http::createServer();

    // ── Middleware ──
    app.use(middleware::bodyParser());
    app.use(middleware::cors());

    // ── Define GraphQL Schema ──
    auto schema = std::make_shared<graphql::Schema>();

    // Query: users — List all users
    schema->query("users", [](JsonValue args, JsonValue ctx) -> JsonValue {
        nlohmann::json result = nlohmann::json::array();
        for (auto& user : users) {
            result.push_back(nlohmann::json(user));
        }
        return JsonValue(result);
    });

    // Query: user(id) — Get a specific user
    schema->query("user", [](JsonValue args, JsonValue ctx) -> JsonValue {
        int id = args["id"];
        for (auto& user : users) {
            if (user.id == id) {
                return JsonValue(nlohmann::json(user));
            }
        }
        throw std::runtime_error("User not found with id " + std::to_string(id));
    });

    // Mutation: createUser(name, email) — Create a user
    schema->mutation("createUser", [](JsonValue args, JsonValue ctx) -> JsonValue {
        std::string name  = args["name"];
        std::string email = args.get<std::string>("email", "");

        User newUser{name, nextId++, email};
        users.push_back(newUser);

        return JsonValue(nlohmann::json(newUser));
    });

    // ── Mount GraphQL endpoint ──
    app.post("/graphql", graphql::createHandler(schema));
    app.get("/graphql", graphql::createHandler(schema));

    // ── Health check ──
    app.get("/", [](auto& req, auto& res) {
        res.json({
            {"service", "GraphQL API"},
            {"endpoint", "/graphql"},
            {"status", "running"}
        });
    });

    app.listen(4000, [] {
        console::log("GraphQL server running on http://localhost:4000/graphql");
        console::info("Try:");
        console::info("  curl -X POST http://localhost:4000/graphql \\");
        console::info("    -H 'Content-Type: application/json' \\");
        console::info("    -d '{\"query\": \"{ users { name email } }\"}'");
    });
}
