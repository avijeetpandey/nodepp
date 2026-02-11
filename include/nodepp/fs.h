#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/fs.h — Node.js-style file system operations
// ═══════════════════════════════════════════════════════════════════

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>
#include <system_error>
#include <future>
#include <stdexcept>

namespace nodepp::fs {

// ═══════════════════════════════════════════
//  Synchronous API
// ═══════════════════════════════════════════

inline std::string readFileSync(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("ENOENT: no such file or directory, open '" + path + "'");
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

inline void writeFileSync(const std::string& path, const std::string& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("EACCES: permission denied, open '" + path + "'");
    }
    file << data;
}

inline void appendFileSync(const std::string& path, const std::string& data) {
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("EACCES: permission denied, open '" + path + "'");
    }
    file << data;
}

inline bool existsSync(const std::string& path) {
    return std::filesystem::exists(path);
}

inline void mkdirSync(const std::string& path, bool recursive = false) {
    if (recursive) {
        std::filesystem::create_directories(path);
    } else {
        std::filesystem::create_directory(path);
    }
}

inline std::vector<std::string> readdirSync(const std::string& path) {
    std::vector<std::string> result;
    for (auto& entry : std::filesystem::directory_iterator(path)) {
        result.push_back(entry.path().filename().string());
    }
    return result;
}

inline void unlinkSync(const std::string& path) {
    if (!std::filesystem::remove(path)) {
        throw std::runtime_error("ENOENT: no such file or directory, unlink '" + path + "'");
    }
}

inline void rmdirSync(const std::string& path, bool recursive = false) {
    if (recursive) {
        std::filesystem::remove_all(path);
    } else {
        if (!std::filesystem::remove(path)) {
            throw std::runtime_error("ENOENT: no such file or directory, rmdir '" + path + "'");
        }
    }
}

inline void copyFileSync(const std::string& src, const std::string& dest) {
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing);
}

inline void renameSync(const std::string& oldPath, const std::string& newPath) {
    std::filesystem::rename(oldPath, newPath);
}

struct Stats {
    std::uintmax_t size;
    bool isFile;
    bool isDirectory;
    bool isSymlink;
    std::filesystem::file_time_type lastModified;
};

inline Stats statSync(const std::string& path) {
    auto status = std::filesystem::status(path);
    Stats s;
    s.size        = std::filesystem::file_size(path);
    s.isFile      = std::filesystem::is_regular_file(status);
    s.isDirectory = std::filesystem::is_directory(status);
    s.isSymlink   = std::filesystem::is_symlink(std::filesystem::symlink_status(path));
    s.lastModified = std::filesystem::last_write_time(path);
    return s;
}

// ═══════════════════════════════════════════
//  Asynchronous API (callback style)
// ═══════════════════════════════════════════

inline void readFile(const std::string& path,
                     std::function<void(std::error_code, std::string)> callback) {
    std::thread([path, callback = std::move(callback)]() {
        try {
            auto data = readFileSync(path);
            callback({}, std::move(data));
        } catch (...) {
            callback(std::make_error_code(std::errc::no_such_file_or_directory), "");
        }
    }).detach();
}

inline void writeFile(const std::string& path, const std::string& data,
                      std::function<void(std::error_code)> callback) {
    std::thread([path, data, callback = std::move(callback)]() {
        try {
            writeFileSync(path, data);
            callback({});
        } catch (...) {
            callback(std::make_error_code(std::errc::permission_denied));
        }
    }).detach();
}

} // namespace nodepp::fs
