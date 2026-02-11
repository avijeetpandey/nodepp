// ═══════════════════════════════════════════════════════════════════
//  jwt_auth.cpp — JWT authentication example
// ═══════════════════════════════════════════════════════════════════

#include <nodepp/http.h>
#include <nodepp/jwt.h>
#include <nodepp/crypto.h>
#include <nodepp/console.h>

using namespace nodepp;

const std::string JWT_SECRET = "your-secret-key-here";

int main() {
    auto app = http::createServer();

    // Login endpoint — returns a JWT
    app.post("/login", [](http::Request& req, http::Response& res) {
        auto username = req.body["username"].get<std::string>();
        auto password = req.body["password"].get<std::string>();

        // In real app, verify credentials against database
        if (username == "admin" && password == "secret") {
            auto token = jwt::sign(
                {{"userId", 1}, {"username", username}, {"role", "admin"}},
                JWT_SECRET,
                {.expiresInSec = 3600}
            );
            res.json({{"token", token}});
        } else {
            res.status(401).json({{"error", "Invalid credentials"}});
        }
    });

    // Protected endpoint — requires valid JWT
    app.get("/profile", [](http::Request& req, http::Response& res) {
        // Manual token verification
        auto authHeader = req.header("authorization");
        if (authHeader.substr(0, 7) != "Bearer ") {
            res.status(401).json({{"error", "No token"}});
            return;
        }
        auto decoded = jwt::verify(authHeader.substr(7), JWT_SECRET);
        if (!decoded.valid) {
            res.status(401).json({{"error", decoded.error}});
            return;
        }
        res.json({
            {"message", "Welcome back!"},
            {"user", decoded.payload["username"]}
        });
    });

    // UUID example
    app.get("/uuid", [](http::Request&, http::Response& res) {
        res.json({{"uuid", crypto::uuid()}});
    });

    app.listen(3000, [] {
        console::log("JWT Auth server on http://localhost:3000");
        console::log("  POST /login   — get a JWT token");
        console::log("  GET /profile  — protected route");
        console::log("  GET /uuid     — generate a UUID");
    });
}
