#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/json_utils.h — Zero-config JSON serialization engine
// ═══════════════════════════════════════════════════════════════════
//  Uses nlohmann/json + C++20 Concepts for automatic conversion of
//  any standard C++ type (vectors, maps, structs) to/from JSON.
// ═══════════════════════════════════════════════════════════════════

#include <nlohmann/json.hpp>
#include <concepts>
#include <string>
#include <type_traits>
#include <optional>

namespace nodepp {

// ─────────────────────────────────────────────
//  Macro: NODE_SERIALIZE
//  Makes any struct auto-serializable to JSON.
//
//  Usage:
//    struct User {
//        std::string name;
//        int id;
//        NODE_SERIALIZE(User, name, id)
//    };
// ─────────────────────────────────────────────
#define NODE_SERIALIZE(Type, ...) \
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, __VA_ARGS__)

// ─────────────────────────────────────────────
//  Concept: JsonSerializable
//  Any type T that nlohmann::json can construct from.
// ─────────────────────────────────────────────
template <typename T>
concept JsonSerializable = requires(T t) {
    { nlohmann::json(t) } -> std::convertible_to<nlohmann::json>;
};

// ─────────────────────────────────────────────
//  Concept: JsonDeserializable
//  Any type T that nlohmann::json can convert to.
// ─────────────────────────────────────────────
template <typename T>
concept JsonDeserializable = requires(nlohmann::json j) {
    { j.get<T>() } -> std::same_as<T>;
};

// ─────────────────────────────────────────────
//  class JsonValue
//  Wraps nlohmann::json with an ergonomic API.
//  This powers `req.body` and `res.json()`.
// ─────────────────────────────────────────────
class JsonValue {
public:
    JsonValue() : data_(nlohmann::json::object()) {}
    JsonValue(const nlohmann::json& j) : data_(j) {}
    JsonValue(nlohmann::json&& j) : data_(std::move(j)) {}

    // Construct from any serializable type
    template <JsonSerializable T>
    JsonValue(const T& value) : data_(nlohmann::json(value)) {}

    // Construct from initializer list (e.g., {{"key", "value"}})
    JsonValue(std::initializer_list<nlohmann::json::basic_json::value_type> init)
        : data_(nlohmann::json(init)) {}

    // ── Subscript Access ──
    JsonValue operator[](const std::string& key) const {
        if (data_.contains(key)) {
            return JsonValue(data_[key]);
        }
        return JsonValue(nlohmann::json(nullptr));
    }

    // Overload for string literals to avoid ambiguity with implicit numeric conversions
    JsonValue operator[](const char* key) const {
        return operator[](std::string(key));
    }

    JsonValue operator[](std::size_t index) const {
        if (data_.is_array() && index < data_.size()) {
            return JsonValue(data_[index]);
        }
        return JsonValue(nlohmann::json(nullptr));
    }

    // Overload for int literals (resolves ambiguity between const char* and size_t)
    JsonValue operator[](int index) const {
        return operator[](static_cast<std::size_t>(index));
    }

    // ── Typed Getters ──
    template <typename T>
    T get() const {
        return data_.get<T>();
    }

    template <typename T>
    T get(const std::string& key) const {
        return data_.at(key).get<T>();
    }

    template <typename T>
    T get(const std::string& key, const T& defaultValue) const {
        if (data_.contains(key)) {
            try {
                return data_.at(key).get<T>();
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    // ── Implicit Conversions ──
    operator std::string() const {
        if (data_.is_string()) return data_.get<std::string>();
        return data_.dump();
    }

    operator int() const { return data_.get<int>(); }
    operator double() const { return data_.get<double>(); }
    operator bool() const {
        if (data_.is_boolean()) return data_.get<bool>();
        return !data_.is_null();
    }

    // ── Inspection ──
    bool isNull() const { return data_.is_null(); }
    bool isObject() const { return data_.is_object(); }
    bool isArray() const { return data_.is_array(); }
    bool isString() const { return data_.is_string(); }
    bool isNumber() const { return data_.is_number(); }
    bool has(const std::string& key) const { return data_.contains(key); }
    std::size_t size() const { return data_.size(); }

    // ── Serialization ──
    std::string dump(int indent = -1) const { return data_.dump(indent); }

    // ── Access underlying nlohmann::json ──
    const nlohmann::json& raw() const { return data_; }
    nlohmann::json& raw() { return data_; }

    // ── Iteration (for arrays and objects) ──
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }

    // ── Comparison ──
    bool operator==(const JsonValue& other) const { return data_ == other.data_; }
    bool operator!=(const JsonValue& other) const { return data_ != other.data_; }

    // ── nlohmann::json interop ──
    friend void to_json(nlohmann::json& j, const JsonValue& v) { j = v.data_; }
    friend void from_json(const nlohmann::json& j, JsonValue& v) { v.data_ = j; }

private:
    nlohmann::json data_;
};

// ─────────────────────────────────────────────
//  Helper: toJson / fromJson (free functions)
// ─────────────────────────────────────────────
template <JsonSerializable T>
inline nlohmann::json toJson(const T& value) {
    return nlohmann::json(value);
}

template <typename T>
inline T fromJson(const nlohmann::json& j) {
    return j.get<T>();
}

template <typename T>
inline T fromJson(const std::string& str) {
    return nlohmann::json::parse(str).get<T>();
}

} // namespace nodepp
