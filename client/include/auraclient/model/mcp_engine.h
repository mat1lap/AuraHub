#pragma once

#include "auramcp/tool_result.h"
#include "auraclient/model/ilocal_db_service.h"

#include <memory>
#include <string>

namespace auraclient::model {

class McpEngine final {
 public:
  explicit McpEngine(std::shared_ptr<ILocalDbService> db);

  // Generic entrypoint (future JSON-RPC / tool router).
  auramcp::ToolResult RunTool(std::string tool_name, std::string json_args);

  // First real tool: edit a file with Time Machine hooks.
  auramcp::ToolResult EditFile(std::string path, std::string new_content);
  auramcp::ToolResult RollbackAction(ActionId action_id);

 private:
  static bool ReadFileToString(const std::string& path, std::string* out);

 public:
  // Exposed for rollback path until rollback becomes a dedicated tool.
  static bool WriteStringToFile(const std::string& path,
                                const std::string& content);

 private:
  std::shared_ptr<ILocalDbService> db_;
};

}  // namespace auraclient::model

