#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/security.h — Built-in security configuration types
// ═══════════════════════════════════════════════════════════════════

#include <string>
#include <vector>

namespace nodepp::security {

// ── CORS Configuration ──
struct CorsOptions {
    std::string origin       = "*";
    std::string methods      = "GET, POST, PUT, DELETE, PATCH, OPTIONS";
    std::string allowHeaders = "Content-Type, Authorization, X-Requested-With";
    std::string exposeHeaders;
    bool        credentials  = false;
    int         maxAge       = 86400; // seconds
};

// ── Rate Limiter Configuration ──
struct RateLimitOptions {
    int         windowMs = 60000;               // 1 minute window
    int         max      = 100;                 // max requests per window
    std::string message  = "Too many requests, please try again later.";
    int         statusCode = 429;
    // Headers
    bool        standardHeaders = true;         // RateLimit-* headers
    bool        legacyHeaders   = false;        // X-RateLimit-* headers
};

// ── Header Sanitization Options ──
struct SanitizeOptions {
    bool removeServerHeader        = true;
    bool addNoSniff                = true;       // X-Content-Type-Options: nosniff
    bool addFrameDeny              = true;       // X-Frame-Options: DENY
    bool addXssProtection          = true;       // X-XSS-Protection: 1; mode=block
    bool addHsts                   = false;      // Strict-Transport-Security (off by default)
    int  hstsMaxAge                = 31536000;
    bool hstsIncludeSubDomains     = true;
    bool addReferrerPolicy         = true;       // Referrer-Policy: strict-origin-when-cross-origin
    std::string contentSecurityPolicy;           // Empty = don't set
};

} // namespace nodepp::security
