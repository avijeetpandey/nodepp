#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/events.h — Node.js-style EventEmitter
// ═══════════════════════════════════════════════════════════════════

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <algorithm>
#include <mutex>
#include <memory>

namespace nodepp {

class EventEmitter {
public:
    using EventListener = std::function<void(const std::vector<std::any>&)>;

    EventEmitter() : mutex_(std::make_unique<std::mutex>()) {}
    virtual ~EventEmitter() = default;

    // Movable (unique_ptr<mutex> enables this)
    EventEmitter(EventEmitter&&) noexcept = default;
    EventEmitter& operator=(EventEmitter&&) noexcept = default;

    // Non-copyable
    EventEmitter(const EventEmitter&) = delete;
    EventEmitter& operator=(const EventEmitter&) = delete;

    // ── Register a persistent listener ──
    EventEmitter& on(const std::string& event, EventListener listener) {
        std::lock_guard<std::mutex> lock(*mutex_);
        listeners_[event].push_back({std::move(listener), false});
        return *this;
    }

    // ── Register a one-time listener ──
    EventEmitter& once(const std::string& event, EventListener listener) {
        std::lock_guard<std::mutex> lock(*mutex_);
        listeners_[event].push_back({std::move(listener), true});
        return *this;
    }

    // ── Emit an event with arguments ──
    template <typename... Args>
    void emit(const std::string& event, Args&&... args) {
        std::vector<Entry> toCall;
        {
            std::lock_guard<std::mutex> lock(*mutex_);
            auto it = listeners_.find(event);
            if (it == listeners_.end()) return;

            toCall = it->second;

            // Remove once-listeners
            auto& entries = it->second;
            entries.erase(
                std::remove_if(entries.begin(), entries.end(),
                    [](const Entry& e) { return e.once; }),
                entries.end()
            );
        }

        std::vector<std::any> packed = {std::any(std::forward<Args>(args))...};
        for (auto& entry : toCall) {
            entry.fn(packed);
        }
    }

    // ── Remove all listeners for an event ──
    void removeAllListeners(const std::string& event) {
        std::lock_guard<std::mutex> lock(*mutex_);
        listeners_.erase(event);
    }

    // ── Remove all listeners ──
    void removeAllListeners() {
        std::lock_guard<std::mutex> lock(*mutex_);
        listeners_.clear();
    }

    // ── Get listener count ──
    std::size_t listenerCount(const std::string& event) const {
        std::lock_guard<std::mutex> lock(*mutex_);
        auto it = listeners_.find(event);
        if (it == listeners_.end()) return 0;
        return it->second.size();
    }

    // ── Convenience: typed single-arg listener ──
    template <typename T>
    EventEmitter& on(const std::string& event, std::function<void(const T&)> listener) {
        return on(event, EventListener([listener = std::move(listener)](const std::vector<std::any>& args) {
            if (!args.empty()) {
                listener(std::any_cast<const T&>(args[0]));
            }
        }));
    }

    // ── Convenience: no-arg listener ──
    EventEmitter& on(const std::string& event, std::function<void()> listener) {
        return on(event, EventListener([listener = std::move(listener)](const std::vector<std::any>&) {
            listener();
        }));
    }

private:
    struct Entry {
        EventListener fn;
        bool once = false;
    };

    std::unique_ptr<std::mutex> mutex_;
    std::unordered_map<std::string, std::vector<Entry>> listeners_;
};

} // namespace nodepp
