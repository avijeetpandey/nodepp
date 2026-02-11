#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/openapi.h — OpenAPI/Swagger auto-generation from routes
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include "json_utils.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace nodepp::openapi {

// ── Route metadata ──
struct RouteDoc {
    std::string method;
    std::string path;
    std::string summary;
    std::string description;
    std::vector<std::string> tags;
    int successStatus = 200;
    std::string requestBodyType;   // "application/json", etc.
    nlohmann::json requestSchema;  // JSON Schema
    nlohmann::json responseSchema; // JSON Schema
};

// ── OpenAPI document builder ──
class Document {
public:
    Document& title(const std::string& t) { title_ = t; return *this; }
    Document& description(const std::string& d) { description_ = d; return *this; }
    Document& version(const std::string& v) { version_ = v; return *this; }
    Document& server(const std::string& url, const std::string& desc = "") {
        servers_.push_back({url, desc});
        return *this;
    }

    Document& route(const RouteDoc& doc) {
        routes_.push_back(doc);
        return *this;
    }

    nlohmann::json generate() const {
        nlohmann::json spec = {
            {"openapi", "3.0.3"},
            {"info", {
                {"title", title_},
                {"description", description_},
                {"version", version_}
            }},
            {"paths", nlohmann::json::object()}
        };

        // Servers
        if (!servers_.empty()) {
            nlohmann::json servers = nlohmann::json::array();
            for (auto& [url, desc] : servers_) {
                nlohmann::json s = {{"url", url}};
                if (!desc.empty()) s["description"] = desc;
                servers.push_back(s);
            }
            spec["servers"] = servers;
        }

        // Paths
        for (auto& route : routes_) {
            std::string method = route.method;
            std::transform(method.begin(), method.end(), method.begin(), ::tolower);

            // Convert :param to {param} for OpenAPI
            std::string oaPath = route.path;
            std::size_t pos = 0;
            while ((pos = oaPath.find(':', pos)) != std::string::npos) {
                auto end = oaPath.find('/', pos);
                auto paramName = oaPath.substr(pos + 1, end == std::string::npos ? end : end - pos - 1);
                oaPath.replace(pos, paramName.size() + 1, "{" + paramName + "}");
                pos += paramName.size() + 2;
            }

            nlohmann::json operation;
            if (!route.summary.empty()) operation["summary"] = route.summary;
            if (!route.description.empty()) operation["description"] = route.description;
            if (!route.tags.empty()) operation["tags"] = route.tags;

            // Path parameters
            nlohmann::json params = nlohmann::json::array();
            std::string pathCopy = route.path;
            pos = 0;
            while ((pos = pathCopy.find(':', pos)) != std::string::npos) {
                auto end = pathCopy.find('/', pos);
                auto paramName = pathCopy.substr(pos + 1, end == std::string::npos ? end : end - pos - 1);
                params.push_back({
                    {"name", paramName},
                    {"in", "path"},
                    {"required", true},
                    {"schema", {{"type", "string"}}}
                });
                pos = end == std::string::npos ? pathCopy.size() : end;
            }
            if (!params.empty()) operation["parameters"] = params;

            // Request body
            if (!route.requestBodyType.empty()) {
                nlohmann::json content;
                nlohmann::json mediaType;
                if (!route.requestSchema.is_null()) {
                    mediaType["schema"] = route.requestSchema;
                }
                content[route.requestBodyType] = mediaType;
                operation["requestBody"] = {{"content", content}};
            }

            // Response
            nlohmann::json responses;
            nlohmann::json successResp = {{"description", "Success"}};
            if (!route.responseSchema.is_null()) {
                successResp["content"] = {
                    {"application/json", {{"schema", route.responseSchema}}}
                };
            }
            responses[std::to_string(route.successStatus)] = successResp;
            operation["responses"] = responses;

            spec["paths"][oaPath][method] = operation;
        }

        return spec;
    }

    // ── Create endpoint that serves the spec ──
    http::RouteHandler serveSpec() {
        auto spec = generate();
        return [spec](http::Request&, http::Response& res) {
            res.json(spec);
        };
    }

private:
    std::string title_ = "API";
    std::string description_;
    std::string version_ = "1.0.0";
    std::vector<std::pair<std::string, std::string>> servers_;
    std::vector<RouteDoc> routes_;
};

} // namespace nodepp::openapi
