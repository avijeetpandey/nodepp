// ═══════════════════════════════════════════════════════════════════
//  observable_server.cpp — Metrics, health checks, and logging
// ═══════════════════════════════════════════════════════════════════

#include <nodepp/http.h>
#include <nodepp/observability.h>
#include <nodepp/lifecycle.h>
#include <nodepp/console.h>
#include <thread>
#include <chrono>

using namespace nodepp;

int main() {
    auto app = http::createServer();

    // Enable observability middleware
    app.use(observability::requestId());
    app.use(observability::metrics());
    app.use(observability::jsonLogger());

    // Application routes
    app.get("/", [](http::Request&, http::Response& res) {
        res.json({{"message", "Hello from observable server!"}});
    });

    app.get("/slow", [](http::Request&, http::Response& res) {
        // Simulate slow endpoint
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        res.json({{"message", "This was slow"}});
    });

    // Observability endpoints
    app.get("/metrics", observability::metricsEndpoint());
    app.get("/health", observability::healthCheck({
        .healthy = true,
        .version = "1.0.0",
        .checks = {{"database", true}, {"cache", true}}
    }));

    // Graceful shutdown
    lifecycle::enableGracefulShutdown(app);

    app.listen(3000, [] {
        console::log("Observable server on http://localhost:3000");
        console::log("  GET /metrics — Prometheus metrics");
        console::log("  GET /health  — health check");
    });
}
