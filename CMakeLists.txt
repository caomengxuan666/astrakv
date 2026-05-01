cmake_minimum_required(VERSION 3.20)
project(astrakv
  VERSION 0.1.0
  DESCRIPTION "Embedded thread-safe key-value store with sorted sets, C API, multi-language support"
  LANGUAGES CXX C
)

option(ASTRAKV_BUILD_TESTS "Build tests" OFF)
option(ASTRAKV_BUILD_EXAMPLES "Build examples" OFF)

# CPM
include(cmake/CPM.cmake)
include(cmake/Dependencies.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library target
add_library(astrakv STATIC
  src/kv_impl.cpp
  src/kv.cpp
  src/engine.cpp
  src/eviction_2q.cpp
  src/metadata.cpp
  src/rocksdb_adapter.cpp
  src/stream_data.cpp
  src/vector_data.cpp
  src/zset/bplustree_zset.cpp
)

target_include_directories(astrakv
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(astrakv
  PUBLIC
    absl::flat_hash_map
    absl::hash
    absl::synchronization
    absl::base
    simdjson::simdjson
    hnswlib::hnswlib
  PRIVATE
    rocksdb::rocksdb
    spdlog::spdlog
)

if(ASTRAKV_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/cpp)
endif()

if(ASTRAKV_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Benchmark executable
add_executable(astrakv_bench examples/bench.cpp)
target_link_libraries(astrakv_bench PRIVATE astrakv rocksdb::rocksdb spdlog::spdlog)
