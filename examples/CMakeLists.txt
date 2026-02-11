# ──────────────────────────────────────────────
# Node++ Examples
# ──────────────────────────────────────────────

function(nodepp_add_example EXAMPLE_NAME)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_NAME}.cpp)
    target_link_libraries(${EXAMPLE_NAME} PRIVATE nodepp)
endfunction()

# Core examples
nodepp_add_example(hello_world)
nodepp_add_example(rest_api)
nodepp_add_example(graphql_server)

# New feature examples
nodepp_add_example(file_server)
nodepp_add_example(jwt_auth)
nodepp_add_example(validated_api)
nodepp_add_example(websocket_chat)
nodepp_add_example(database_api)
nodepp_add_example(observable_server)
