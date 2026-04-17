cmake_minimum_required(VERSION 3.16)
project(pdk VERSION 4.0.0 LANGUAGES CXX)

# ── C++17 standard (required, no extensions) ──────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter   # legacy code has many intentionally unused params
    )
endif()

# ── Platform definition ───────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    add_compile_definitions(LINUX)
endif()

# ── Source files ──────────────────────────────────────────────────────────────
set(PDK_SOURCES
    src/base.cpp
    src/file.cpp
    src/archive.cpp
    src/jthread.cpp
    src/logger.cpp
    src/util.cpp
    src/sock.cpp
    src/tcpserver.cpp
    src/udpserver.cpp
    src/inifile.cpp
    src/qthread.cpp
)

# ── Include directories ───────────────────────────────────────────────────────
include_directories(include)

# ── Static library ────────────────────────────────────────────────────────────
add_library(pdk_static STATIC ${PDK_SOURCES})
set_target_properties(pdk_static PROPERTIES OUTPUT_NAME pdk)
target_include_directories(pdk_static PUBLIC include)
target_link_libraries(pdk_static PUBLIC pthread)

# ── Shared library ────────────────────────────────────────────────────────────
add_library(pdk_shared SHARED ${PDK_SOURCES})
set_target_properties(pdk_shared PROPERTIES
    OUTPUT_NAME pdk
    VERSION ${PROJECT_VERSION}
    SOVERSION 4
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(pdk_shared PUBLIC include)
target_link_libraries(pdk_shared PUBLIC pthread)

# ── PostgreSQL connection pool (optional) ─────────────────────────────────────
option(BUILD_WITH_POSTGRESQL "Build with PostgreSQL connection pool support" ON)
if(BUILD_WITH_POSTGRESQL)
    find_package(PostgreSQL)
    if(PostgreSQL_FOUND)
        target_sources(pdk_static PRIVATE src/pgpool.cpp)
        target_sources(pdk_shared PRIVATE src/pgpool.cpp)
        target_link_libraries(pdk_static PUBLIC PostgreSQL::PostgreSQL)
        target_link_libraries(pdk_shared PUBLIC PostgreSQL::PostgreSQL)
        target_compile_definitions(pdk_static PUBLIC PDK_HAS_POSTGRESQL)
        target_compile_definitions(pdk_shared PUBLIC PDK_HAS_POSTGRESQL)
        message(STATUS "PDK: PostgreSQL ${PostgreSQL_VERSION_STRING} enabled")
    else()
        message(WARNING "PDK: PostgreSQL not found – pgpool will not be built")
    endif()
endif()

# ── Build type defaults ───────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

# Debug flags
target_compile_options(pdk_static  PRIVATE $<$<CONFIG:Debug>:-g -O0>)
target_compile_options(pdk_shared  PRIVATE $<$<CONFIG:Debug>:-g -O0>)

# Release flags
target_compile_options(pdk_static  PRIVATE $<$<CONFIG:Release>:-O2>)
target_compile_options(pdk_shared  PRIVATE $<$<CONFIG:Release>:-O2>)

# ── Install rules ─────────────────────────────────────────────────────────────
install(TARGETS pdk_static pdk_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/pdk)

# ── GoogleTest (optional) ──────────────────────────────────────────────────────
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    enable_testing()

    add_executable(test_pgpool test/test_pgpool.cpp)
    target_link_libraries(test_pgpool PRIVATE pdk_static GTest::gtest_main)
    add_test(NAME test_pgpool COMMAND test_pgpool)
endif()

message(STATUS "PDK 4.0 – C++17 build configured (${CMAKE_BUILD_TYPE})")
