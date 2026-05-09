#include "auramcp/tool_result.h"

namespace auramcp {

ToolResult ToolResult::Ok(std::string content) {
  ToolResult r;
  r.is_error = false;
  r.content = std::move(content);
  return r;
}

ToolResult ToolResult::Error(std::string message) {
  ToolResult r;
  r.is_error = true;
  r.content = std::move(message);
  return r;
}

}  // namespace auramcp

