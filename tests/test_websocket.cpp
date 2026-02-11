// ═══════════════════════════════════════════════════════════════════
//  test_websocket.cpp — Tests for WebSocket rooms & broadcasting
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/websocket.h>

using namespace nodepp::ws;

TEST(WebSocketClientTest, CreateClient) {
    Client client("client-1");
    EXPECT_EQ(client.id(), "client-1");
    EXPECT_TRUE(client.isConnected());
}

TEST(WebSocketClientTest, SendMessage) {
    std::string received;
    Client client("c1", [&received](const std::string& msg) { received = msg; });
    client.send(std::string("Hello"));
    EXPECT_EQ(received, "Hello");
}

TEST(WebSocketClientTest, SendJson) {
    std::string received;
    Client client("c1", [&received](const std::string& msg) { received = msg; });
    client.send(nlohmann::json{{"type", "greeting"}});
    EXPECT_EQ(received, "{\"type\":\"greeting\"}");
}

TEST(WebSocketClientTest, CloseClient) {
    bool closed = false;
    Client client("c1", nullptr, [&closed]() { closed = true; });
    client.close();
    EXPECT_TRUE(closed);
    EXPECT_FALSE(client.isConnected());
}

TEST(WebSocketRoomTest, JoinAndLeave) {
    Room room("chat");
    auto c1 = std::make_shared<Client>("c1");
    auto c2 = std::make_shared<Client>("c2");

    room.join(c1);
    room.join(c2);
    EXPECT_EQ(room.size(), 2u);

    room.leave("c1");
    EXPECT_EQ(room.size(), 1u);
}

TEST(WebSocketRoomTest, BroadcastToAll) {
    Room room("chat");
    std::string msg1, msg2;
    auto c1 = std::make_shared<Client>("c1", [&msg1](const std::string& m) { msg1 = m; });
    auto c2 = std::make_shared<Client>("c2", [&msg2](const std::string& m) { msg2 = m; });

    room.join(c1);
    room.join(c2);
    room.broadcast(std::string("Hello all"));

    EXPECT_EQ(msg1, "Hello all");
    EXPECT_EQ(msg2, "Hello all");
}

TEST(WebSocketRoomTest, BroadcastExcludesSender) {
    Room room("chat");
    std::string msg1, msg2;
    auto c1 = std::make_shared<Client>("c1", [&msg1](const std::string& m) { msg1 = m; });
    auto c2 = std::make_shared<Client>("c2", [&msg2](const std::string& m) { msg2 = m; });

    room.join(c1);
    room.join(c2);
    room.broadcast(std::string("Hello"), std::string("c1")); // Exclude c1

    EXPECT_TRUE(msg1.empty());
    EXPECT_EQ(msg2, "Hello");
}

TEST(WebSocketServerTest, AddAndRemoveClients) {
    WebSocketServer server;
    server.addClient("c1");
    server.addClient("c2");
    EXPECT_EQ(server.clientCount(), 2u);

    server.removeClient("c1");
    EXPECT_EQ(server.clientCount(), 1u);
}

TEST(WebSocketServerTest, ConnectionHandlers) {
    WebSocketServer server;
    std::string connectedId;
    std::string disconnectedId;

    server.onConnection([&](auto client) { connectedId = client->id(); });
    server.onDisconnect([&](auto client) { disconnectedId = client->id(); });

    server.addClient("c1");
    EXPECT_EQ(connectedId, "c1");

    server.removeClient("c1");
    EXPECT_EQ(disconnectedId, "c1");
}

TEST(WebSocketServerTest, MessageHandler) {
    WebSocketServer server;
    std::string receivedMsg;
    std::string senderId;

    server.onMessage([&](auto client, const std::string& msg) {
        senderId = client->id();
        receivedMsg = msg;
    });

    server.addClient("c1");
    server.handleMessage("c1", "Hello server");

    EXPECT_EQ(senderId, "c1");
    EXPECT_EQ(receivedMsg, "Hello server");
}

TEST(WebSocketServerTest, RoomManagement) {
    WebSocketServer server;
    auto c1 = server.addClient("c1");
    auto c2 = server.addClient("c2");

    server.joinRoom("c1", "room1");
    server.joinRoom("c2", "room1");

    EXPECT_EQ(server.room("room1").size(), 2u);

    server.leaveRoom("c1", "room1");
    EXPECT_EQ(server.room("room1").size(), 1u);
}

TEST(WebSocketServerTest, ServerBroadcast) {
    WebSocketServer server;
    std::string msg1, msg2;
    server.addClient("c1", [&msg1](const std::string& m) { msg1 = m; });
    server.addClient("c2", [&msg2](const std::string& m) { msg2 = m; });

    server.broadcast(std::string("Global message"));

    EXPECT_EQ(msg1, "Global message");
    EXPECT_EQ(msg2, "Global message");
}
