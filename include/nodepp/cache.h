#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/cache.h — LRU cache, response caching middleware, ETag
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include <unordered_map>
#include <list>
#include <mutex>
#include <chrono>
#include <string>
#include <functional>
#include <optional>

namespace nodepp::cache {

// ═══════════════════════════════════════════
//  LRU Cache — O(1) get/set with TTL
// ═══════════════════════════════════════════
template <typename Key = std::string, typename Value = std::string>
class LRUCache {
public:
    explicit LRUCache(std::size_t maxSize = 1000, int defaultTtlMs = 0)
        : maxSize_(maxSize), defaultTtlMs_(defaultTtlMs) {}

    void set(const Key& key, const Value& value, int ttlMs = -1) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.erase(it->second);
            map_.erase(it);
        }
        auto expiry = (ttlMs > 0 || (ttlMs < 0 && defaultTtlMs_ > 0))
            ? std::chrono::steady_clock::now() +
              std::chrono::milliseconds(ttlMs > 0 ? ttlMs : defaultTtlMs_)
            : std::chrono::steady_clock::time_point::max();

        list_.push_front({key, value, expiry});
        map_[key] = list_.begin();
        evict();
    }

    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;

        // Check TTL
        if (it->second->expiry != std::chrono::steady_clock::time_point::max() &&
            std::chrono::steady_clock::now() > it->second->expiry) {
            list_.erase(it->second);
            map_.erase(it);
            return std::nullopt;
        }

        // Move to front (most recently used)
        list_.splice(list_.begin(), list_, it->second);
        return it->second->value;
    }

    bool has(const Key& key) {
        return get(key).has_value();
    }

    void del(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.erase(it->second);
            map_.erase(it);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.clear();
        map_.clear();
    }

    std::size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return map_.size();
    }

private:
    struct Entry {
        Key key;
        Value value;
        std::chrono::steady_clock::time_point expiry;
    };

    std::size_t maxSize_;
    int defaultTtlMs_;
    std::list<Entry> list_;
    std::unordered_map<Key, typename std::list<Entry>::iterator> map_;
    std::mutex mutex_;

    void evict() {
        while (map_.size() > maxSize_) {
            auto last = list_.end();
            --last;
            map_.erase(last->key);
            list_.pop_back();
        }
    }
};

// ═══════════════════════════════════════════
//  ETag generation (simple hash-based)
// ═══════════════════════════════════════════
inline std::string generateETag(const std::string& body) {
    // FNV-1a hash
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : body) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\"%llx\"", static_cast<unsigned long long>(hash));
    return buf;
}

// ═══════════════════════════════════════════
//  Response caching middleware
// ═══════════════════════════════════════════
struct CacheOptions {
    int ttlMs = 60000;           // 1 minute default
    std::size_t maxEntries = 500;
    bool etag = true;            // Generate ETags
};

inline http::MiddlewareFunction responseCache(CacheOptions options = {}) {
    auto store = std::make_shared<LRUCache<std::string, std::string>>(
        options.maxEntries, options.ttlMs);

    return [store, options](http::Request& req, http::Response& res, http::NextFunction next) {
        if (req.method != "GET") { next(); return; }

        auto key = req.method + ":" + req.url;
        auto cached = store->get(key);

        if (cached.has_value()) {
            auto parsed = nlohmann::json::parse(*cached);
            int status = parsed.value("status", 200);
            std::string body = parsed.value("body", "");
            std::string contentType = parsed.value("contentType", "");

            // ETag check
            if (options.etag) {
                auto etag = generateETag(body);
                auto ifNoneMatch = req.header("if-none-match");
                if (!ifNoneMatch.empty() && ifNoneMatch == etag) {
                    res.status(304).end();
                    return;
                }
                res.set("ETag", etag);
            }

            res.set("X-Cache", "HIT");
            if (!contentType.empty()) res.set("Content-Type", contentType);
            res.status(status).send(body);
            return;
        }

        res.set("X-Cache", "MISS");
        next();

        // After handler, cache the response
        if (res.headersSent() && res.getStatusCode() >= 200 && res.getStatusCode() < 300) {
            auto headers = res.getHeaders();
            nlohmann::json entry = {
                {"status", res.getStatusCode()},
                {"body", res.getBody()},
                {"contentType", headers.count("Content-Type") ? headers.at("Content-Type") : ""}
            };
            store->set(key, entry.dump());

            if (options.etag) {
                res.set("ETag", generateETag(res.getBody()));
            }
        }
    };
}

} // namespace nodepp::cache
