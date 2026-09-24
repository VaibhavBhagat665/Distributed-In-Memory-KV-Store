cmake_minimum_required(VERSION 3.15)
project(kvstore_engine CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warning flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fsanitize=address -fsanitize=undefined")

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Source files (will expand as we add components)
set(ENGINE_SOURCES
    src/engine.cpp
    src/hash_table.cpp
    src/lru_policy.cpp
    src/lfu_policy.cpp
    src/sharded_engine.cpp
    src/wal.cpp
    src/snapshot.cpp
    src/resp.cpp
    src/epoll_server.cpp
)

set(ENGINE_HEADERS
    include/engine.h
    include/hash_table.h
    include/lru_policy.h
    include/lfu_policy.h
    include/sharded_engine.h
    include/wal.h
    include/snapshot.h
    include/resp.h
    include/network_server.h
    include/epoll_server.h
)

# Main engine library
add_library(engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
target_include_directories(engine PUBLIC include)

# Testing (RapidCheck for property-based testing)
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
option(BUILD_BENCHMARKS "Build benchmarks" ON)
if(BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()
