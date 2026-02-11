#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/session.h — Session management with pluggable stores
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include "json_utils.h"
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <memory>

namespace nodepp::session {

// ── Session Store interface ──
class Store {
public:
    virtual ~Store() = default;
    virtual JsonValue get(const std::string& sid) = 0;
    virtual void set(const std::string& sid, const JsonValue& data) = 0;
    virtual void destroy(const std::string& sid) = 0;
    virtual void touch(const std::string& sid) = 0;
};

// ── In-memory store with TTL ──
class MemoryStore : public Store {
public:
    explicit MemoryStore(int ttlMs = 3600000) : ttlMs_(ttlMs) {}

    JsonValue get(const std::string& sid) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(sid);
        if (it == store_.end()) return JsonValue(nlohmann::json(nullptr));
        if (isExpired(it->second)) {
            store_.erase(it);
            return JsonValue(nlohmann::json(nullptr));
        }
        return it->second.data;
    }

    void set(const std::string& sid, const JsonValue& data) override {
        std::lock_guard<std::mutex> lock(mutex_);
        store_[sid] = {data, std::chrono::steady_clock::now()};
    }

    void destroy(const std::string& sid) override {
        std::lock_guard<std::mutex> lock(mutex_);
        store_.erase(sid);
    }

    void touch(const std::string& sid) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = store_.find(sid);
        if (it != store_.end()) {
            it->second.lastAccess = std::chrono::steady_clock::now();
        }
    }

    std::size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return store_.size();
    }

private:
    struct Entry {
        JsonValue data;
        std::chrono::steady_clock::time_point lastAccess;
    };

    bool isExpired(const Entry& e) const {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - e.lastAccess).count();
        return elapsed > ttlMs_;
    }

    int ttlMs_;
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> store_;
};

// ── Session options ──
struct SessionOptions {
    std::string cookieName = "nodepp.sid";
    int maxAge = 3600000;       // 1 hour
    bool httpOnly = true;
    bool secure = false;
    std::string sameSite = "Lax";
    std::string path = "/";
    std::shared_ptr<Store> store = nullptr; // null → auto-create MemoryStore
};

namespace detail {
inline std::string generateSid() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << dist(rng) << dist(rng);
    return oss.str();
}

inline std::string buildSetCookie(const std::string& name, const std::string& value,
                                   const SessionOptions& opts) {
    std::string cookie = name + "=" + value + "; Path=" + opts.path;
    if (opts.maxAge > 0) cookie += "; Max-Age=" + std::to_string(opts.maxAge / 1000);
    if (opts.httpOnly) cookie += "; HttpOnly";
    if (opts.secure) cookie += "; Secure";
    if (!opts.sameSite.empty()) cookie += "; SameSite=" + opts.sameSite;
    return cookie;
}
} // namespace detail

// ── Session middleware ──
// Populates req.headers["x-session-id"] and provides session data
// through a shared session store.
inline http::MiddlewareFunction session(SessionOptions opts = {}) {
    if (!opts.store) {
        opts.store = std::make_shared<MemoryStore>(opts.maxAge);
    }
    auto store = opts.store;

    return [opts, store](http::Request& req, http::Response& res, http::NextFunction next) {
        // Extract session ID from cookies
        std::string sid;
        auto cookieIt = req.cookies.find(opts.cookieName);
        if (cookieIt != req.cookies.end()) {
            sid = cookieIt->second;
        }

        bool isNew = false;
        if (sid.empty()) {
            sid = detail::generateSid();
            isNew = true;
        }

        // Load session data into req
        auto sessionData = store->get(sid);
        if (sessionData.isNull() || (sessionData.isObject() && sessionData.size() == 0)) {
            if (!isNew) {
                // Session expired, generate new
                sid = detail::generateSid();
                isNew = true;
            }
            sessionData = JsonValue(nlohmann::json::object());
        }

        // Store session ID for handlers to use
        req.headers["x-session-id"] = sid;

        // Set cookie
        res.set("Set-Cookie", detail::buildSetCookie(opts.cookieName, sid, opts));

        store->touch(sid);
        next();

        // After handler, save session data back
        // (Handlers can modify the store directly via the shared store)
    };
}

} // namespace nodepp::session
