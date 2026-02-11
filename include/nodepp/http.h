#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/http.h — Express-style HTTP Server, Request, and Response
// ═══════════════════════════════════════════════════════════════════
//
//  Usage:
//    auto app = http::createServer();
//    app.get("/hello", [](auto& req, auto& res) {
//        res.json({{"message", "Hello, World!"}});
//    });
//    app.listen(3000, []{ console::log("Listening on :3000"); });
//
// ═══════════════════════════════════════════════════════════════════

#include "json_utils.h"
#include "events.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <algorithm>

namespace nodepp::http {

// ── Forward declarations ──
class Request;
class Response;
class Server;

// ── Type aliases ──
using NextFunction       = std::function<void()>;
using MiddlewareFunction = std::function<void(Request&, Response&, NextFunction)>;
using RouteHandler       = std::function<void(Request&, Response&)>;

// ═══════════════════════════════════════════════════════════════════
//  class Request
//  Represents an incoming HTTP request.
//  req.body is auto-populated by bodyParser middleware.
// ═══════════════════════════════════════════════════════════════════
class Request {
public:
    // ── Core properties ──
    std::string method;
    std::string url;            // Full URL including query string
    std::string path;           // URL path without query string
    std::string rawBody;        // Raw request body
    std::string ip;             // Client IP address
    std::string protocol;       // "http" or "https"
    std::string hostname;       // Host header value

    // ── Parsed data ──
    std::unordered_map<std::string, std::string> headers;   // All headers (lowercase keys)
    std::unordered_map<std::string, std::string> params;    // Route parameters (:id -> params["id"])
    std::unordered_map<std::string, std::string> query;     // Query string parameters
    std::unordered_map<std::string, std::string> cookies;   // Parsed cookies

    // ── Auto-parsed JSON body (populated by bodyParser) ──
    JsonValue body;

    // ── Get a header value (case-insensitive) ──
    std::string header(const std::string& name) const {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        auto it = headers.find(lower);
        return it != headers.end() ? it->second : "";
    }

    // ── Alias for header() (Express compatibility) ──
    std::string get(const std::string& name) const {
        return header(name);
    }

    // ── Check if request accepts a content type ──
    bool accepts(const std::string& type) const {
        auto accept = header("accept");
        return accept.find(type) != std::string::npos || accept.find("*/*") != std::string::npos;
    }

    // ── Check Content-Type ──
    bool is(const std::string& type) const {
        auto ct = header("content-type");
        return ct.find(type) != std::string::npos;
    }
};

// ═══════════════════════════════════════════════════════════════════
//  class Response
//  Represents the HTTP response to send back.
//  Uses a SendCallback to decouple from the transport layer.
// ═══════════════════════════════════════════════════════════════════
class Response {
public:
    using SendCallback = std::function<void(
        int statusCode,
        const std::unordered_map<std::string, std::string>& headers,
        const std::string& body
    )>;

    explicit Response(SendCallback cb)
        : sendCallback_(std::move(cb)) {}

    // Default constructor for testing
    Response() : sendCallback_(nullptr) {}

    // ── Set status code (chainable) ──
    Response& status(int code) {
        statusCode_ = code;
        return *this;
    }

    // ── Set a response header (chainable) ──
    Response& set(const std::string& key, const std::string& value) {
        headers_[key] = value;
        return *this;
    }

    // ── Alias for set() (Express compatibility) ──
    Response& header(const std::string& key, const std::string& value) {
        return set(key, value);
    }

    // ── Set Content-Type (chainable) ──
    Response& type(const std::string& contentType) {
        return set("Content-Type", contentType);
    }

    // ── Send a string body ──
    void send(const std::string& body) {
        if (sent_) return;
        sent_ = true;
        if (headers_.find("Content-Type") == headers_.end()) {
            headers_["Content-Type"] = "text/plain; charset=utf-8";
        }
        if (sendCallback_) {
            sendCallback_(statusCode_, headers_, body);
        }
        body_ = body;
    }

    void send(const char* body) {
        send(std::string(body));
    }

    // ── Send any JSON-serializable type ──
    //    Works with: std::map, std::vector, structs with NODE_SERIALIZE,
    //    nlohmann::json, initializer lists, primitives, etc.
    template <typename T>
    void json(const T& data) {
        nlohmann::json j;
        if constexpr (std::is_same_v<std::decay_t<T>, nlohmann::json>) {
            j = data;
        } else if constexpr (std::is_same_v<std::decay_t<T>, JsonValue>) {
            j = data.raw();
        } else {
            j = nlohmann::json(data);
        }
        set("Content-Type", "application/json; charset=utf-8");
        send(j.dump());
    }

    // ── Send JSON with initializer list ──
    //    Supports: res.json({{"key", "value"}, {"count", 5}})
    void json(nlohmann::json::initializer_list_t init) {
        nlohmann::json j(init);
        set("Content-Type", "application/json; charset=utf-8");
        send(j.dump());
    }

    // ── Generic send: auto-detect JSON-serializable types ──
    template <typename T>
        requires JsonSerializable<T> && (!std::is_convertible_v<T, std::string>)
    void send(const T& data) {
        json(data);
    }

    // ── Send with status code shorthand ──
    void sendStatus(int code) {
        status(code);
        send(std::to_string(code));
    }

    // ── Redirect ──
    void redirect(const std::string& url) {
        redirect(302, url);
    }

    void redirect(int code, const std::string& url) {
        status(code);
        set("Location", url);
        send("");
    }

    // ── End without body ──
    void end() {
        if (!sent_) {
            send("");
        }
    }

    // ── Check if response was already sent ──
    bool headersSent() const { return sent_; }

    // ── Access the sent body (for testing) ──
    const std::string& getBody() const { return body_; }
    int getStatusCode() const { return statusCode_; }
    const std::unordered_map<std::string, std::string>& getHeaders() const { return headers_; }

private:
    int statusCode_ = 200;
    std::unordered_map<std::string, std::string> headers_;
    bool sent_ = false;
    SendCallback sendCallback_;
    std::string body_;
};

// ═══════════════════════════════════════════════════════════════════
//  class Server
//  Express-style HTTP server with routing and middleware.
//  Uses pimpl to hide Boost.Beast implementation details.
// ═══════════════════════════════════════════════════════════════════
class Server : public EventEmitter {
public:
    Server();
    ~Server();
    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;

    // Non-copyable
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // ── Middleware Registration ──
    Server& use(MiddlewareFunction middleware);

    // ── Route Registration ──
    //    Accepts any callable: lambdas, function pointers, std::function.
    //    `auto` parameters in lambdas are supported.

    template <typename Handler>
    Server& get(const std::string& path, Handler&& handler) {
        addRoute("GET", path, wrapHandler(std::forward<Handler>(handler)));
        return *this;
    }

    template <typename Handler>
    Server& post(const std::string& path, Handler&& handler) {
        addRoute("POST", path, wrapHandler(std::forward<Handler>(handler)));
        return *this;
    }

    template <typename Handler>
    Server& put(const std::string& path, Handler&& handler) {
        addRoute("PUT", path, wrapHandler(std::forward<Handler>(handler)));
        return *this;
    }

    template <typename Handler>
    Server& patch(const std::string& path, Handler&& handler) {
        addRoute("PATCH", path, wrapHandler(std::forward<Handler>(handler)));
        return *this;
    }

    template <typename Handler>
    Server& del(const std::string& path, Handler&& handler) {
        addRoute("DELETE", path, wrapHandler(std::forward<Handler>(handler)));
        return *this;
    }

    template <typename Handler>
    Server& options(const std::string& path, Handler&& handler) {
        addRoute("OPTIONS", path, wrapHandler(std::forward<Handler>(handler)));
        return *this;
    }

    template <typename Handler>
    Server& all(const std::string& path, Handler&& handler) {
        addRoute("*", path, wrapHandler(std::forward<Handler>(handler)));
        return *this;
    }

    // ── Start Listening ──
    void listen(int port, std::function<void()> callback = nullptr);
    void listen(const std::string& host, int port, std::function<void()> callback = nullptr);

    // ── Stop the server ──
    void close();

    // ── Process a request (used internally and for testing) ──
    void handleRequest(Request& req, Response& res);

private:
    // Allow internal networking classes (defined in http.cpp) to access Impl
    friend class HttpSession;
    friend class HttpListener;

    struct Impl;
    std::unique_ptr<Impl> impl_;

    void addRoute(const std::string& method, const std::string& pattern, RouteHandler handler);

    // Wrap any callable into a type-erased RouteHandler
    template <typename Handler>
    static RouteHandler wrapHandler(Handler&& handler) {
        return [h = std::forward<Handler>(handler)](Request& req, Response& res) mutable {
            h(req, res);
        };
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Factory: http::createServer()
//  Mimics Node.js `http.createServer()`
// ═══════════════════════════════════════════════════════════════════
inline Server createServer() {
    return Server();
}

} // namespace nodepp::http
