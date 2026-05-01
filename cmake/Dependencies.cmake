# Abseil - high-performance containers + synchronization
CPMAddPackage(
  NAME abseil
  VERSION 20240116.1
  GITHUB_REPOSITORY abseil/abseil-cpp
  GIT_TAG 20240116.1
  OPTIONS
    "ABSL_ENABLE_INSTALL OFF"
    "ABSL_PROPAGATE_CXX_STD OFF"
    "BUILD_TESTING OFF"
)

# spdlog - fast C++ logging
CPMAddPackage(
  NAME spdlog
  VERSION 1.13.0
  GITHUB_REPOSITORY gabime/spdlog
  GIT_TAG v1.13.0
  OPTIONS
    "SPDLOG_FMT_EXTERNAL OFF"
    "SPDLOG_BUILD_SHARED OFF"
    "SPDLOG_BUILD_TESTS OFF"
    "SPDLOG_BUILD_EXAMPLE OFF"
)

# RocksDB - persistent storage backend
CPMAddPackage(
  NAME rocksdb
  VERSION 10.10.1
  URL https://github.com/facebook/rocksdb/archive/refs/tags/v10.10.1.tar.gz
  OPTIONS
    "WITH_TESTS OFF"
    "WITH_BENCHMARK_TOOLS OFF"
    "WITH_TOOLS OFF"
    "WITH_CORETOOLS OFF"
    "WITH_FATAL_ERROR_HANDLER OFF"
    "WITH_XPRESS OFF"
    "WITH_ZSTD OFF"
    "WITH_LZ4 OFF"
    "WITH_SNAPPY OFF"
    "WITH_GFLAGS OFF"
    "USE_RTTI ON"
    "ROCKSDB_BUILD_SHARED OFF"
    "ROCKSDB_INSTALL ON"
    "FAIL_ON_WARNINGS OFF"
)

# simdjson - ultra-fast SIMD-accelerated JSON parser
CPMAddPackage(
  NAME simdjson
  VERSION 3.10.0
  GITHUB_REPOSITORY simdjson/simdjson
  GIT_TAG v3.10.0
  OPTIONS
    "SIMDJSON_DEVELOPER_MODE OFF"
    "SIMDJSON_TESTS OFF"
    "SIMDJSON_TOOLS OFF"
)

if(rocksdb_ADDED AND TARGET rocksdb)
  if(MSVC)
  else()
    target_compile_options(rocksdb PRIVATE -Wno-error)
  endif()
  if(NOT TARGET rocksdb::rocksdb)
    add_library(rocksdb::rocksdb ALIAS rocksdb)
  endif()
endif()

# hnswlib - HNSW vector similarity search
CPMAddPackage(
  NAME hnswlib
  VERSION 0.8.0
  GITHUB_REPOSITORY nmslib/hnswlib
  GIT_TAG v0.8.0
)

# Google Test (only if tests enabled)

if(ASTRAKV_BUILD_TESTS)
  CPMAddPackage(
    NAME googletest
    VERSION 1.14.0
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    OPTIONS
      "BUILD_GMOCK ON"
      "INSTALL_GTEST OFF"
      "gtest_force_shared_crt OFF"
  )
endif()
