#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/testing.h — TestClient (supertest equivalent), mock factories
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include "json_utils.h"
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace nodepp::testing {

// ── Create a mock Request ──
inline http::Request createRequest(
    const std::string& method = "GET",
    const std::string& path = "/",
    const std::string& body = "",
    const std::unordered_map<std::string, std::string>& headers = {}) {
    http::Request req;
    req.method = method;
    req.path = path;
    req.url = path;
    req.rawBody = body;
    req.ip = "127.0.0.1";
    req.protocol = "http";
    req.hostname = "localhost";
    req.headers = headers;
    // Lowercase all header keys
    std::unordered_map<std::string, std::string> lower;
    for (auto& [k, v] : req.headers) {
        std::string lk = k;
        std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
        lower[lk] = v;
    }
    req.headers = lower;
    return req;
}

// ── Create a capture-mode Response ──
inline http::Response createResponse() {
    return http::Response([](int, const auto&, const std::string&) {});
}

// ── Test Result ──
struct TestResult {
    int status = 0;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    nlohmann::json json() const {
        return nlohmann::json::parse(body);
    }
};

// ═══════════════════════════════════════════
//  TestClient — supertest-style API
// ═══════════════════════════════════════════
class TestClient {
public:
    explicit TestClient(http::Server& app) : app_(app) {}

    // ── Fluent request builder ──
    class RequestBuilder {
    public:
        RequestBuilder(http::Server& app, const std::string& method, const std::string& path)
            : app_(app), method_(method), path_(path) {}

        RequestBuilder& set(const std::string& key, const std::string& value) {
            headers_[key] = value;
            return *this;
        }

        RequestBuilder& send(const std::string& body) {
            body_ = body;
            if (headers_.find("Content-Type") == headers_.end()) {
                headers_["Content-Type"] = "application/json";
            }
            return *this;
        }

        RequestBuilder& send(const nlohmann::json& j) {
            body_ = j.dump();
            headers_["Content-Type"] = "application/json";
            return *this;
        }

        RequestBuilder& query(const std::string& key, const std::string& value) {
            query_[key] = value;
            return *this;
        }

        // ── Execute the request ──
        TestResult expect(int expectedStatus) {
            auto result = exec();
            if (result.status != expectedStatus) {
                throw std::runtime_error(
                    "Expected status " + std::to_string(expectedStatus) +
                    " but got " + std::to_string(result.status));
            }
            return result;
        }

        TestResult exec() {
            auto req = createRequest(method_, path_, body_, headers_);
            req.query = query_;

            // Parse body if JSON
            if (!body_.empty() && req.header("content-type").find("json") != std::string::npos) {
                try {
                    req.body = JsonValue(nlohmann::json::parse(body_));
                } catch (...) {}
            }

            TestResult result;
            http::Response res([&result](int status,
                                        const std::unordered_map<std::string, std::string>& headers,
                                        const std::string& body) {
                result.status = status;
                result.body = body;
                result.headers = headers;
            });

            app_.handleRequest(req, res);
            if (!res.headersSent()) {
                result.status = res.getStatusCode();
                result.body = res.getBody();
                result.headers = res.getHeaders();
            }
            return result;
        }

    private:
        http::Server& app_;
        std::string method_;
        std::string path_;
        std::string body_;
        std::unordered_map<std::string, std::string> headers_;
        std::unordered_map<std::string, std::string> query_;
    };

    RequestBuilder get(const std::string& path) { return {app_, "GET", path}; }
    RequestBuilder post(const std::string& path) { return {app_, "POST", path}; }
    RequestBuilder put(const std::string& path) { return {app_, "PUT", path}; }
    RequestBuilder patch(const std::string& path) { return {app_, "PATCH", path}; }
    RequestBuilder del(const std::string& path) { return {app_, "DELETE", path}; }

private:
    http::Server& app_;
};

} // namespace nodepp::testing
