#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/observability.h — Request ID, metrics, health check
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include <string>
#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>
#include <random>
#include <iomanip>
#include <iostream>
#include <unordered_map>

namespace nodepp::observability {

// ═══════════════════════════════════════════
//  Request ID middleware
// ═══════════════════════════════════════════
namespace detail {
inline std::string generateRequestId() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    auto c = counter.fetch_add(1);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%llx-%04llx",
        static_cast<unsigned long long>(ms),
        static_cast<unsigned long long>(c & 0xFFFF));
    return buf;
}
} // namespace detail

inline http::MiddlewareFunction requestId(const std::string& headerName = "X-Request-Id") {
    return [headerName](http::Request& req, http::Response& res, http::NextFunction next) {
        auto existing = req.header(headerName);
        auto id = existing.empty() ? detail::generateRequestId() : existing;
        req.headers[headerName] = id;
        res.set(headerName, id);
        next();
    };
}

// ═══════════════════════════════════════════
//  Prometheus-compatible metrics
// ═══════════════════════════════════════════
class Metrics {
public:
    static Metrics& instance() {
        static Metrics m;
        return m;
    }

    void recordRequest(const std::string& method, const std::string& path,
                       int status, double durationMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        totalRequests_++;
        auto key = method + " " + path;
        routeHits_[key]++;
        statusCounts_[status]++;
        totalDurationMs_ += durationMs;

        if (durationMs > maxDurationMs_) maxDurationMs_ = durationMs;
    }

    std::string serialize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "# HELP http_requests_total Total HTTP requests\n"
            << "# TYPE http_requests_total counter\n"
            << "http_requests_total " << totalRequests_ << "\n\n"
            << "# HELP http_request_duration_ms_total Total request duration\n"
            << "# TYPE http_request_duration_ms_total counter\n"
            << "http_request_duration_ms_total " << std::fixed << std::setprecision(2)
            << totalDurationMs_ << "\n\n"
            << "# HELP http_request_duration_ms_max Max request duration\n"
            << "# TYPE http_request_duration_ms_max gauge\n"
            << "http_request_duration_ms_max " << std::fixed << std::setprecision(2)
            << maxDurationMs_ << "\n\n";

        oss << "# HELP http_requests_by_status Requests by HTTP status code\n"
            << "# TYPE http_requests_by_status counter\n";
        for (auto& [status, count] : statusCounts_) {
            oss << "http_requests_by_status{status=\"" << status << "\"} " << count << "\n";
        }
        oss << "\n# HELP http_requests_by_route Requests by route\n"
            << "# TYPE http_requests_by_route counter\n";
        for (auto& [route, count] : routeHits_) {
            oss << "http_requests_by_route{route=\"" << route << "\"} " << count << "\n";
        }
        return oss.str();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        totalRequests_ = 0;
        totalDurationMs_ = 0;
        maxDurationMs_ = 0;
        routeHits_.clear();
        statusCounts_.clear();
    }

    uint64_t totalRequests() const { return totalRequests_; }

private:
    Metrics() = default;
    mutable std::mutex mutex_;
    uint64_t totalRequests_ = 0;
    double totalDurationMs_ = 0;
    double maxDurationMs_ = 0;
    std::unordered_map<std::string, uint64_t> routeHits_;
    std::unordered_map<int, uint64_t> statusCounts_;
};

inline http::MiddlewareFunction metrics() {
    return [](http::Request& req, http::Response& res, http::NextFunction next) {
        auto start = std::chrono::steady_clock::now();
        next();
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000.0;
        Metrics::instance().recordRequest(req.method, req.path, res.getStatusCode(), ms);
    };
}

inline http::RouteHandler metricsEndpoint() {
    return [](http::Request&, http::Response& res) {
        res.type("text/plain; version=0.0.4; charset=utf-8");
        res.send(Metrics::instance().serialize());
    };
}

// ═══════════════════════════════════════════
//  Health check endpoint
// ═══════════════════════════════════════════
struct HealthStatus {
    bool healthy = true;
    std::string version = "0.1.0";
    std::unordered_map<std::string, bool> checks;
};

inline http::RouteHandler healthCheck(HealthStatus status = {}) {
    return [status](http::Request&, http::Response& res) {
        nlohmann::json j = {
            {"status", status.healthy ? "healthy" : "unhealthy"},
            {"version", status.version},
            {"uptime", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()}
        };
        if (!status.checks.empty()) {
            nlohmann::json checks = nlohmann::json::object();
            for (auto& [name, ok] : status.checks) {
                checks[name] = ok ? "ok" : "failing";
            }
            j["checks"] = checks;
        }
        res.status(status.healthy ? 200 : 503).json(j);
    };
}

// ═══════════════════════════════════════════
//  Structured JSON logging
// ═══════════════════════════════════════════
inline http::MiddlewareFunction jsonLogger() {
    return [](http::Request& req, http::Response& res, http::NextFunction next) {
        auto start = std::chrono::steady_clock::now();
        next();
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000.0;

        nlohmann::json log = {
            {"method", req.method},
            {"path", req.path},
            {"status", res.getStatusCode()},
            {"duration_ms", ms},
            {"ip", req.ip},
            {"user_agent", req.header("user-agent")}
        };
        auto reqId = req.header("x-request-id");
        if (!reqId.empty()) log["request_id"] = reqId;

        std::cout << log.dump() << std::endl;
    };
}

} // namespace nodepp::observability
