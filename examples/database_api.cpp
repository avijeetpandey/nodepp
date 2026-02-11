// ═══════════════════════════════════════════════════════════════════
//  database_api.cpp — SQLite database example with REST API
// ═══════════════════════════════════════════════════════════════════

#include <nodepp/http.h>
#include <nodepp/database.h>
#include <nodepp/console.h>

using namespace nodepp;

int main() {
    auto app = http::createServer();

    // Create in-memory database with a users table
    db::Database database(":memory:");
    database.execMulti(
        "CREATE TABLE users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  email TEXT UNIQUE NOT NULL,"
        "  age INTEGER"
        ");"
        "INSERT INTO users (name, email, age) VALUES ('Alice', 'alice@example.com', 30);"
        "INSERT INTO users (name, email, age) VALUES ('Bob', 'bob@example.com', 25);"
    );

    // GET /users — list all users
    app.get("/users", [&](http::Request&, http::Response& res) {
        auto result = db::query(database).table("users").select().run();
        res.json(result.toJson());
    });

    // GET /users/:id — get user by ID
    app.get("/users/:id", [&](http::Request& req, http::Response& res) {
        auto result = database.exec("SELECT * FROM users WHERE id = ?", {req.params["id"]});
        if (result.empty()) {
            res.status(404).json({{"error", "User not found"}});
        } else {
            auto& row = result.first();
            res.json({
                {"id", std::stoi(row.at("id"))},
                {"name", row.at("name")},
                {"email", row.at("email")},
                {"age", std::stoi(row.at("age"))}
            });
        }
    });

    // POST /users — create a user
    app.post("/users", [&](http::Request& req, http::Response& res) {
        auto name = req.body["name"].get<std::string>();
        auto email = req.body["email"].get<std::string>();
        auto age = std::to_string(req.body["age"].get<int>());

        auto result = database.exec(
            "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
            {name, email, age}
        );
        res.status(201).json({
            {"id", result.lastInsertId},
            {"message", "User created"}
        });
    });

    app.listen(3000, [] {
        console::log("Database API on http://localhost:3000");
        console::log("  GET  /users      — list users");
        console::log("  GET  /users/:id  — get user");
        console::log("  POST /users      — create user");
    });
}
