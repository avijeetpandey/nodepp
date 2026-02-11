#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/lifecycle.h — Graceful shutdown, signal handling
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include "console.h"
#include <csignal>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>

namespace nodepp::lifecycle {

namespace detail {
    inline std::mutex& handlersMutex() {
        static std::mutex m;
        return m;
    }
    inline std::vector<std::function<void(int)>>& shutdownHandlers() {
        static std::vector<std::function<void(int)>> handlers;
        return handlers;
    }
    inline std::atomic<bool>& shuttingDown() {
        static std::atomic<bool> v{false};
        return v;
    }
    inline void signalHandler(int sig) {
        if (shuttingDown().exchange(true)) return; // Already shutting down
        console::info("Received signal", sig, "— shutting down gracefully...");
        std::lock_guard<std::mutex> lock(handlersMutex());
        for (auto& handler : shutdownHandlers()) {
            handler(sig);
        }
    }
} // namespace detail

// ── Register a shutdown handler ──
inline void onShutdown(std::function<void(int)> handler) {
    std::lock_guard<std::mutex> lock(detail::handlersMutex());
    detail::shutdownHandlers().push_back(std::move(handler));
}

// ── Enable graceful shutdown on SIGINT and SIGTERM ──
inline void enableGracefulShutdown(http::Server& server) {
    // Register signal handlers
    std::signal(SIGINT, detail::signalHandler);
    std::signal(SIGTERM, detail::signalHandler);

    // Register server close as a shutdown handler
    onShutdown([&server](int) {
        console::info("Stopping HTTP server...");
        server.close();
        console::success("Server stopped.");
    });
}

// ── Check if shutdown is in progress ──
inline bool isShuttingDown() {
    return detail::shuttingDown().load();
}

} // namespace nodepp::lifecycle
