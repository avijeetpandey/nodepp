// ═══════════════════════════════════════════════════════════════════
//  database.cpp — SQLite driver implementation
// ═══════════════════════════════════════════════════════════════════

#include "nodepp/database.h"
#include <sqlite3.h>

namespace nodepp::db {

Database::Database(const std::string& path) {
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open database: " + err);
    }
    // Enable WAL mode for better concurrency
    exec("PRAGMA journal_mode=WAL");
}

Database::~Database() {
    close();
}

Database::Database(Database&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Result Database::exec(const std::string& sql) {
    return exec(sql, {});
}

Result Database::exec(const std::string& sql, const std::vector<std::string>& params) {
    Result result;

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()), &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("SQL error: " + std::string(sqlite3_errmsg(db_)));
    }

    // Bind parameters
    for (int i = 0; i < static_cast<int>(params.size()); i++) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(),
                          static_cast<int>(params[i].size()), SQLITE_TRANSIENT);
    }

    // Get column names
    int colCount = sqlite3_column_count(stmt);
    result.columns.reserve(colCount);
    for (int i = 0; i < colCount; i++) {
        result.columns.push_back(sqlite3_column_name(stmt, i));
    }

    // Execute and fetch rows
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Row row;
        for (int i = 0; i < colCount; i++) {
            auto text = sqlite3_column_text(stmt, i);
            row[result.columns[i]] = text ? reinterpret_cast<const char*>(text) : "";
        }
        result.rows.push_back(std::move(row));
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("SQL step error: " + std::string(sqlite3_errmsg(db_)));
    }

    result.affectedRows = sqlite3_changes(db_);
    result.lastInsertId = sqlite3_last_insert_rowid(db_);

    sqlite3_finalize(stmt);
    return result;
}

void Database::execMulti(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("SQL exec error: " + err);
    }
}

void Database::beginTransaction() {
    exec("BEGIN TRANSACTION");
}

void Database::commit() {
    exec("COMMIT");
}

void Database::rollback() {
    exec("ROLLBACK");
}

} // namespace nodepp::db
