#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/sse.h — Server-Sent Events support
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include <string>
#include <sstream>
#include <functional>

namespace nodepp::sse {

// ── SSE Event ──
struct Event {
    std::string data;
    std::string event;    // optional event name
    std::string id;       // optional event id
    int retry = -1;       // optional retry interval in ms

    std::string serialize() const {
        std::ostringstream oss;
        if (!id.empty())    oss << "id: " << id << "\n";
        if (!event.empty()) oss << "event: " << event << "\n";
        if (retry >= 0)     oss << "retry: " << retry << "\n";
        // Split data by newlines for multi-line data
        std::istringstream dataStream(data);
        std::string line;
        while (std::getline(dataStream, line)) {
            oss << "data: " << line << "\n";
        }
        oss << "\n"; // Blank line terminates the event
        return oss.str();
    }
};

// ── SSE Writer — wraps a response for streaming events ──
class Writer {
public:
    using FlushCallback = std::function<void(const std::string&)>;

    explicit Writer(FlushCallback flush) : flush_(std::move(flush)) {}

    void send(const std::string& data,
              const std::string& event = "",
              const std::string& id = "") {
        Event evt{data, event, id};
        if (flush_) flush_(evt.serialize());
    }

    void send(const Event& evt) {
        if (flush_) flush_(evt.serialize());
    }

    void comment(const std::string& text) {
        if (flush_) flush_(": " + text + "\n\n");
    }

    void close() { closed_ = true; }
    bool isClosed() const { return closed_; }

private:
    FlushCallback flush_;
    bool closed_ = false;
};

// ── Initialize SSE on a response ──
// Returns the serialized SSE header prefix. The caller's transport layer
// should switch to streaming mode.
inline std::string initHeaders() {
    return ""; // Headers are set on res, this returns empty body prefix
}

// ── Create a middleware-compatible SSE endpoint ──
inline http::RouteHandler createEndpoint(
    std::function<void(http::Request&, Writer&)> handler) {
    return [handler](http::Request& req, http::Response& res) {
        res.set("Content-Type", "text/event-stream");
        res.set("Cache-Control", "no-cache");
        res.set("Connection", "keep-alive");
        res.set("X-Accel-Buffering", "no");

        // For non-streaming transport, collect all events and send at once
        std::ostringstream buffer;
        Writer writer([&buffer](const std::string& chunk) {
            buffer << chunk;
        });

        handler(req, writer);
        res.send(buffer.str());
    };
}

} // namespace nodepp::sse
