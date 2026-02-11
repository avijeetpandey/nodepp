# ──────────────────────────────────────────────
# Node++ Examples
# ──────────────────────────────────────────────

add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PRIVATE nodepp)

add_executable(rest_api rest_api.cpp)
target_link_libraries(rest_api PRIVATE nodepp)

add_executable(graphql_server graphql_server.cpp)
target_link_libraries(graphql_server PRIVATE nodepp)
