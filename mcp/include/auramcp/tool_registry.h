#pragma once

#include "auramcp/tool_result.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace auramcp {

// Minimal registry. Must be built at init time and treated as immutable during
// execution to keep the hot path lock-free.
class ToolRegistry final {
 public:
  using ToolFn = std::function<ToolResult(std::string_view args)>;

  struct ToolInfo final {
    std::string name;
    std::string description;
    // Placeholder for future JSON schema.
    std::string input_schema_json;
  };

  bool Register(ToolInfo info, ToolFn fn);
  ToolResult Call(std::string_view name, std::string_view args) const;
  const std::unordered_map<std::string, ToolInfo>& Tools() const;

 private:
  std::unordered_map<std::string, ToolInfo> infos_;
  std::unordered_map<std::string, ToolFn> tools_;
};

}  // namespace auramcp

