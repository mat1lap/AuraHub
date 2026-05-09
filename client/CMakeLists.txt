add_executable(auraclient
  src/main.cpp
  src/model/mcp_engine.cpp
  src/model/local_db_service.cpp
  src/model/network_gateway.cpp
)

target_include_directories(auraclient PRIVATE include)
target_link_libraries(auraclient PRIVATE aurahub_warnings auramcp)

# Optional Qt GUI target.
find_package(Qt6 COMPONENTS Widgets Sql QUIET)
if(Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
  set(CMAKE_AUTOUIC ON)

  add_executable(auraclient_gui
    src/gui/main_gui.cpp
    src/gui/main_window.cpp
    src/gui/action_log_table_model.cpp
    src/mvp/aura_presenter.cpp
    src/model/mcp_engine.cpp
    src/model/local_db_service_qt.cpp
  )

  target_include_directories(auraclient_gui PRIVATE include)
  target_link_libraries(auraclient_gui PRIVATE
    aurahub_warnings
    auramcp
    Qt6::Widgets
    Qt6::Sql
  )

  # MCP host process (runs JSON-RPC over stdio, but backed by client DB + FS).
  add_executable(auraclient_mcp_host
    src/mcp_host_main.cpp
    src/model/mcp_engine.cpp
    src/model/local_db_service_qt.cpp
  )
  target_include_directories(auraclient_mcp_host PRIVATE include)
  target_link_libraries(auraclient_mcp_host PRIVATE
    aurahub_warnings
    auramcp
    Qt6::Sql
    Qt6::Core
  )
endif()

