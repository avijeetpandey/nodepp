#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/console.h — Node.js-style console logging with colors
// ═══════════════════════════════════════════════════════════════════

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <chrono>
#include <iomanip>
#include <type_traits>
#include <nlohmann/json.hpp>

namespace nodepp::console {

namespace detail {

// ANSI color codes
struct Colors {
    static constexpr const char* Reset   = "\033[0m";
    static constexpr const char* Red     = "\033[31m";
    static constexpr const char* Yellow  = "\033[33m";
    static constexpr const char* Blue    = "\033[34m";
    static constexpr const char* Cyan    = "\033[36m";
    static constexpr const char* Green   = "\033[32m";
    static constexpr const char* Gray    = "\033[90m";
    static constexpr const char* Bold    = "\033[1m";
};

// Stringify a single argument
template <typename T>
std::string stringify(const T& arg) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::string(std::string_view(arg));
    } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return arg ? "true" : "false";
    } else if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (std::is_floating_point_v<T>) {
            std::ostringstream oss;
            oss << arg;
            return oss.str();
        }
        return std::to_string(arg);
    } else if constexpr (requires { nlohmann::json(arg).dump(); }) {
        return nlohmann::json(arg).dump(2);
    } else {
        std::ostringstream oss;
        oss << arg;
        return oss.str();
    }
}

inline std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

template <typename... Args>
void print(std::ostream& os, const char* color, const char* prefix, const Args&... args) {
    os << detail::Colors::Gray << "[" << timestamp() << "] "
       << color << prefix << detail::Colors::Reset;

    bool first = true;
    auto printOne = [&](const auto& arg) {
        if (!first) os << " ";
        first = false;
        os << stringify(arg);
    };
    (printOne(args), ...);
    os << std::endl;
}

} // namespace detail

// ── console::log ──
template <typename... Args>
void log(const Args&... args) {
    detail::print(std::cout, detail::Colors::Reset, "", args...);
}

// ── console::info ──
template <typename... Args>
void info(const Args&... args) {
    detail::print(std::cout, detail::Colors::Blue, "ℹ ", args...);
}

// ── console::warn ──
template <typename... Args>
void warn(const Args&... args) {
    detail::print(std::cerr, detail::Colors::Yellow, "⚠ ", args...);
}

// ── console::error ──
template <typename... Args>
void error(const Args&... args) {
    detail::print(std::cerr, detail::Colors::Red, "✖ ", args...);
}

// ── console::success ──
template <typename... Args>
void success(const Args&... args) {
    detail::print(std::cout, detail::Colors::Green, "✔ ", args...);
}

// ── console::debug ──
template <typename... Args>
void debug(const Args&... args) {
    detail::print(std::cout, detail::Colors::Cyan, "● ", args...);
}

// ── console::time / console::timeEnd ──
namespace detail {
    inline std::unordered_map<std::string, std::chrono::steady_clock::time_point>& timers() {
        static std::unordered_map<std::string, std::chrono::steady_clock::time_point> t;
        return t;
    }
}

inline void time(const std::string& label) {
    detail::timers()[label] = std::chrono::steady_clock::now();
}

inline void timeEnd(const std::string& label) {
    auto it = detail::timers().find(label);
    if (it == detail::timers().end()) {
        warn("Timer '" + label + "' does not exist");
        return;
    }
    auto elapsed = std::chrono::steady_clock::now() - it->second;
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000.0;
    log(label + ":", std::to_string(ms) + "ms");
    detail::timers().erase(it);
}

} // namespace nodepp::console
