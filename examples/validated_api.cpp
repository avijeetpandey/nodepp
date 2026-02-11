// ═══════════════════════════════════════════════════════════════════
//  validated_api.cpp — Request validation example
// ═══════════════════════════════════════════════════════════════════

#include <nodepp/http.h>
#include <nodepp/validator.h>
#include <nodepp/console.h>

using namespace nodepp;

int main() {
    auto app = http::createServer();

    // Define validation schemas
    validator::Schema userSchema;
    userSchema.field("name").required().isString().minLength(2).maxLength(50);
    userSchema.field("email").required().isString().email();
    userSchema.field("age").required().isNumber().min(0).max(150);
    userSchema.field("role").required().isString().oneOf({"admin", "user", "guest"});
    userSchema.field("password").required().isString().minLength(8);

    // POST /users — validated endpoint
    app.use(validator::validate(userSchema));
    app.post("/users", [](http::Request& req, http::Response& res) {
        // If we reach here, validation passed
        res.status(201).json({
            {"message", "User created"},
            {"user", req.body.raw()}
        });
    });

    app.listen(3000, [] {
        console::log("Validated API on http://localhost:3000");
        console::log("  POST /users — create user with validation");
    });
}
