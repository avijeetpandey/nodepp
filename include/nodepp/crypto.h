#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/crypto.h — Hashing, encryption, UUID, base64
// ═══════════════════════════════════════════════════════════════════

#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <stdexcept>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

namespace nodepp::crypto {

// ═══════════════════════════════════════════
//  Hashing
// ═══════════════════════════════════════════

inline std::string toHex(const unsigned char* data, std::size_t len) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < len; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

inline std::string sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    return toHex(hash, SHA256_DIGEST_LENGTH);
}

inline std::string sha512(const std::string& input) {
    unsigned char hash[SHA512_DIGEST_LENGTH];
    SHA512(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    return toHex(hash, SHA512_DIGEST_LENGTH);
}

inline std::string md5(const std::string& input) {
    unsigned char hash[MD5_DIGEST_LENGTH];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    MD5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
#pragma GCC diagnostic pop
    return toHex(hash, MD5_DIGEST_LENGTH);
}

// ═══════════════════════════════════════════
//  HMAC
// ═══════════════════════════════════════════

inline std::string hmacSha256(const std::string& key, const std::string& data) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         result, &len);
    return std::string(reinterpret_cast<char*>(result), len);
}

inline std::string hmacSha256Hex(const std::string& key, const std::string& data) {
    auto raw = hmacSha256(key, data);
    return toHex(reinterpret_cast<const unsigned char*>(raw.data()), raw.size());
}

// ═══════════════════════════════════════════
//  Base64
// ═══════════════════════════════════════════

inline std::string base64Encode(const std::string& input) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

inline std::string base64Decode(const std::string& input) {
    static const int lookup[] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };
    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (c == '=' || c >= 128) break;
        int v = lookup[c];
        if (v == -1) continue;
        val = (val << 6) + v;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// URL-safe Base64
inline std::string base64UrlEncode(const std::string& input) {
    auto encoded = base64Encode(input);
    for (char& c : encoded) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    // Remove trailing padding
    while (!encoded.empty() && encoded.back() == '=') encoded.pop_back();
    return encoded;
}

inline std::string base64UrlDecode(const std::string& input) {
    std::string padded = input;
    for (char& c : padded) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (padded.size() % 4) padded.push_back('=');
    return base64Decode(padded);
}

// ═══════════════════════════════════════════
//  Random bytes
// ═══════════════════════════════════════════

inline std::string randomBytes(std::size_t length) {
    std::string buf(length, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(buf.data()),
                   static_cast<int>(length)) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }
    return buf;
}

inline std::string randomHex(std::size_t length) {
    return toHex(reinterpret_cast<const unsigned char*>(randomBytes(length).data()), length);
}

// ═══════════════════════════════════════════
//  UUID v4
// ═══════════════════════════════════════════

inline std::string uuid() {
    auto bytes = randomBytes(16);
    auto* b = reinterpret_cast<unsigned char*>(bytes.data());
    b[6] = (b[6] & 0x0F) | 0x40; // Version 4
    b[8] = (b[8] & 0x3F) | 0x80; // Variant 1
    char buf[37];
    std::snprintf(buf, sizeof(buf),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
        b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
    return buf;
}

// ═══════════════════════════════════════════
//  Constant-time comparison (timing-attack safe)
// ═══════════════════════════════════════════

inline bool timingSafeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    volatile unsigned char result = 0;
    for (std::size_t i = 0; i < a.size(); i++) {
        result |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return result == 0;
}

} // namespace nodepp::crypto
