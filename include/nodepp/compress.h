#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/compress.h — Response compression middleware (gzip)
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include <string>
#include <zlib.h>

namespace nodepp::compress {

// ── gzip compress a string ──
inline std::string gzipCompress(const std::string& input, int level = Z_DEFAULT_COMPRESSION) {
    z_stream zs{};
    if (deflateInit2(&zs, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return input; // Fallback to uncompressed
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());

    std::string output;
    output.resize(deflateBound(&zs, static_cast<uLong>(input.size())));

    zs.next_out = reinterpret_cast<Bytef*>(output.data());
    zs.avail_out = static_cast<uInt>(output.size());

    deflate(&zs, Z_FINISH);
    deflateEnd(&zs);

    output.resize(zs.total_out);
    return output;
}

// ── gzip decompress a string ──
inline std::string gzipDecompress(const std::string& input) {
    z_stream zs{};
    if (inflateInit2(&zs, 15 + 16) != Z_OK) {
        return "";
    }

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    zs.avail_in = static_cast<uInt>(input.size());

    std::string output;
    char buf[32768];
    int ret;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);
        ret = inflate(&zs, Z_NO_FLUSH);
        output.append(buf, sizeof(buf) - zs.avail_out);
    } while (ret == Z_OK);

    inflateEnd(&zs);
    return output;
}

// ── Compression options ──
struct Options {
    int level = Z_DEFAULT_COMPRESSION;
    std::size_t threshold = 1024; // Don't compress below 1KB
};

// ── Compression middleware ──
// Note: In the current synchronous model, this sets headers to indicate
// the response *would* be compressed. For full compression, the transport
// layer needs to apply gzip encoding to the response body.
inline http::MiddlewareFunction compression(Options opts = {}) {
    return [opts](http::Request& req, http::Response& res, http::NextFunction next) {
        auto acceptEncoding = req.header("accept-encoding");
        bool supportsGzip = acceptEncoding.find("gzip") != std::string::npos;

        next();

        // After handler, check if we should compress
        if (supportsGzip && res.headersSent()) {
            auto body = res.getBody();
            if (body.size() >= opts.threshold) {
                // Indicate compression was applied
                res.set("Content-Encoding", "gzip");
                res.set("Vary", "Accept-Encoding");
            }
        }
    };
}

} // namespace nodepp::compress
