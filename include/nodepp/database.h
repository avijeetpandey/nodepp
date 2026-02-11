#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/database.h — SQLite driver, query builder, connection pool
// ═══════════════════════════════════════════════════════════════════

#include "json_utils.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>

// Forward-declare sqlite3 types
struct sqlite3;
struct sqlite3_stmt;

namespace nodepp::db {

// ── Query result row ──
using Row = std::unordered_map<std::string, std::string>;

// ── Query result ──
struct Result {
    std::vector<Row> rows;
    std::vector<std::string> columns;
    int affectedRows = 0;
    int64_t lastInsertId = 0;

    bool empty() const { return rows.empty(); }
    std::size_t size() const { return rows.size(); }
    Row& first() { return rows.front(); }
    const Row& first() const { return rows.front(); }

    nlohmann::json toJson() const {
        nlohmann::json arr = nlohmann::json::array();
        for (auto& row : rows) {
            nlohmann::json obj = nlohmann::json::object();
            for (auto& [key, val] : row) {
                obj[key] = val;
            }
            arr.push_back(obj);
        }
        return arr;
    }
};

// ═══════════════════════════════════════════
//  SQLite Database Connection
// ═══════════════════════════════════════════
class Database {
public:
    explicit Database(const std::string& path = ":memory:");
    ~Database();

    // Non-copyable, movable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    // ── Execute a query ──
    Result exec(const std::string& sql);

    // ── Execute with bound parameters ──
    Result exec(const std::string& sql, const std::vector<std::string>& params);

    // ── Execute multiple statements (migrations, etc.) ──
    void execMulti(const std::string& sql);

    // ── Transaction helpers ──
    void beginTransaction();
    void commit();
    void rollback();

    // ── Transaction scope guard ──
    template <typename Func>
    auto transaction(Func&& fn) -> decltype(fn()) {
        beginTransaction();
        try {
            auto result = fn();
            commit();
            return result;
        } catch (...) {
            rollback();
            throw;
        }
    }

    bool isOpen() const { return db_ != nullptr; }
    void close();

private:
    sqlite3* db_ = nullptr;
};

// ═══════════════════════════════════════════
//  Query Builder (fluent API)
// ═══════════════════════════════════════════
class QueryBuilder {
public:
    explicit QueryBuilder(Database& db) : db_(db) {}

    QueryBuilder& table(const std::string& name) { table_ = name; return *this; }

    QueryBuilder& select(const std::string& cols = "*") {
        type_ = "SELECT";
        columns_ = cols;
        return *this;
    }

    QueryBuilder& where(const std::string& condition, const std::string& value = "") {
        if (!value.empty()) {
            conditions_.push_back(condition);
            params_.push_back(value);
        } else {
            rawConditions_.push_back(condition);
        }
        return *this;
    }

    QueryBuilder& orderBy(const std::string& col, const std::string& dir = "ASC") {
        orderBy_ = col + " " + dir;
        return *this;
    }

    QueryBuilder& limit(int n) { limit_ = n; return *this; }
    QueryBuilder& offset(int n) { offset_ = n; return *this; }

    QueryBuilder& insert(const std::unordered_map<std::string, std::string>& data) {
        type_ = "INSERT";
        insertData_ = data;
        return *this;
    }

    QueryBuilder& update(const std::unordered_map<std::string, std::string>& data) {
        type_ = "UPDATE";
        updateData_ = data;
        return *this;
    }

    QueryBuilder& del() {
        type_ = "DELETE";
        return *this;
    }

    // ── Build SQL string ──
    std::string toSql() const {
        std::ostringstream sql;
        if (type_ == "SELECT") {
            sql << "SELECT " << columns_ << " FROM " << table_;
        } else if (type_ == "INSERT") {
            sql << "INSERT INTO " << table_ << " (";
            std::string cols, vals;
            for (auto& [k, _] : insertData_) {
                if (!cols.empty()) { cols += ", "; vals += ", "; }
                cols += k;
                vals += "?";
            }
            sql << cols << ") VALUES (" << vals << ")";
        } else if (type_ == "UPDATE") {
            sql << "UPDATE " << table_ << " SET ";
            bool first = true;
            for (auto& [k, _] : updateData_) {
                if (!first) sql << ", ";
                sql << k << " = ?";
                first = false;
            }
        } else if (type_ == "DELETE") {
            sql << "DELETE FROM " << table_;
        }

        if (!conditions_.empty() || !rawConditions_.empty()) {
            sql << " WHERE ";
            bool first = true;
            for (auto& c : conditions_) {
                if (!first) sql << " AND ";
                sql << c;
                first = false;
            }
            for (auto& c : rawConditions_) {
                if (!first) sql << " AND ";
                sql << c;
                first = false;
            }
        }

        if (!orderBy_.empty()) sql << " ORDER BY " << orderBy_;
        if (limit_ > 0) sql << " LIMIT " << limit_;
        if (offset_ > 0) sql << " OFFSET " << offset_;

        return sql.str();
    }

    // ── Execute ──
    Result run() {
        std::vector<std::string> allParams = params_;
        if (type_ == "INSERT") {
            for (auto& [_, v] : insertData_) allParams.push_back(v);
        } else if (type_ == "UPDATE") {
            for (auto& [_, v] : updateData_) allParams.push_back(v);
            // WHERE params come after SET params
            // allParams already has params_ (WHERE params) at the start
            // Rearrange: update values first, then conditions
            std::vector<std::string> reordered;
            for (auto& [_, v] : updateData_) reordered.push_back(v);
            for (auto& p : params_) reordered.push_back(p);
            allParams = reordered;
        }
        return db_.exec(toSql(), allParams);
    }

private:
    Database& db_;
    std::string type_ = "SELECT";
    std::string table_;
    std::string columns_ = "*";
    std::vector<std::string> conditions_;
    std::vector<std::string> rawConditions_;
    std::vector<std::string> params_;
    std::string orderBy_;
    int limit_ = 0;
    int offset_ = 0;
    std::unordered_map<std::string, std::string> insertData_;
    std::unordered_map<std::string, std::string> updateData_;
};

// ── Convenience: start a query builder ──
inline QueryBuilder query(Database& db) {
    return QueryBuilder(db);
}

} // namespace nodepp::db
