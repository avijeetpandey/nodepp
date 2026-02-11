#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/fetch.h — HTTP client (fetch/axios equivalent)
// ═══════════════════════════════════════════════════════════════════
//
//  Usage:
//    auto resp = fetch::get("http://example.com/api");
//    auto resp = fetch::post("http://example.com/api", {{"key","val"}});
//    auto resp = fetch::request({.url = "...", .method = "PUT"});
// ═══════════════════════════════════════════════════════════════════

#include "json_utils.h"
#include <string>
#include <unordered_map>
#include <sstream>
#include <stdexcept>

// Boost.Beast for HTTP client
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace nodepp::fetch {

namespace beast = boost::beast;
namespace http_ns = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// ── HTTP Response ──
struct FetchResponse {
    int status = 0;
    std::string statusText;
    std::string body;
    std::unordered_map<std::string, std::string> headers;

    bool ok() const { return status >= 200 && status < 300; }

    nlohmann::json json() const {
        return nlohmann::json::parse(body);
    }

    std::string text() const { return body; }
};

// ── Request options ──
struct RequestOptions {
    std::string url;
    std::string method = "GET";
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    int timeoutMs = 30000;
};

namespace detail {

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string port;
    std::string path;
};

inline ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl parsed;
    auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        parsed.scheme = "http";
        schemeEnd = 0;
    } else {
        parsed.scheme = url.substr(0, schemeEnd);
        schemeEnd += 3;
    }

    auto pathStart = url.find('/', schemeEnd);
    std::string hostPort;
    if (pathStart == std::string::npos) {
        hostPort = url.substr(schemeEnd);
        parsed.path = "/";
    } else {
        hostPort = url.substr(schemeEnd, pathStart - schemeEnd);
        parsed.path = url.substr(pathStart);
    }

    auto colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        parsed.host = hostPort.substr(0, colonPos);
        parsed.port = hostPort.substr(colonPos + 1);
    } else {
        parsed.host = hostPort;
        parsed.port = (parsed.scheme == "https") ? "443" : "80";
    }

    return parsed;
}

} // namespace detail

// ── Make an HTTP request ──
inline FetchResponse request(const RequestOptions& opts) {
    FetchResponse response;

    try {
        auto parsed = detail::parseUrl(opts.url);

        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        auto const results = resolver.resolve(parsed.host, parsed.port);
        stream.connect(results);

        // Build request
        auto verb = http_ns::string_to_verb(opts.method);
        if (verb == http_ns::verb::unknown) verb = http_ns::verb::get;

        http_ns::request<http_ns::string_body> req{verb, parsed.path, 11};
        req.set(http_ns::field::host, parsed.host);
        req.set(http_ns::field::user_agent, "nodepp-fetch/1.0");

        for (auto& [key, val] : opts.headers) {
            req.set(key, val);
        }

        if (!opts.body.empty()) {
            req.body() = opts.body;
            req.prepare_payload();
            if (req.find(http_ns::field::content_type) == req.end()) {
                req.set(http_ns::field::content_type, "application/json");
            }
        }

        http_ns::write(stream, req);

        beast::flat_buffer buffer;
        http_ns::response<http_ns::string_body> res;
        http_ns::read(stream, buffer, res);

        response.status = static_cast<int>(res.result_int());
        response.statusText = std::string(res.reason());
        response.body = res.body();

        for (auto& field : res) {
            response.headers[std::string(field.name_string())] = std::string(field.value());
        }

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    } catch (const std::exception& e) {
        response.status = 0;
        response.statusText = e.what();
    }

    return response;
}

// ── Convenience methods ──
inline FetchResponse get(const std::string& url,
                          const std::unordered_map<std::string, std::string>& headers = {}) {
    return request({.url = url, .method = "GET", .headers = headers});
}

inline FetchResponse post(const std::string& url, const nlohmann::json& body,
                           const std::unordered_map<std::string, std::string>& headers = {}) {
    auto h = headers;
    h["Content-Type"] = "application/json";
    return request({.url = url, .method = "POST", .headers = h, .body = body.dump()});
}

inline FetchResponse put(const std::string& url, const nlohmann::json& body,
                          const std::unordered_map<std::string, std::string>& headers = {}) {
    auto h = headers;
    h["Content-Type"] = "application/json";
    return request({.url = url, .method = "PUT", .headers = h, .body = body.dump()});
}

inline FetchResponse del(const std::string& url,
                          const std::unordered_map<std::string, std::string>& headers = {}) {
    return request({.url = url, .method = "DELETE", .headers = headers});
}

} // namespace nodepp::fetch
