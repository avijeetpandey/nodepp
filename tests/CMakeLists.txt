# ──────────────────────────────────────────────
# Node++ Test Suite
# ──────────────────────────────────────────────

find_package(GTest CONFIG REQUIRED)

# ── JSON Serialization Tests ──
add_executable(test_json test_json.cpp)
target_link_libraries(test_json PRIVATE nodepp GTest::gtest GTest::gtest_main)
add_test(NAME test_json COMMAND test_json)

# ── HTTP Server Tests (unit tests, no network) ──
add_executable(test_http test_http.cpp)
target_link_libraries(test_http PRIVATE nodepp GTest::gtest GTest::gtest_main)
add_test(NAME test_http COMMAND test_http)

# ── GraphQL Tests ──
add_executable(test_graphql test_graphql.cpp)
target_link_libraries(test_graphql PRIVATE nodepp GTest::gtest GTest::gtest_main)
add_test(NAME test_graphql COMMAND test_graphql)
