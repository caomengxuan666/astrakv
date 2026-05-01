add_executable(astrakv_test
  kv_test.cpp
)
target_link_libraries(astrakv_test PRIVATE astrakv gtest_main)
target_include_directories(astrakv_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
include(GoogleTest)
gtest_discover_tests(astrakv_test)
