#pragma once

#include <string>

namespace auramcp {

// AuraMCP core is non-throwing by design.
// Runtime errors must be represented as ToolResult{is_error=true}.
struct ToolResult final {
  bool is_error{false};
  std::string content;

  static ToolResult Ok(std::string content);
  static ToolResult Error(std::string message);
};

}  // namespace auramcp

