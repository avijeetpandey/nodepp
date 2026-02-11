#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/path.h — Node.js-style path utilities
// ═══════════════════════════════════════════════════════════════════

#include <string>
#include <vector>
#include <filesystem>
#include <sstream>

namespace nodepp::path {

#ifdef _WIN32
    inline constexpr char sep = '\\';
    inline constexpr char delimiter = ';';
#else
    inline constexpr char sep = '/';
    inline constexpr char delimiter = ':';
#endif

// ── path::join ── Variadic join of path segments
template <typename... Args>
std::string join(const std::string& first, const Args&... rest) {
    std::filesystem::path result(first);
    ((result /= rest), ...);
    return result.string();
}

inline std::string join(const std::vector<std::string>& parts) {
    if (parts.empty()) return ".";
    std::filesystem::path result(parts[0]);
    for (std::size_t i = 1; i < parts.size(); ++i) {
        result /= parts[i];
    }
    return result.string();
}

// ── path::resolve ── Resolve to an absolute path
inline std::string resolve(const std::string& p) {
    return std::filesystem::absolute(p).string();
}

// ── path::basename ── Get the filename component
inline std::string basename(const std::string& p) {
    return std::filesystem::path(p).filename().string();
}

inline std::string basename(const std::string& p, const std::string& ext) {
    auto name = std::filesystem::path(p).filename().string();
    if (name.size() >= ext.size() &&
        name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
        return name.substr(0, name.size() - ext.size());
    }
    return name;
}

// ── path::dirname ── Get the directory component
inline std::string dirname(const std::string& p) {
    return std::filesystem::path(p).parent_path().string();
}

// ── path::extname ── Get the file extension
inline std::string extname(const std::string& p) {
    return std::filesystem::path(p).extension().string();
}

// ── path::normalize ── Normalize a path (resolve . and ..)
inline std::string normalize(const std::string& p) {
    return std::filesystem::path(p).lexically_normal().string();
}

// ── path::isAbsolute ──
inline bool isAbsolute(const std::string& p) {
    return std::filesystem::path(p).is_absolute();
}

// ── path::relative ── Compute relative path
inline std::string relative(const std::string& from, const std::string& to) {
    return std::filesystem::relative(to, from).string();
}

// ── path::parse ── Parse a path into components
struct ParsedPath {
    std::string root;
    std::string dir;
    std::string base;
    std::string ext;
    std::string name;
};

inline ParsedPath parse(const std::string& p) {
    std::filesystem::path fp(p);
    ParsedPath result;
    result.root = fp.root_path().string();
    result.dir  = fp.parent_path().string();
    result.base = fp.filename().string();
    result.ext  = fp.extension().string();
    result.name = fp.stem().string();
    return result;
}

} // namespace nodepp::path
