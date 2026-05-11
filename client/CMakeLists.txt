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

  find_package(Qt6 QUIET COMPONENTS Concurrent Network WebSockets)
  if(TARGET Qt6::Concurrent AND TARGET Qt6::Network AND TARGET Qt6::WebSockets)
    add_executable(aurahub_mcp_assistant
      src/mcp_assistant/main.cpp
      src/mcp_assistant/app/application.cpp
      src/mcp_assistant/chat/chat_delegate.cpp
      src/mcp_assistant/chat/chat_messages_model.cpp
      src/mcp_assistant/checkpoint/checkpoint_list_model.cpp
      src/mcp_assistant/checkpoint/checkpoint_manager.cpp
      src/mcp_assistant/mcp/mcp_client.cpp
      src/mcp_assistant/mcp/stdio_mcp_transport.cpp
      src/mcp_assistant/mcp/websocket_mcp_transport.cpp
      src/mcp_assistant/services/assistant_session_controller.cpp
      src/mcp_assistant/views/main_window.cpp
      src/mcp_assistant/widgets/diff_preview_widget.cpp
      resources/mcp_assistant.qrc
      include/mcp_assistant/mcp/imcp_transport.h
      include/mcp_assistant/mcp/stdio_mcp_transport.h
      include/mcp_assistant/mcp/websocket_mcp_transport.h
      include/mcp_assistant/mcp/mcp_client.h
      include/mcp_assistant/chat/chat_messages_model.h
      include/mcp_assistant/chat/chat_delegate.h
      include/mcp_assistant/checkpoint/checkpoint_list_model.h
      include/mcp_assistant/checkpoint/checkpoint_manager.h
      include/mcp_assistant/services/assistant_session_controller.h
      include/mcp_assistant/views/main_window.h
      include/mcp_assistant/widgets/diff_preview_widget.h
    )
    target_include_directories(aurahub_mcp_assistant PRIVATE include)
    target_link_libraries(aurahub_mcp_assistant PRIVATE
      aurahub_warnings
      Qt6::Widgets
      Qt6::Concurrent
      Qt6::Network
      Qt6::WebSockets
    )
    target_compile_features(aurahub_mcp_assistant PRIVATE cxx_std_20)
  else()
    message(STATUS "Qt Concurrent/Network/WebSockets not found; aurahub_mcp_assistant skipped")
  endif()
endif()

