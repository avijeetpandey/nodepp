cmake_minimum_required(VERSION 3.20)

# Use config-mode Boost (FindBoost removed in CMake 4.x)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(nodepp
    VERSION 0.1.0
    DESCRIPTION "A high-performance C++ web framework with Node.js/Express-like API"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ──────────────────────────────────────────────
# Dependencies
# ──────────────────────────────────────────────
find_package(Boost CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ──────────────────────────────────────────────
# Library Target
# ──────────────────────────────────────────────
add_library(nodepp
    src/http.cpp
    src/graphql.cpp
)

target_include_directories(nodepp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(nodepp PUBLIC
    Boost::headers
    nlohmann_json::nlohmann_json
)

target_compile_features(nodepp PUBLIC cxx_std_20)

# ──────────────────────────────────────────────
# Alias for FetchContent consumers
# ──────────────────────────────────────────────
add_library(nodepp::nodepp ALIAS nodepp)

# ──────────────────────────────────────────────
# Tests
# ──────────────────────────────────────────────
option(NODEPP_BUILD_TESTS "Build unit tests" ON)
if(NODEPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ──────────────────────────────────────────────
# Examples
# ──────────────────────────────────────────────
option(NODEPP_BUILD_EXAMPLES "Build example programs" ON)
if(NODEPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ──────────────────────────────────────────────
# Install
# ──────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS nodepp
    EXPORT nodeppTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/nodepp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT nodeppTargets
    FILE nodeppTargets.cmake
    NAMESPACE nodepp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nodepp
)
