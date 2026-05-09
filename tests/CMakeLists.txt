include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.16.0
)
FetchContent_MakeAvailable(googletest)

add_executable(aurahub_tests
  src/mcp_jsonrpc_test.cpp
  src/mcp_router_test.cpp
  src/mcp_registry_test.cpp
)

target_link_libraries(aurahub_tests PRIVATE
  aurahub_warnings
  auramcp
  GTest::gtest
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(aurahub_tests)

# Integration test: run stdio server and perform a tools/call to echo.
add_test(
  NAME auramcp_stdio_echo_integration
  COMMAND bash -lc
    "printf '%s\n' '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"echo\",\"arguments\":{\"hello\":\"world\"}}}' | \"${CMAKE_BINARY_DIR}/mcp/auramcp_stdio_server\" | head -n 1"
)

