#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/tls.h — TLS/HTTPS server configuration
// ═══════════════════════════════════════════════════════════════════
//
//  Usage:
//    auto app = http::createServer();
//    app.listen(443, tls::Options{
//        .certFile = "cert.pem",
//        .keyFile  = "key.pem"
//    });
//
//  Note: Full TLS integration requires the transport layer (http.cpp)
//  to use Boost.Asio SSL. This header provides configuration types
//  and helpers.
// ═══════════════════════════════════════════════════════════════════

#include <string>
#include <vector>

namespace nodepp::tls {

// ── TLS Options ──
struct Options {
    std::string certFile;               // Path to certificate PEM file
    std::string keyFile;                // Path to private key PEM file
    std::string caFile;                 // Path to CA bundle (optional)
    std::string passphrase;             // Key passphrase (optional)
    bool requestClientCert = false;     // mTLS: request client certificate
    bool rejectUnauthorized = true;     // Reject invalid client certs
    std::string minVersion = "TLSv1.2";
    std::vector<std::string> ciphers;   // Custom cipher list
};

// ── TLS Context (wraps configuration for use by the transport) ──
struct Context {
    Options options;
    bool enabled = false;

    static Context create(const Options& opts) {
        Context ctx;
        ctx.options = opts;
        ctx.enabled = !opts.certFile.empty() && !opts.keyFile.empty();
        return ctx;
    }
};

// ── Self-signed cert generation helper (for development only) ──
// Actual implementation would use OpenSSL APIs to generate certs.
// This is a placeholder documenting the intended API.
struct SelfSignedResult {
    std::string cert;
    std::string key;
};

// ── Create HTTPS redirect middleware ──
// Redirects HTTP requests to HTTPS
inline auto httpsRedirect(int httpsPort = 443) {
    return [httpsPort](auto& req, auto& res, auto next) {
        if (req.protocol != "https") {
            auto host = req.hostname;
            auto colonPos = host.find(':');
            if (colonPos != std::string::npos) host = host.substr(0, colonPos);
            auto url = "https://" + host;
            if (httpsPort != 443) url += ":" + std::to_string(httpsPort);
            url += req.url;
            res.redirect(301, url);
            return;
        }
        next();
    };
}

} // namespace nodepp::tls
