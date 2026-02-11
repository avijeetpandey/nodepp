// ═══════════════════════════════════════════════════════════════════
//  hello_world.cpp — Your first Node++ server in 5 lines
// ═══════════════════════════════════════════════════════════════════

#include "nodepp/nodepp.h"
using namespace nodepp;

int main() {
    auto app = http::createServer();

    app.get("/", [](auto& req, auto& res) {
        res.json({{"message", "Hello, World!"}, {"framework", "Node++"}});
    });

    app.listen(3000, [] {
        console::log("Server running on http://localhost:3000");
    });
}
