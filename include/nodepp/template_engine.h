#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/template_engine.h — Mustache-like template engine
// ═══════════════════════════════════════════════════════════════════

#include "json_utils.h"
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <functional>
#include <stdexcept>

namespace nodepp::tmpl {

namespace detail {

inline std::string htmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
    return out;
}

inline std::string resolve(const nlohmann::json& ctx, const std::string& key) {
    // Support dotted paths: "user.name"
    const nlohmann::json* current = &ctx;
    std::istringstream stream(key);
    std::string part;
    while (std::getline(stream, part, '.')) {
        if (!current->is_object() || !current->contains(part)) return "";
        current = &(*current)[part];
    }
    if (current->is_string()) return current->get<std::string>();
    if (current->is_null()) return "";
    return current->dump();
}

} // namespace detail

// ── Render a template string with data ──
inline std::string render(const std::string& tpl, const nlohmann::json& data) {
    std::string result;
    result.reserve(tpl.size());
    std::size_t pos = 0;

    while (pos < tpl.size()) {
        auto open = tpl.find("{{", pos);
        if (open == std::string::npos) {
            result += tpl.substr(pos);
            break;
        }

        result += tpl.substr(pos, open - pos);
        auto close = tpl.find("}}", open);
        if (close == std::string::npos) {
            result += tpl.substr(pos);
            break;
        }

        auto tag = tpl.substr(open + 2, close - open - 2);
        // Trim whitespace
        auto start = tag.find_first_not_of(" \t");
        auto end_pos = tag.find_last_not_of(" \t");
        if (start != std::string::npos) {
            tag = tag.substr(start, end_pos - start + 1);
        }

        if (tag.empty()) {
            pos = close + 2;
            continue;
        }

        char prefix = tag[0];

        if (prefix == '{') {
            // Triple mustache {{{var}}} — unescaped
            auto innerClose = tpl.find("}}}", open);
            if (innerClose != std::string::npos) {
                auto inner = tpl.substr(open + 3, innerClose - open - 3);
                auto s2 = inner.find_first_not_of(" \t");
                auto e2 = inner.find_last_not_of(" \t");
                if (s2 != std::string::npos) inner = inner.substr(s2, e2 - s2 + 1);
                result += detail::resolve(data, inner);
                pos = innerClose + 3;
                continue;
            }
        }

        if (prefix == '#') {
            // Section: {{#key}}...{{/key}}
            auto key = tag.substr(1);
            auto endTag = "{{/" + key + "}}";
            auto endPos = tpl.find(endTag, close + 2);
            if (endPos == std::string::npos) {
                pos = close + 2;
                continue;
            }

            auto sectionBody = tpl.substr(close + 2, endPos - close - 2);
            auto val = data.contains(key) ? data[key] : nlohmann::json(nullptr);

            if (val.is_array()) {
                // {{#items}}...{{/items}} — iterate
                for (auto& item : val) {
                    nlohmann::json merged = data;
                    if (item.is_object()) {
                        for (auto& [k, v] : item.items()) merged[k] = v;
                    } else {
                        merged["."] = item;
                    }
                    result += render(sectionBody, merged);
                }
            } else if (val.is_boolean() && val.get<bool>()) {
                result += render(sectionBody, data);
            } else if (val.is_object()) {
                nlohmann::json merged = data;
                for (auto& [k, v] : val.items()) merged[k] = v;
                result += render(sectionBody, merged);
            } else if (!val.is_null() && !val.is_boolean()) {
                result += render(sectionBody, data);
            }

            pos = endPos + endTag.size();
            continue;
        }

        if (prefix == '^') {
            // Inverted section: {{^key}}...{{/key}}
            auto key = tag.substr(1);
            auto endTag = "{{/" + key + "}}";
            auto endPos = tpl.find(endTag, close + 2);
            if (endPos == std::string::npos) {
                pos = close + 2;
                continue;
            }

            auto sectionBody = tpl.substr(close + 2, endPos - close - 2);
            auto val = data.contains(key) ? data[key] : nlohmann::json(nullptr);

            bool isEmpty = val.is_null() ||
                           (val.is_boolean() && !val.get<bool>()) ||
                           (val.is_array() && val.empty());
            if (isEmpty) {
                result += render(sectionBody, data);
            }

            pos = endPos + endTag.size();
            continue;
        }

        if (prefix == '!' ) {
            // Comment: {{! this is a comment }}
            pos = close + 2;
            continue;
        }

        // Variable: {{key}} — HTML-escaped
        result += detail::htmlEscape(detail::resolve(data, tag));
        pos = close + 2;
    }

    return result;
}

// ── Render a template file ──
inline std::string renderFile(const std::string& path, const nlohmann::json& data) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Template file not found: " + path);
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return render(oss.str(), data);
}

// ── Template Engine with caching and partials ──
class Engine {
public:
    void setViewsDir(const std::string& dir) { viewsDir_ = dir; }
    void setExtension(const std::string& ext) { ext_ = ext; }

    void registerPartial(const std::string& name, const std::string& content) {
        partials_[name] = content;
    }

    std::string render(const std::string& name, const nlohmann::json& data) {
        auto tpl = loadTemplate(name);
        // Resolve partials: {{> partialName}}
        tpl = resolvePartials(tpl);
        return tmpl::render(tpl, data);
    }

private:
    std::string viewsDir_ = "views";
    std::string ext_ = ".html";
    std::unordered_map<std::string, std::string> cache_;
    std::unordered_map<std::string, std::string> partials_;

    std::string loadTemplate(const std::string& name) {
        auto it = cache_.find(name);
        if (it != cache_.end()) return it->second;

        auto path = viewsDir_ + "/" + name + ext_;
        std::ifstream file(path);
        if (!file.is_open()) throw std::runtime_error("Template not found: " + path);
        std::ostringstream oss;
        oss << file.rdbuf();
        cache_[name] = oss.str();
        return cache_[name];
    }

    std::string resolvePartials(const std::string& tpl) {
        std::string result = tpl;
        std::size_t pos = 0;
        while ((pos = result.find("{{>", pos)) != std::string::npos) {
            auto close = result.find("}}", pos);
            if (close == std::string::npos) break;
            auto name = result.substr(pos + 3, close - pos - 3);
            auto s = name.find_first_not_of(" \t");
            auto e = name.find_last_not_of(" \t");
            if (s != std::string::npos) name = name.substr(s, e - s + 1);

            std::string content;
            auto it = partials_.find(name);
            if (it != partials_.end()) {
                content = it->second;
            } else {
                try { content = loadTemplate(name); } catch (...) {}
            }
            result.replace(pos, close - pos + 2, content);
        }
        return result;
    }
};

} // namespace nodepp::tmpl
