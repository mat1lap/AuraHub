#include "auraclient/model/local_db_service_qt.h"
#include "auraclient/model/mcp_engine.h"
#include "auramcp/jsonrpc/router.h"
#include "auramcp/tool_registry.h"
#include "auramcp/transport/stdio_transport.h"

#include <nlohmann/json.hpp>

#include <QCoreApplication>

#include <iostream>
#include <memory>
#include <string>

namespace {

auramcp::ToolResult ToolEditFile(auraclient::model::McpEngine& mcp,
                                std::string_view args_json) {
  try {
    const auto j = nlohmann::json::parse(std::string(args_json));
    if (!j.is_object()) return auramcp::ToolResult::Error("invalid args");
    if (!j.contains("path") || !j["path"].is_string()) {
      return auramcp::ToolResult::Error("missing path");
    }
    if (!j.contains("content") || !j["content"].is_string()) {
      return auramcp::ToolResult::Error("missing content");
    }
    return mcp.EditFile(j["path"].get<std::string>(),
                        j["content"].get<std::string>());
  } catch (...) {
    return auramcp::ToolResult::Error("invalid json args");
  }
}

auramcp::ToolResult ToolRollback(auraclient::model::McpEngine& mcp,
                                std::string_view args_json) {
  try {
    const auto j = nlohmann::json::parse(std::string(args_json));
    if (!j.is_object()) return auramcp::ToolResult::Error("invalid args");
    if (!j.contains("action_id") || !j["action_id"].is_number_integer()) {
      return auramcp::ToolResult::Error("missing action_id");
    }
    return mcp.RollbackAction(j["action_id"].get<long long>());
  } catch (...) {
    return auramcp::ToolResult::Error("invalid json args");
  }
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication::setApplicationName(QStringLiteral("AuraHub"));
  QCoreApplication::setOrganizationName(QStringLiteral("AuraHub"));

  QCoreApplication app(argc, argv);

  auto db = std::make_shared<auraclient::model::LocalDbServiceQt>();
  auraclient::model::McpEngine mcp(db);

  auramcp::ToolRegistry registry;
  registry.Register(
      auramcp::ToolRegistry::ToolInfo{
          .name = "edit_file",
          .description = "Edits a local file with pre-snapshot for rollback.",
          .input_schema_json =
              R"({"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]})"},
      [&](std::string_view args) { return ToolEditFile(mcp, args); });

  registry.Register(
      auramcp::ToolRegistry::ToolInfo{
          .name = "rollback_action",
          .description = "Restores a file using snapshot stored for action_id.",
          .input_schema_json =
              R"({"type":"object","properties":{"action_id":{"type":"integer"}},"required":["action_id"]})"},
      [&](std::string_view args) { return ToolRollback(mcp, args); });

  const auramcp::jsonrpc::Router router(registry);
  auramcp::transport::StdioTransport transport([&](std::string_view msg) {
    const std::string response = router.Handle(msg);
    std::cout << response << std::endl;
  });

  transport.Start();  // blocking
  return 0;
}

