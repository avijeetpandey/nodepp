#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/websocket.h — WebSocket server with rooms & broadcasting
// ═══════════════════════════════════════════════════════════════════
//
//  This provides the WebSocket room/broadcast logic and data types.
//  The actual Boost.Beast WebSocket transport is in ws.cpp.
// ═══════════════════════════════════════════════════════════════════

#include "events.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <atomic>

namespace nodepp::ws {

// ── WebSocket client handle ──
class Client {
public:
    using SendFunction = std::function<void(const std::string&)>;
    using CloseFunction = std::function<void()>;

    explicit Client(const std::string& id, SendFunction sendFn = nullptr,
                    CloseFunction closeFn = nullptr)
        : id_(id), sendFn_(std::move(sendFn)), closeFn_(std::move(closeFn)) {}

    const std::string& id() const { return id_; }

    void send(const std::string& message) {
        if (sendFn_ && connected_) sendFn_(message);
    }

    void send(const nlohmann::json& j) {
        send(j.dump());
    }

    void close() {
        connected_ = false;
        if (closeFn_) closeFn_();
    }

    bool isConnected() const { return connected_; }
    void disconnect() { connected_ = false; }

    // Properties map for user data
    std::unordered_map<std::string, std::string> data;

private:
    std::string id_;
    SendFunction sendFn_;
    CloseFunction closeFn_;
    bool connected_ = true;
};

// ── Room — a group of clients ──
class Room {
public:
    explicit Room(const std::string& name) : name_(name) {}

    const std::string& name() const { return name_; }

    void join(std::shared_ptr<Client> client) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[client->id()] = client;
    }

    void leave(const std::string& clientId) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(clientId);
    }

    void broadcast(const std::string& message, const std::string& excludeId = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, client] : clients_) {
            if (id != excludeId && client->isConnected()) {
                client->send(message);
            }
        }
    }

    void broadcast(const nlohmann::json& j, const std::string& excludeId = "") {
        broadcast(j.dump(), excludeId);
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return clients_.size();
    }

    std::vector<std::string> clientIds() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> ids;
        for (auto& [id, _] : clients_) ids.push_back(id);
        return ids;
    }

private:
    std::string name_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Client>> clients_;
};

// ═══════════════════════════════════════════
//  WebSocket Server
// ═══════════════════════════════════════════
class WebSocketServer : public EventEmitter {
public:
    using MessageHandler = std::function<void(std::shared_ptr<Client>, const std::string&)>;
    using ConnectionHandler = std::function<void(std::shared_ptr<Client>)>;

    void onConnection(ConnectionHandler handler) { onConnect_ = std::move(handler); }
    void onMessage(MessageHandler handler) { onMessage_ = std::move(handler); }
    void onDisconnect(ConnectionHandler handler) { onDisconnect_ = std::move(handler); }

    // ── Client management ──
    std::shared_ptr<Client> addClient(const std::string& id,
                                       Client::SendFunction sendFn = nullptr,
                                       Client::CloseFunction closeFn = nullptr) {
        auto client = std::make_shared<Client>(id, std::move(sendFn), std::move(closeFn));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            clients_[id] = client;
        }
        if (onConnect_) onConnect_(client);
        return client;
    }

    void removeClient(const std::string& id) {
        std::shared_ptr<Client> client;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = clients_.find(id);
            if (it == clients_.end()) return;
            client = it->second;
            client->disconnect();
            clients_.erase(it);
        }
        // Remove from all rooms
        {
            std::lock_guard<std::mutex> lock(roomMutex_);
            for (auto& [_, room] : rooms_) {
                room->leave(id);
            }
        }
        if (onDisconnect_) onDisconnect_(client);
    }

    void handleMessage(const std::string& clientId, const std::string& message) {
        std::shared_ptr<Client> client;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = clients_.find(clientId);
            if (it == clients_.end()) return;
            client = it->second;
        }
        if (onMessage_) onMessage_(client, message);
    }

    // ── Room management ──
    Room& room(const std::string& name) {
        std::lock_guard<std::mutex> lock(roomMutex_);
        auto it = rooms_.find(name);
        if (it == rooms_.end()) {
            rooms_[name] = std::make_unique<Room>(name);
        }
        return *rooms_[name];
    }

    void joinRoom(const std::string& clientId, const std::string& roomName) {
        std::shared_ptr<Client> client;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = clients_.find(clientId);
            if (it == clients_.end()) return;
            client = it->second;
        }
        room(roomName).join(client);
    }

    void leaveRoom(const std::string& clientId, const std::string& roomName) {
        std::lock_guard<std::mutex> lock(roomMutex_);
        auto it = rooms_.find(roomName);
        if (it != rooms_.end()) it->second->leave(clientId);
    }

    // ── Broadcasting ──
    void broadcast(const std::string& message, const std::string& excludeId = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, client] : clients_) {
            if (id != excludeId && client->isConnected()) {
                client->send(message);
            }
        }
    }

    void broadcast(const nlohmann::json& j, const std::string& excludeId = "") {
        broadcast(j.dump(), excludeId);
    }

    // ── Stats ──
    std::size_t clientCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return clients_.size();
    }

    std::size_t roomCount() const {
        std::lock_guard<std::mutex> lock(roomMutex_);
        return rooms_.size();
    }

private:
    mutable std::mutex mutex_;
    mutable std::mutex roomMutex_;
    std::unordered_map<std::string, std::shared_ptr<Client>> clients_;
    std::unordered_map<std::string, std::unique_ptr<Room>> rooms_;
    ConnectionHandler onConnect_;
    MessageHandler onMessage_;
    ConnectionHandler onDisconnect_;
};

} // namespace nodepp::ws
