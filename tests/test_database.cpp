// ═══════════════════════════════════════════════════════════════════
//  test_database.cpp — Tests for SQLite driver and query builder
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/database.h>

using namespace nodepp::db;

class DatabaseTest : public ::testing::Test {
protected:
    Database db{":memory:"};

    void SetUp() override {
        db.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT, age INTEGER)");
        db.exec("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", {"Alice", "alice@example.com", "30"});
        db.exec("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", {"Bob", "bob@example.com", "25"});
        db.exec("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", {"Charlie", "charlie@example.com", "35"});
    }
};

TEST_F(DatabaseTest, SelectAll) {
    auto result = db.exec("SELECT * FROM users");
    EXPECT_EQ(result.size(), 3u);
    EXPECT_FALSE(result.empty());
}

TEST_F(DatabaseTest, SelectWithParams) {
    auto result = db.exec("SELECT * FROM users WHERE name = ?", {"Alice"});
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result.first().at("name"), "Alice");
    EXPECT_EQ(result.first().at("email"), "alice@example.com");
}

TEST_F(DatabaseTest, InsertAndLastInsertId) {
    auto result = db.exec("INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
                          {"Dave", "dave@example.com", "40"});
    EXPECT_GT(result.lastInsertId, 0);
    EXPECT_EQ(result.affectedRows, 1);
}

TEST_F(DatabaseTest, UpdateRows) {
    auto result = db.exec("UPDATE users SET age = ? WHERE name = ?", {"31", "Alice"});
    EXPECT_EQ(result.affectedRows, 1);

    auto check = db.exec("SELECT age FROM users WHERE name = ?", {"Alice"});
    EXPECT_EQ(check.first().at("age"), "31");
}

TEST_F(DatabaseTest, DeleteRows) {
    auto result = db.exec("DELETE FROM users WHERE name = ?", {"Bob"});
    EXPECT_EQ(result.affectedRows, 1);

    auto check = db.exec("SELECT * FROM users");
    EXPECT_EQ(check.size(), 2u);
}

TEST_F(DatabaseTest, Transaction) {
    db.beginTransaction();
    db.exec("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", {"Eve", "eve@example.com", "28"});
    db.commit();

    auto result = db.exec("SELECT * FROM users WHERE name = ?", {"Eve"});
    EXPECT_EQ(result.size(), 1u);
}

TEST_F(DatabaseTest, TransactionRollback) {
    db.beginTransaction();
    db.exec("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", {"Eve", "eve@example.com", "28"});
    db.rollback();

    auto result = db.exec("SELECT * FROM users WHERE name = ?", {"Eve"});
    EXPECT_TRUE(result.empty());
}

TEST_F(DatabaseTest, TransactionScope) {
    auto result = db.transaction([&]() {
        db.exec("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", {"Frank", "frank@example.com", "45"});
        return db.exec("SELECT * FROM users WHERE name = ?", {"Frank"});
    });
    EXPECT_EQ(result.size(), 1u);
}

TEST_F(DatabaseTest, ExecMultiStatements) {
    Database db2(":memory:");
    db2.execMulti(
        "CREATE TABLE t1 (id INTEGER PRIMARY KEY);"
        "CREATE TABLE t2 (id INTEGER PRIMARY KEY);"
        "INSERT INTO t1 VALUES (1);"
        "INSERT INTO t2 VALUES (2);"
    );

    EXPECT_EQ(db2.exec("SELECT * FROM t1").size(), 1u);
    EXPECT_EQ(db2.exec("SELECT * FROM t2").size(), 1u);
}

TEST_F(DatabaseTest, ResultToJson) {
    auto result = db.exec("SELECT name, email FROM users ORDER BY name");
    auto json = result.toJson();

    ASSERT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 3u);
    EXPECT_EQ(json[0]["name"], "Alice");
}

TEST_F(DatabaseTest, ColumnNames) {
    auto result = db.exec("SELECT name, email FROM users LIMIT 1");
    EXPECT_EQ(result.columns.size(), 2u);
    EXPECT_TRUE(std::find(result.columns.begin(), result.columns.end(), "name") != result.columns.end());
    EXPECT_TRUE(std::find(result.columns.begin(), result.columns.end(), "email") != result.columns.end());
}

TEST_F(DatabaseTest, QueryBuilderSelect) {
    auto result = query(db).table("users").select().run();
    EXPECT_EQ(result.size(), 3u);
}

TEST_F(DatabaseTest, QueryBuilderSelectWithWhere) {
    auto result = query(db).table("users").select("name, email")
        .where("name = ?", "Alice").run();
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result.first().at("name"), "Alice");
}

TEST_F(DatabaseTest, QueryBuilderInsert) {
    query(db).table("users").insert({
        {"name", "Grace"},
        {"email", "grace@example.com"},
        {"age", "22"}
    }).run();

    auto result = db.exec("SELECT * FROM users WHERE name = ?", {"Grace"});
    EXPECT_EQ(result.size(), 1u);
}

TEST_F(DatabaseTest, QueryBuilderLimit) {
    auto result = query(db).table("users").select().limit(2).run();
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(DatabaseTest, MoveConstructor) {
    Database db2(":memory:");
    db2.exec("CREATE TABLE test (id INTEGER)");

    Database db3(std::move(db2));
    EXPECT_TRUE(db3.isOpen());
    // db2 should be empty now
    EXPECT_FALSE(db2.isOpen());
}

TEST_F(DatabaseTest, InvalidSqlThrows) {
    EXPECT_THROW(db.exec("INVALID SQL STATEMENT"), std::runtime_error);
}
