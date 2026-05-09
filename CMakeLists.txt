cmake_minimum_required(VERSION 3.22)

project(AuraHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AURAHUB_BUILD_CLIENT "Build AuraClient (Qt)" ON)
option(AURAHUB_BUILD_SERVER "Build AuraCloud (userver)" ON)
option(AURAHUB_BUILD_MCP "Build AuraMCP core" ON)
option(AURAHUB_BUILD_TESTS "Build unit tests" ON)

add_library(aurahub_warnings INTERFACE)
target_compile_options(aurahub_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(AURAHUB_BUILD_MCP)
  add_subdirectory(mcp)
endif()

if(AURAHUB_BUILD_CLIENT)
  add_subdirectory(client)
endif()

if(AURAHUB_BUILD_SERVER)
  add_subdirectory(server)
endif()

if(AURAHUB_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

