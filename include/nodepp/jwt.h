#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/jwt.h — JSON Web Token sign, verify, decode
// ═══════════════════════════════════════════════════════════════════

#include "crypto.h"
#include "http.h"
#include "json_utils.h"
#include <string>
#include <chrono>
#include <stdexcept>

namespace nodepp::jwt {

// ── Sign options ──
struct SignOptions {
    int expiresInSec = 3600;     // 1 hour
    std::string issuer;
    std::string subject;
    std::string audience;
};

// ── Decoded token ──
struct DecodedToken {
    nlohmann::json header;
    nlohmann::json payload;
    std::string signature;
    bool valid = false;
    std::string error;
};

// ── Sign a JWT (HS256) ──
inline std::string sign(const nlohmann::json& payload, const std::string& secret,
                        SignOptions opts = {}) {
    nlohmann::json header = {{"alg", "HS256"}, {"typ", "JWT"}};

    nlohmann::json claims = payload;
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    claims["iat"] = epoch;
    if (opts.expiresInSec != 0) claims["exp"] = epoch + opts.expiresInSec;
    if (!opts.issuer.empty()) claims["iss"] = opts.issuer;
    if (!opts.subject.empty()) claims["sub"] = opts.subject;
    if (!opts.audience.empty()) claims["aud"] = opts.audience;

    auto headerEnc = crypto::base64UrlEncode(header.dump());
    auto payloadEnc = crypto::base64UrlEncode(claims.dump());
    auto sigInput = headerEnc + "." + payloadEnc;
    auto signature = crypto::base64UrlEncode(crypto::hmacSha256(secret, sigInput));

    return sigInput + "." + signature;
}

// ── Decode without verification ──
inline DecodedToken decode(const std::string& token) {
    DecodedToken result;

    auto dot1 = token.find('.');
    if (dot1 == std::string::npos) {
        result.error = "Invalid token format";
        return result;
    }
    auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) {
        result.error = "Invalid token format";
        return result;
    }

    try {
        result.header = nlohmann::json::parse(crypto::base64UrlDecode(token.substr(0, dot1)));
        result.payload = nlohmann::json::parse(
            crypto::base64UrlDecode(token.substr(dot1 + 1, dot2 - dot1 - 1)));
        result.signature = token.substr(dot2 + 1);
    } catch (const std::exception& e) {
        result.error = std::string("Decode error: ") + e.what();
    }
    return result;
}

// ── Verify and decode ──
inline DecodedToken verify(const std::string& token, const std::string& secret) {
    auto result = decode(token);
    if (!result.error.empty()) return result;

    // Verify signature
    auto dot2 = token.rfind('.');
    auto sigInput = token.substr(0, dot2);
    auto expectedSig = crypto::base64UrlEncode(crypto::hmacSha256(secret, sigInput));

    if (!crypto::timingSafeEqual(result.signature, expectedSig)) {
        result.error = "Invalid signature";
        return result;
    }

    // Check expiration
    if (result.payload.contains("exp")) {
        auto exp = result.payload["exp"].get<int64_t>();
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > exp) {
            result.error = "Token expired";
            return result;
        }
    }

    result.valid = true;
    return result;
}

// ── JWT authentication middleware ──
inline http::MiddlewareFunction auth(const std::string& secret) {
    return [secret](http::Request& req, http::Response& res, http::NextFunction next) {
        auto authHeader = req.header("authorization");
        if (authHeader.empty() || authHeader.substr(0, 7) != "Bearer ") {
            res.status(401).json({{"error", "No token provided"}});
            return;
        }

        auto token = authHeader.substr(7);
        auto decoded = verify(token, secret);

        if (!decoded.valid) {
            res.status(401).json({{"error", decoded.error}});
            return;
        }

        // Store decoded payload in request headers for handler access
        req.headers["x-jwt-payload"] = decoded.payload.dump();
        req.body.raw()["_user"] = decoded.payload;

        next();
    };
}

} // namespace nodepp::jwt
