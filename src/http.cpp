// ═══════════════════════════════════════════════════════════════════
//  src/http.cpp — Boost.Beast-powered HTTP server implementation
// ═══════════════════════════════════════════════════════════════════

#include "nodepp/http.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>

#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <memory>
#include <thread>

namespace nodepp::http {

namespace beast  = boost::beast;
namespace net    = boost::asio;
namespace bhttp  = beast::http;
using tcp        = net::ip::tcp;

// ═══════════════════════════════════════════
//  Internal: Compiled route with regex
// ═══════════════════════════════════════════
struct CompiledRoute {
    std::string method;
    std::string pattern;
    std::regex  regex;
    std::vector<std::string> paramNames;
    RouteHandler handler;
};

// ═══════════════════════════════════════════
//  Internal: URL parsing utilities
// ═══════════════════════════════════════════
namespace detail {

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

inline std::string urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int hex = 0;
            std::istringstream iss(str.substr(i + 1, 2));
            if (iss >> std::hex >> hex) {
                result += static_cast<char>(hex);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

inline std::pair<std::string, std::string> splitUrl(const std::string& url) {
    auto pos = url.find('?');
    if (pos == std::string::npos) return {url, ""};
    return {url.substr(0, pos), url.substr(pos + 1)};
}

inline std::unordered_map<std::string, std::string> parseQueryString(const std::string& qs) {
    std::unordered_map<std::string, std::string> result;
    if (qs.empty()) return result;

    std::istringstream stream(qs);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            result[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
        } else {
            result[urlDecode(pair)] = "";
        }
    }
    return result;
}

inline CompiledRoute compileRoute(const std::string& method,
                                   const std::string& pattern,
                                   RouteHandler handler) {
    CompiledRoute route;
    route.method  = method;
    route.pattern = pattern;
    route.handler = std::move(handler);

    std::string regexStr;
    std::size_t pos = 0;

    while (pos < pattern.size()) {
        char c = pattern[pos];
        if (c == ':') {
            // Route parameter — :paramName
            pos++;
            std::string paramName;
            while (pos < pattern.size() && pattern[pos] != '/') {
                paramName += pattern[pos++];
            }
            route.paramNames.push_back(paramName);
            regexStr += "([^/]+)";
        } else if (c == '*') {
            // Wildcard
            regexStr += "(.*)";
            route.paramNames.push_back("*");
            pos++;
        } else {
            // Escape regex special characters
            if (c == '.' || c == '(' || c == ')' ||
                c == '[' || c == ']' || c == '{' ||
                c == '}' || c == '+' || c == '?' ||
                c == '^' || c == '$' || c == '|') {
                regexStr += '\\';
            }
            regexStr += c;
            pos++;
        }
    }

    route.regex = std::regex("^" + regexStr + "$");
    return route;
}

inline bool matchRoute(const CompiledRoute& route,
                        const std::string& method,
                        const std::string& path,
                        Request& req) {
    // Method match: exact match, or wildcard "*"
    if (route.method != method && route.method != "*") return false;

    std::smatch match;
    if (std::regex_match(path, match, route.regex)) {
        for (std::size_t i = 0; i < route.paramNames.size(); ++i) {
            req.params[route.paramNames[i]] = match[i + 1].str();
        }
        return true;
    }
    return false;
}

} // namespace detail

// ═══════════════════════════════════════════
//  Server::Impl — Hidden implementation
// ═══════════════════════════════════════════
struct Server::Impl {
    std::vector<MiddlewareFunction>  middlewares;
    std::vector<CompiledRoute>       routes;
    std::unique_ptr<net::io_context> ioc;
    bool running = false;

    // ── Middleware chain executor ──
    void executeMiddlewareChain(Request& req, Response& res,
                                std::size_t index,
                                std::function<void()> done) {
        if (res.headersSent()) return;
        if (index >= middlewares.size()) {
            done();
            return;
        }

        auto& mw = middlewares[index];
        mw(req, res, [this, &req, &res, index, done = std::move(done)]() {
            executeMiddlewareChain(req, res, index + 1, std::move(done));
        });
    }

    // ── Request handler: middleware chain → route matching ──
    void handleRequest(Request& req, Response& res) {
        executeMiddlewareChain(req, res, 0, [this, &req, &res]() {
            if (res.headersSent()) return;

            for (auto& route : routes) {
                // Reset params for each route attempt
                auto savedParams = req.params;
                req.params.clear();

                if (detail::matchRoute(route, req.method, req.path, req)) {
                    route.handler(req, res);
                    return;
                }

                req.params = std::move(savedParams);
            }

            // No route matched → 404
            res.status(404).json(nlohmann::json{
                {"error", "Not Found"},
                {"message", "Cannot " + req.method + " " + req.path}
            });
        });
    }
};

// ═══════════════════════════════════════════
//  Session — Handles a single HTTP connection
// ═══════════════════════════════════════════
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket socket, Server::Impl& server)
        : socket_(std::move(socket))
        , server_(server)
    {}

    void run() {
        readRequest();
    }

private:
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    bhttp::request<bhttp::string_body> beastRequest_;
    Server::Impl& server_;

    void readRequest() {
        auto self = shared_from_this();
        bhttp::async_read(
            socket_, buffer_, beastRequest_,
            [self](beast::error_code ec, std::size_t /*bytes_transferred*/) {
                if (!ec) {
                    self->processRequest();
                }
                // On error, session is dropped (shared_ptr ref count -> 0)
            }
        );
    }

    void processRequest() {
        auto self = shared_from_this();

        // ── Build nodepp::Request from Beast request ──
        Request req;
        req.method = std::string(beastRequest_.method_string());
        req.url    = std::string(beastRequest_.target());

        auto [path, queryString] = detail::splitUrl(req.url);
        req.path     = path;
        req.query    = detail::parseQueryString(queryString);
        req.rawBody  = beastRequest_.body();
        req.protocol = "http";

        // Remote IP
        try {
            req.ip = socket_.remote_endpoint().address().to_string();
        } catch (...) {
            req.ip = "unknown";
        }

        // Copy headers (lowercase keys for consistent lookup)
        for (auto& field : beastRequest_) {
            req.headers[detail::toLower(std::string(field.name_string()))]
                = std::string(field.value());
        }

        // Hostname from Host header
        req.hostname = req.header("host");

        // ── Build nodepp::Response with Beast write callback ──
        Response res([self](int statusCode,
                            const std::unordered_map<std::string, std::string>& headers,
                            const std::string& body) {
            // Create Beast response
            auto beastRes = std::make_shared<bhttp::response<bhttp::string_body>>();
            beastRes->result(static_cast<bhttp::status>(statusCode));
            beastRes->version(self->beastRequest_.version());

            // Copy headers
            for (auto& [key, value] : headers) {
                if (!value.empty()) {
                    beastRes->set(key, value);
                }
            }

            beastRes->body() = body;
            beastRes->prepare_payload();

            // Keep-alive
            beastRes->keep_alive(self->beastRequest_.keep_alive());

            // Async write
            bhttp::async_write(
                self->socket_, *beastRes,
                [self, beastRes](beast::error_code ec, std::size_t /*bytes*/) {
                    if (self->beastRequest_.keep_alive() && !ec) {
                        // Reset and read next request
                        self->buffer_.consume(self->buffer_.size());
                        self->beastRequest_ = {};
                        self->readRequest();
                    } else {
                        // Close the connection
                        beast::error_code shutdown_ec;
                        self->socket_.shutdown(tcp::socket::shutdown_send, shutdown_ec);
                    }
                }
            );
        });

        // ── Run the handler ──
        server_.handleRequest(req, res);

        // If handler didn't send a response, send 404
        if (!res.headersSent()) {
            res.status(404).json(nlohmann::json{
                {"error", "Not Found"},
                {"message", "No response sent by handler"}
            });
        }
    }
};

// ═══════════════════════════════════════════
//  Listener — Accepts incoming TCP connections
// ═══════════════════════════════════════════
class HttpListener : public std::enable_shared_from_this<HttpListener> {
public:
    HttpListener(net::io_context& ioc, tcp::endpoint endpoint, Server::Impl& server)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , server_(server)
    {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) throw std::runtime_error("Failed to open acceptor: " + ec.message());

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) throw std::runtime_error("Failed to set reuse_address: " + ec.message());

        acceptor_.bind(endpoint, ec);
        if (ec) throw std::runtime_error("Failed to bind to port: " + ec.message());

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) throw std::runtime_error("Failed to listen: " + ec.message());
    }

    void run() {
        doAccept();
    }

private:
    net::io_context& ioc_;
    tcp::acceptor    acceptor_;
    Server::Impl&    server_;

    void doAccept() {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&HttpListener::onAccept, shared_from_this())
        );
    }

    void onAccept(beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<HttpSession>(std::move(socket), server_)->run();
        }
        // Always continue accepting (unless io_context is stopped)
        doAccept();
    }
};

// ═══════════════════════════════════════════
//  Server — Public API implementation
// ═══════════════════════════════════════════

Server::Server()
    : impl_(std::make_unique<Impl>())
{}

Server::~Server() = default;
Server::Server(Server&&) noexcept = default;
Server& Server::operator=(Server&&) noexcept = default;

Server& Server::use(MiddlewareFunction middleware) {
    impl_->middlewares.push_back(std::move(middleware));
    return *this;
}

void Server::addRoute(const std::string& method,
                       const std::string& pattern,
                       RouteHandler handler) {
    impl_->routes.push_back(
        detail::compileRoute(method, pattern, std::move(handler))
    );
}

void Server::handleRequest(Request& req, Response& res) {
    impl_->handleRequest(req, res);
}

void Server::listen(int port, std::function<void()> callback) {
    listen("0.0.0.0", port, std::move(callback));
}

void Server::listen(const std::string& host, int port, std::function<void()> callback) {
    impl_->ioc = std::make_unique<net::io_context>(1);

    auto address  = net::ip::make_address(host);
    auto endpoint = tcp::endpoint(address, static_cast<unsigned short>(port));

    // Create and start the listener
    auto httpListener = std::make_shared<HttpListener>(*impl_->ioc, endpoint, *impl_);
    httpListener->run();

    impl_->running = true;

    // Fire the callback before entering the event loop
    if (callback) {
        callback();
    }

    // Emit 'listening' event
    emit("listening");

    // Block on the event loop
    impl_->ioc->run();
}

void Server::close() {
    if (impl_->ioc && impl_->running) {
        impl_->running = false;
        impl_->ioc->stop();
        emit("close");
    }
}

} // namespace nodepp::http
