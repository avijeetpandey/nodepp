#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/sendfile.h — File download, streaming responses, Range
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <ctime>

namespace nodepp::sendfile {

namespace detail {

inline std::string getMimeType(const std::string& ext) {
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        {".html","text/html"}, {".htm","text/html"}, {".css","text/css"},
        {".js","application/javascript"}, {".json","application/json"},
        {".xml","text/xml"}, {".txt","text/plain"}, {".csv","text/csv"},
        {".png","image/png"}, {".jpg","image/jpeg"}, {".jpeg","image/jpeg"},
        {".gif","image/gif"}, {".svg","image/svg+xml"}, {".ico","image/x-icon"},
        {".webp","image/webp"}, {".bmp","image/bmp"},
        {".mp3","audio/mpeg"}, {".wav","audio/wav"}, {".ogg","audio/ogg"},
        {".mp4","video/mp4"}, {".webm","video/webm"}, {".avi","video/x-msvideo"},
        {".pdf","application/pdf"}, {".zip","application/zip"},
        {".gz","application/gzip"}, {".tar","application/x-tar"},
        {".woff","font/woff"}, {".woff2","font/woff2"}, {".ttf","font/ttf"},
        {".eot","application/vnd.ms-fontobject"},
    };
    auto it = mimeTypes.find(ext);
    return it != mimeTypes.end() ? it->second : "application/octet-stream";
}

inline std::string formatHttpDate(std::time_t t) {
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&t));
    return buf;
}

struct Range {
    std::size_t start;
    std::size_t end; // inclusive
};

inline bool parseRangeHeader(const std::string& header, std::size_t fileSize, Range& out) {
    // Parse "bytes=start-end" or "bytes=start-" or "bytes=-suffix"
    if (header.substr(0, 6) != "bytes=") return false;
    auto spec = header.substr(6);
    auto dash = spec.find('-');
    if (dash == std::string::npos) return false;

    if (dash == 0) {
        // bytes=-500 → last 500 bytes
        auto suffix = std::stoull(spec.substr(1));
        out.start = fileSize > suffix ? fileSize - suffix : 0;
        out.end = fileSize - 1;
    } else if (dash == spec.size() - 1) {
        // bytes=500- → from 500 to end
        out.start = std::stoull(spec.substr(0, dash));
        out.end = fileSize - 1;
    } else {
        out.start = std::stoull(spec.substr(0, dash));
        out.end = std::stoull(spec.substr(dash + 1));
    }

    if (out.start > out.end || out.start >= fileSize) return false;
    if (out.end >= fileSize) out.end = fileSize - 1;
    return true;
}

} // namespace detail

// ── Send a file with auto content-type and range support ──
inline void sendFile(http::Request& req, http::Response& res, const std::string& filePath) {
    namespace fs = std::filesystem;
    if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
        res.status(404).json({{"error", "File not found"}, {"path", filePath}});
        return;
    }

    auto fileSize = fs::file_size(filePath);
    auto ext = fs::path(filePath).extension().string();
    auto mtime = fs::last_write_time(filePath);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        mtime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    auto lastMod = std::chrono::system_clock::to_time_t(sctp);

    res.set("Content-Type", detail::getMimeType(ext));
    res.set("Accept-Ranges", "bytes");
    res.set("Last-Modified", detail::formatHttpDate(lastMod));

    // Check for Range request
    auto rangeHeader = req.header("range");
    if (!rangeHeader.empty()) {
        detail::Range range;
        if (detail::parseRangeHeader(rangeHeader, fileSize, range)) {
            auto length = range.end - range.start + 1;
            res.status(206);
            res.set("Content-Range", "bytes " + std::to_string(range.start) + "-"
                     + std::to_string(range.end) + "/" + std::to_string(fileSize));
            res.set("Content-Length", std::to_string(length));

            std::ifstream file(filePath, std::ios::binary);
            file.seekg(static_cast<std::streamoff>(range.start));
            std::string data(length, '\0');
            file.read(data.data(), static_cast<std::streamsize>(length));
            res.send(data);
            return;
        }
        // Invalid range
        res.status(416);
        res.set("Content-Range", "bytes */" + std::to_string(fileSize));
        res.send("");
        return;
    }

    // Full file
    res.set("Content-Length", std::to_string(fileSize));
    std::ifstream file(filePath, std::ios::binary);
    std::ostringstream oss;
    oss << file.rdbuf();
    res.send(oss.str());
}

// ── Trigger browser download ──
inline void download(http::Request& req, http::Response& res,
                     const std::string& filePath, const std::string& filename = "") {
    auto name = filename.empty() ? std::filesystem::path(filePath).filename().string() : filename;
    res.set("Content-Disposition", "attachment; filename=\"" + name + "\"");
    sendFile(req, res, filePath);
}

} // namespace nodepp::sendfile
