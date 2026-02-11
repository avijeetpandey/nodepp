# ──────────────────────────────────────────────
# Node++ Test Suite
# ──────────────────────────────────────────────

find_package(GTest CONFIG REQUIRED)

# Helper: add a test with common settings
function(nodepp_add_test TEST_NAME)
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE nodepp GTest::gtest GTest::gtest_main)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# ── Core Tests ──
nodepp_add_test(test_json)
nodepp_add_test(test_http)
nodepp_add_test(test_graphql)

# ── File Serving & Download ──
nodepp_add_test(test_sendfile)

# ── Multipart Parser ──
nodepp_add_test(test_multipart)

# ── Crypto ──
nodepp_add_test(test_crypto)

# ── JWT ──
nodepp_add_test(test_jwt)

# ── Session Management ──
nodepp_add_test(test_session)

# ── WebSocket ──
nodepp_add_test(test_websocket)

# ── Server-Sent Events ──
nodepp_add_test(test_sse)

# ── Compression ──
nodepp_add_test(test_compress)

# ── Validation ──
nodepp_add_test(test_validator)

# ── Template Engine ──
nodepp_add_test(test_template)

# ── Cache ──
nodepp_add_test(test_cache)

# ── Scheduler ──
nodepp_add_test(test_scheduler)

# ── Observability ──
nodepp_add_test(test_observability)

# ── Database ──
nodepp_add_test(test_database)

# ── Testing Utilities ──
nodepp_add_test(test_testing)

# ── OpenAPI ──
nodepp_add_test(test_openapi)

# ── Performance ──
nodepp_add_test(test_perf)
