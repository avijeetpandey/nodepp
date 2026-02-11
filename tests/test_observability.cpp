// ═══════════════════════════════════════════════════════════════════
//  test_observability.cpp — Tests for metrics, request ID, health
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/observability.h>

using namespace nodepp;
using namespace nodepp::observability;

TEST(RequestIdTest, GeneratesUniqueIds) {
    auto id1 = detail::generateRequestId();
    auto id2 = detail::generateRequestId();
    EXPECT_NE(id1, id2);
}

TEST(MetricsTest, RecordAndSerialize) {
    auto& m = Metrics::instance();
    m.reset();

    m.recordRequest("GET", "/api/users", 200, 15.5);
    m.recordRequest("POST", "/api/users", 201, 30.2);
    m.recordRequest("GET", "/api/users", 200, 5.0);

    EXPECT_EQ(m.totalRequests(), 3u);

    auto output = m.serialize();
    EXPECT_TRUE(output.find("http_requests_total 3") != std::string::npos);
    EXPECT_TRUE(output.find("http_request_duration_ms_max") != std::string::npos);
}

TEST(MetricsTest, Reset) {
    auto& m = Metrics::instance();
    m.recordRequest("GET", "/test", 200, 1.0);
    m.reset();
    EXPECT_EQ(m.totalRequests(), 0u);
}

TEST(MetricsTest, StatusCodeCounts) {
    auto& m = Metrics::instance();
    m.reset();

    m.recordRequest("GET", "/", 200, 1.0);
    m.recordRequest("GET", "/missing", 404, 1.0);
    m.recordRequest("POST", "/", 500, 1.0);

    auto output = m.serialize();
    EXPECT_TRUE(output.find("status=\"200\"") != std::string::npos);
    EXPECT_TRUE(output.find("status=\"404\"") != std::string::npos);
    EXPECT_TRUE(output.find("status=\"500\"") != std::string::npos);
}

TEST(HealthCheckTest, DefaultHealthy) {
    HealthStatus status;
    EXPECT_TRUE(status.healthy);

    auto handler = healthCheck(status);

    http::Request req;
    int sentStatus = 0;
    std::string sentBody;
    http::Response res([&](int s, const auto&, const std::string& b) {
        sentStatus = s;
        sentBody = b;
    });

    handler(req, res);

    EXPECT_EQ(sentStatus, 200);
    auto j = nlohmann::json::parse(sentBody);
    EXPECT_EQ(j["status"], "healthy");
}

TEST(HealthCheckTest, UnhealthyStatus) {
    HealthStatus status;
    status.healthy = false;
    status.checks["database"] = false;

    auto handler = healthCheck(status);

    http::Request req;
    int sentStatus = 0;
    std::string sentBody;
    http::Response res([&](int s, const auto&, const std::string& b) {
        sentStatus = s;
        sentBody = b;
    });

    handler(req, res);

    EXPECT_EQ(sentStatus, 503);
    auto j = nlohmann::json::parse(sentBody);
    EXPECT_EQ(j["status"], "unhealthy");
}
