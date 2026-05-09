add_library(auramcp
  src/tool_result.cpp
  src/stdio_transport.cpp
  src/tool_registry.cpp
  src/jsonrpc/json_rpc_parser.cpp
  src/jsonrpc/router.cpp
)

target_include_directories(auramcp PUBLIC include)
target_link_libraries(auramcp PUBLIC aurahub_warnings)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(auramcp PUBLIC nlohmann_json::nlohmann_json)

add_executable(auramcp_stdio_server
  src/stdio_server_main.cpp
)
target_link_libraries(auramcp_stdio_server PRIVATE aurahub_warnings auramcp)

# Stdio test agent for MCP Assistant GUI (append-only log + diff payload).
add_executable(aurahub_test_mcp_agent
  src/test_mcp_agent_main.cpp
)
target_link_libraries(aurahub_test_mcp_agent PRIVATE aurahub_warnings auramcp)
target_compile_features(aurahub_test_mcp_agent PRIVATE cxx_std_20)

