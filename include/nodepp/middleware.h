#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/middleware.h — Built-in Express-style middleware
// ═══════════════════════════════════════════════════════════════════
//
//  Included middleware:
//    • bodyParser()       — Auto-parse JSON request bodies
//    • cors()             — Cross-Origin Resource Sharing
//    • rateLimiter()      — Rate limiting per IP
//    • helmet()           — Security header sanitization
//    • requestLogger()    — Request logging (like Morgan)
//    • staticFiles()      — Serve static files from a directory
//
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include "security.h"
#include "console.h"
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace nodepp::middleware {

// ═══════════════════════════════════════════
//  bodyParser — Automatic JSON body parsing
// ═══════════════════════════════════════════
//  Detects `application/json` Content-Type and
//  automatically populates req.body as a JsonValue.
//  The user NEVER has to manually parse JSON.
//
inline http::MiddlewareFunction bodyParser() {
    return [](http::Request& req, http::Response& res, http::NextFunction next) {
        auto contentType = req.header("content-type");

        // Auto-detect JSON content
        if (contentType.find("application/json") != std::string::npos
            && !req.rawBody.empty())
        {
            try {
                auto parsed = nlohmann::json::parse(req.rawBody);
                req.body = JsonValue(std::move(parsed));
            } catch (const nlohmann::json::exception& e) {
                res.status(400).json(nlohmann::json{
                    {"error", "Bad Request"},
                    {"message", std::string("Invalid JSON: ") + e.what()}
                });
                return; // Stop the chain — don't call next()
            }
        }

        // Parse URL-encoded form data
        if (contentType.find("application/x-www-form-urlencoded") != std::string::npos
            && !req.rawBody.empty())
        {
            nlohmann::json formData = nlohmann::json::object();
            std::istringstream stream(req.rawBody);
            std::string pair;
            while (std::getline(stream, pair, '&')) {
                auto eq = pair.find('=');
                if (eq != std::string::npos) {
                    formData[pair.substr(0, eq)] = pair.substr(eq + 1);
                }
            }
            req.body = JsonValue(std::move(formData));
        }

        next();
    };
}

// ═══════════════════════════════════════════
//  cors — Cross-Origin Resource Sharing
// ═══════════════════════════════════════════
inline http::MiddlewareFunction cors(security::CorsOptions options = {}) {
    return [options](http::Request& req, http::Response& res, http::NextFunction next) {
        res.set("Access-Control-Allow-Origin", options.origin);
        res.set("Access-Control-Allow-Methods", options.methods);
        res.set("Access-Control-Allow-Headers", options.allowHeaders);

        if (!options.exposeHeaders.empty()) {
            res.set("Access-Control-Expose-Headers", options.exposeHeaders);
        }

        if (options.credentials) {
            res.set("Access-Control-Allow-Credentials", "true");
        }

        // Handle preflight OPTIONS request
        if (req.method == "OPTIONS") {
            res.set("Access-Control-Max-Age", std::to_string(options.maxAge));
            res.status(204).end();
            return;
        }

        next();
    };
}

// ═══════════════════════════════════════════
//  rateLimiter — IP-based rate limiting
// ═══════════════════════════════════════════
inline http::MiddlewareFunction rateLimiter(security::RateLimitOptions options = {}) {
    struct ClientRecord {
        int count = 0;
        std::chrono::steady_clock::time_point windowStart;
    };

    auto store = std::make_shared<std::unordered_map<std::string, ClientRecord>>();
    auto mutex = std::make_shared<std::mutex>();

    return [options, store, mutex](http::Request& req, http::Response& res, http::NextFunction next) {
        auto now = std::chrono::steady_clock::now();
        std::string key = req.ip;

        std::lock_guard<std::mutex> lock(*mutex);
        auto& record = (*store)[key];

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - record.windowStart).count();

        // Reset window if expired
        if (elapsed > options.windowMs || record.count == 0) {
            record.count = 0;
            record.windowStart = now;
        }

        record.count++;

        int remaining = std::max(0, options.max - record.count);
        auto resetTime = record.windowStart +
            std::chrono::milliseconds(options.windowMs);
        auto resetSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            resetTime.time_since_epoch()).count();

        // Set rate limit headers
        if (options.standardHeaders) {
            res.set("RateLimit-Limit", std::to_string(options.max));
            res.set("RateLimit-Remaining", std::to_string(remaining));
            res.set("RateLimit-Reset", std::to_string(resetSeconds));
        }

        if (record.count > options.max) {
            res.status(options.statusCode).json(nlohmann::json{
                {"error", "Too Many Requests"},
                {"message", options.message},
                {"retryAfter", (options.windowMs - elapsed) / 1000}
            });
            return;
        }

        next();
    };
}

// ═══════════════════════════════════════════
//  helmet — Security headers (like Helmet.js)
// ═══════════════════════════════════════════
inline http::MiddlewareFunction helmet(security::SanitizeOptions options = {}) {
    return [options](http::Request& req, http::Response& res, http::NextFunction next) {
        // Remove server identification
        if (options.removeServerHeader) {
            res.set("X-Powered-By", ""); // Will be omitted by transport
        }

        if (options.addNoSniff) {
            res.set("X-Content-Type-Options", "nosniff");
        }

        if (options.addFrameDeny) {
            res.set("X-Frame-Options", "DENY");
        }

        if (options.addXssProtection) {
            res.set("X-XSS-Protection", "1; mode=block");
        }

        if (options.addHsts) {
            std::string hsts = "max-age=" + std::to_string(options.hstsMaxAge);
            if (options.hstsIncludeSubDomains) {
                hsts += "; includeSubDomains";
            }
            res.set("Strict-Transport-Security", hsts);
        }

        if (options.addReferrerPolicy) {
            res.set("Referrer-Policy", "strict-origin-when-cross-origin");
        }

        if (!options.contentSecurityPolicy.empty()) {
            res.set("Content-Security-Policy", options.contentSecurityPolicy);
        }

        next();
    };
}

// ═══════════════════════════════════════════
//  requestLogger — Morgan-style request logging
// ═══════════════════════════════════════════
inline http::MiddlewareFunction requestLogger() {
    return [](http::Request& req, http::Response& res, http::NextFunction next) {
        auto start = std::chrono::steady_clock::now();
        console::info(req.method, req.path, "from", req.ip);

        next();

        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000.0;

        int status = res.getStatusCode();
        std::string statusStr = std::to_string(status);

        if (status >= 400) {
            console::error(req.method, req.path, statusStr,
                           std::to_string(ms) + "ms");
        } else {
            console::success(req.method, req.path, statusStr,
                             std::to_string(ms) + "ms");
        }
    };
}

// ═══════════════════════════════════════════
//  staticFiles — Serve static files
// ═══════════════════════════════════════════
inline http::MiddlewareFunction staticFiles(const std::string& root) {
    return [root](http::Request& req, http::Response& res, http::NextFunction next) {
        if (req.method != "GET" && req.method != "HEAD") {
            next();
            return;
        }

        std::filesystem::path filePath = std::filesystem::path(root) / req.path.substr(1);

        if (!std::filesystem::exists(filePath) || !std::filesystem::is_regular_file(filePath)) {
            next();
            return;
        }

        // Determine Content-Type from extension
        static const std::unordered_map<std::string, std::string> mimeTypes = {
            {".html", "text/html"},
            {".css",  "text/css"},
            {".js",   "application/javascript"},
            {".json", "application/json"},
            {".png",  "image/png"},
            {".jpg",  "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif",  "image/gif"},
            {".svg",  "image/svg+xml"},
            {".ico",  "image/x-icon"},
            {".txt",  "text/plain"},
            {".pdf",  "application/pdf"},
            {".woff", "font/woff"},
            {".woff2","font/woff2"},
            {".ttf",  "font/ttf"},
        };

        auto ext = filePath.extension().string();
        auto mimeIt = mimeTypes.find(ext);
        std::string contentType = (mimeIt != mimeTypes.end())
            ? mimeIt->second
            : "application/octet-stream";

        // Read file
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            next();
            return;
        }

        std::ostringstream oss;
        oss << file.rdbuf();

        res.type(contentType);
        res.send(oss.str());
    };
}

// ═══════════════════════════════════════════
//  cookieParser — Parse cookies from headers
// ═══════════════════════════════════════════
inline http::MiddlewareFunction cookieParser() {
    return [](http::Request& req, http::Response& res, http::NextFunction next) {
        auto cookieHeader = req.header("cookie");
        if (!cookieHeader.empty()) {
            std::istringstream stream(cookieHeader);
            std::string pair;
            while (std::getline(stream, pair, ';')) {
                // Trim leading whitespace
                auto start = pair.find_first_not_of(" \t");
                if (start == std::string::npos) continue;
                pair = pair.substr(start);

                auto eq = pair.find('=');
                if (eq != std::string::npos) {
                    req.cookies[pair.substr(0, eq)] = pair.substr(eq + 1);
                }
            }
        }
        next();
    };
}

} // namespace nodepp::middleware
