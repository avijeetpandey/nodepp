#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/multipart.h — Multipart form-data parser (multer equivalent)
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include "json_utils.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>

namespace nodepp::multipart {

// ── Uploaded file ──
struct UploadedFile {
    std::string fieldName;
    std::string filename;
    std::string contentType;
    std::string data;           // Raw binary content
    std::size_t size = 0;
};

// ── Parser result ──
struct ParseResult {
    std::unordered_map<std::string, std::string> fields;
    std::vector<UploadedFile> files;
};

namespace detail {

inline std::string extractBoundary(const std::string& contentType) {
    auto pos = contentType.find("boundary=");
    if (pos == std::string::npos) return "";
    auto boundary = contentType.substr(pos + 9);
    // Remove quotes if present
    if (!boundary.empty() && boundary.front() == '"') boundary = boundary.substr(1);
    if (!boundary.empty() && boundary.back() == '"') boundary.pop_back();
    // Remove trailing semicolons or whitespace
    auto end = boundary.find_first_of("; \t\r\n");
    if (end != std::string::npos) boundary = boundary.substr(0, end);
    return boundary;
}

inline std::string getHeaderValue(const std::string& headers, const std::string& name) {
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::istringstream stream(headers);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string lk;
        lk.reserve(key.size());
        for (char c : key) lk += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lk == lower) {
            auto val = line.substr(colon + 1);
            auto start = val.find_first_not_of(" \t");
            return start != std::string::npos ? val.substr(start) : "";
        }
    }
    return "";
}

inline std::string extractParam(const std::string& header, const std::string& param) {
    auto searchStr = param + "=\"";
    auto pos = header.find(searchStr);
    if (pos == std::string::npos) {
        searchStr = param + "=";
        pos = header.find(searchStr);
        if (pos == std::string::npos) return "";
        auto start = pos + searchStr.size();
        auto end = header.find_first_of("; \t\r\n", start);
        return end != std::string::npos ? header.substr(start, end - start) : header.substr(start);
    }
    auto start = pos + searchStr.size();
    auto end = header.find('"', start);
    return end != std::string::npos ? header.substr(start, end - start) : "";
}

} // namespace detail

// ── Parse multipart/form-data body ──
inline ParseResult parse(const std::string& body, const std::string& contentType) {
    ParseResult result;
    auto boundary = detail::extractBoundary(contentType);
    if (boundary.empty()) return result;

    std::string delimiter = "--" + boundary;
    std::string endDelimiter = delimiter + "--";

    std::size_t pos = 0;
    while (true) {
        auto partStart = body.find(delimiter, pos);
        if (partStart == std::string::npos) break;
        partStart += delimiter.size();

        // Skip \r\n after delimiter
        if (partStart < body.size() && body[partStart] == '\r') partStart++;
        if (partStart < body.size() && body[partStart] == '\n') partStart++;

        // Check for end delimiter
        if (body.substr(partStart - 2, 2) == "--") break;

        auto nextDelimiter = body.find(delimiter, partStart);
        if (nextDelimiter == std::string::npos) break;

        auto partContent = body.substr(partStart, nextDelimiter - partStart);
        // Remove trailing \r\n before next delimiter
        if (partContent.size() >= 2 && partContent.substr(partContent.size() - 2) == "\r\n") {
            partContent = partContent.substr(0, partContent.size() - 2);
        }

        // Split headers from body (empty line separates them)
        auto headerEnd = partContent.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            pos = nextDelimiter;
            continue;
        }

        auto headers = partContent.substr(0, headerEnd);
        auto partBody = partContent.substr(headerEnd + 4);

        auto disposition = detail::getHeaderValue(headers, "content-disposition");
        auto fieldName = detail::extractParam(disposition, "name");
        auto filename = detail::extractParam(disposition, "filename");
        auto partContentType = detail::getHeaderValue(headers, "content-type");

        if (!filename.empty()) {
            // File upload
            UploadedFile file;
            file.fieldName = fieldName;
            file.filename = filename;
            file.contentType = partContentType.empty() ? "application/octet-stream" : partContentType;
            file.data = partBody;
            file.size = partBody.size();
            result.files.push_back(std::move(file));
        } else if (!fieldName.empty()) {
            // Regular field
            result.fields[fieldName] = partBody;
        }

        pos = nextDelimiter;
    }

    return result;
}

// ── Multipart middleware options ──
struct Options {
    std::size_t maxFileSize = 10 * 1024 * 1024; // 10MB
    int maxFiles = 10;
    std::vector<std::string> allowedTypes;       // Empty = allow all
};

// ── Middleware that populates req with parsed files ──
inline http::MiddlewareFunction upload(Options opts = {}) {
    return [opts](http::Request& req, http::Response& res, http::NextFunction next) {
        auto ct = req.header("content-type");
        if (ct.find("multipart/form-data") == std::string::npos) {
            next();
            return;
        }

        auto result = parse(req.rawBody, ct);

        // Validate
        if (static_cast<int>(result.files.size()) > opts.maxFiles) {
            res.status(400).json({{"error", "Too many files"},
                                  {"max", opts.maxFiles}});
            return;
        }

        for (auto& file : result.files) {
            if (file.size > opts.maxFileSize) {
                res.status(413).json({{"error", "File too large"},
                                      {"file", file.filename},
                                      {"maxSize", opts.maxFileSize}});
                return;
            }
            if (!opts.allowedTypes.empty()) {
                bool allowed = false;
                for (auto& t : opts.allowedTypes) {
                    if (file.contentType.find(t) != std::string::npos) {
                        allowed = true;
                        break;
                    }
                }
                if (!allowed) {
                    res.status(415).json({{"error", "File type not allowed"},
                                          {"file", file.filename},
                                          {"type", file.contentType}});
                    return;
                }
            }
        }

        // Populate req.body with fields and files metadata
        nlohmann::json bodyData = req.body.raw();
        if (!bodyData.is_object()) bodyData = nlohmann::json::object();

        for (auto& [k, v] : result.fields) {
            bodyData[k] = v;
        }

        nlohmann::json filesArr = nlohmann::json::array();
        for (auto& f : result.files) {
            filesArr.push_back({
                {"fieldName", f.fieldName},
                {"filename", f.filename},
                {"contentType", f.contentType},
                {"size", f.size}
            });
        }
        bodyData["_files"] = filesArr;
        req.body = JsonValue(bodyData);

        // Store raw files in a header for handler access (encoded as count)
        req.headers["x-upload-count"] = std::to_string(result.files.size());

        next();
    };
}

} // namespace nodepp::multipart
