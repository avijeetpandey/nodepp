#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/perf.h — Performance utilities: memory pools, fast parsing
// ═══════════════════════════════════════════════════════════════════

#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <mutex>

namespace nodepp::perf {

// ═══════════════════════════════════════════
//  Arena Allocator (Memory Pool)
//  Fast bump allocator for request-scoped data
// ═══════════════════════════════════════════
class Arena {
public:
    explicit Arena(std::size_t blockSize = 4096)
        : blockSize_(blockSize) {
        allocateBlock();
    }

    // Allocate n bytes, aligned
    void* allocate(std::size_t n, std::size_t alignment = alignof(std::max_align_t)) {
        auto aligned = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned + n > blocks_.back().size) {
            allocateBlock(std::max(n + alignment, blockSize_));
            aligned = (offset_ + alignment - 1) & ~(alignment - 1);
        }
        void* ptr = blocks_.back().data.get() + aligned;
        offset_ = aligned + n;
        totalAllocated_ += n;
        return ptr;
    }

    // Allocate and construct an object
    template <typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    // Allocate a string copy
    const char* allocString(const std::string& s) {
        char* buf = static_cast<char*>(allocate(s.size() + 1, 1));
        std::memcpy(buf, s.data(), s.size());
        buf[s.size()] = '\0';
        return buf;
    }

    // Reset (keep allocated blocks, just reset offset)
    void reset() {
        // Keep only the first block
        if (blocks_.size() > 1) {
            auto firstBlock = std::move(blocks_.front());
            blocks_.clear();
            blocks_.push_back(std::move(firstBlock));
        }
        offset_ = 0;
        totalAllocated_ = 0;
    }

    std::size_t totalAllocated() const { return totalAllocated_; }
    std::size_t blockCount() const { return blocks_.size(); }

private:
    struct Block {
        std::unique_ptr<char[]> data;
        std::size_t size;
    };

    void allocateBlock(std::size_t size = 0) {
        auto sz = size > 0 ? size : blockSize_;
        Block block;
        block.data = std::make_unique<char[]>(sz);
        block.size = sz;
        blocks_.push_back(std::move(block));
        offset_ = 0;
    }

    std::size_t blockSize_;
    std::size_t offset_ = 0;
    std::size_t totalAllocated_ = 0;
    std::vector<Block> blocks_;
};

// ═══════════════════════════════════════════
//  Object Pool — reuse expensive objects
// ═══════════════════════════════════════════
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(std::size_t initialSize = 16) {
        for (std::size_t i = 0; i < initialSize; i++) {
            pool_.push_back(std::make_unique<T>());
        }
    }

    std::unique_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.empty()) {
            return std::make_unique<T>();
        }
        auto obj = std::move(pool_.back());
        pool_.pop_back();
        return obj;
    }

    void release(std::unique_ptr<T> obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::move(obj));
    }

    std::size_t available() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<T>> pool_;
};

// ═══════════════════════════════════════════
//  String View utilities for zero-copy parsing
// ═══════════════════════════════════════════
namespace parse {

// Split a string_view by delimiter without allocating
inline std::vector<std::string_view> split(std::string_view sv, char delim) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= sv.size(); i++) {
        if (i == sv.size() || sv[i] == delim) {
            if (i > start) {
                parts.push_back(sv.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    return parts;
}

// Trim whitespace from a string_view
inline std::string_view trim(std::string_view sv) {
    auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    auto end = sv.find_last_not_of(" \t\r\n");
    return sv.substr(start, end - start + 1);
}

// Parse a key=value pair
struct KeyValue {
    std::string_view key;
    std::string_view value;
};

inline KeyValue parseKeyValue(std::string_view sv, char delim = '=') {
    auto pos = sv.find(delim);
    if (pos == std::string_view::npos) return {trim(sv), {}};
    return {trim(sv.substr(0, pos)), trim(sv.substr(pos + 1))};
}

// Parse query string: key1=val1&key2=val2
inline std::vector<KeyValue> parseQueryString(std::string_view qs) {
    std::vector<KeyValue> result;
    auto parts = split(qs, '&');
    for (auto part : parts) {
        result.push_back(parseKeyValue(part, '='));
    }
    return result;
}

// Fast integer parsing (no exceptions, no allocation)
inline bool parseInt(std::string_view sv, int& out) {
    if (sv.empty()) return false;
    int result = 0;
    bool negative = false;
    std::size_t i = 0;
    if (sv[0] == '-') { negative = true; i = 1; }
    else if (sv[0] == '+') { i = 1; }
    for (; i < sv.size(); i++) {
        if (sv[i] < '0' || sv[i] > '9') return false;
        result = result * 10 + (sv[i] - '0');
    }
    out = negative ? -result : result;
    return true;
}

} // namespace parse

} // namespace nodepp::perf
