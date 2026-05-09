#include "auramcp/tool_registry.h"

namespace auramcp {

bool ToolRegistry::Register(ToolInfo info, ToolFn fn) {
  if (info.name.empty() || !fn) return false;
  const std::string name = info.name;
  if (!infos_.emplace(name, std::move(info)).second) return false;
  if (!tools_.emplace(name, std::move(fn)).second) return false;
  return true;
}

ToolResult ToolRegistry::Call(std::string_view name, std::string_view args) const {
  const auto it = tools_.find(std::string(name));
  if (it == tools_.end()) {
    return ToolResult::Error("unknown tool");
  }
  return it->second(args);
}

const std::unordered_map<std::string, ToolRegistry::ToolInfo>& ToolRegistry::Tools()
    const {
  return infos_;
}

}  // namespace auramcp

