// ═══════════════════════════════════════════════════════════════════
//  websocket_chat.cpp — WebSocket rooms & broadcasting example
// ═══════════════════════════════════════════════════════════════════

#include <nodepp/http.h>
#include <nodepp/websocket.h>
#include <nodepp/crypto.h>
#include <nodepp/console.h>

using namespace nodepp;

int main() {
    auto app = http::createServer();
    ws::WebSocketServer wsServer;

    // Configure WebSocket handlers
    wsServer.onConnection([&](auto client) {
        console::log("Client connected:", client->id());
        wsServer.joinRoom(client->id(), "general");
        wsServer.room("general").broadcast(
            nlohmann::json{{"type", "join"}, {"user", client->id()}},
            client->id()
        );
    });

    wsServer.onMessage([&](auto client, const std::string& msg) {
        console::log("Message from", client->id(), ":", msg);
        // Broadcast to general room, excluding sender
        wsServer.room("general").broadcast(
            nlohmann::json{{"type", "message"}, {"from", client->id()}, {"text", msg}},
            client->id()
        );
    });

    wsServer.onDisconnect([&](auto client) {
        console::log("Client disconnected:", client->id());
    });

    // REST endpoint to see connected clients
    app.get("/clients", [&](http::Request&, http::Response& res) {
        res.json({
            {"count", wsServer.clientCount()},
            {"rooms", wsServer.roomCount()}
        });
    });

    // Simulate WebSocket connections for demo
    app.post("/ws/connect", [&](http::Request&, http::Response& res) {
        auto id = crypto::uuid().substr(0, 8);
        wsServer.addClient(id);
        res.json({{"clientId", id}});
    });

    app.post("/ws/send", [&](http::Request& req, http::Response& res) {
        auto clientId = req.body["clientId"].get<std::string>();
        auto message = req.body["message"].get<std::string>();
        wsServer.handleMessage(clientId, message);
        res.json({{"sent", true}});
    });

    app.listen(3000, [] {
        console::log("WebSocket Chat on http://localhost:3000");
    });
}
