#pragma once

#include "auramcp/tool_registry.h"

#include <string>
#include <string_view>

namespace auramcp::jsonrpc {

class Router final {
 public:
  explicit Router(const ToolRegistry& registry);

  // Returns a JSON-RPC response string.
  std::string Handle(std::string_view request_json) const;

 private:
  std::string HandleToolsList(std::string_view id_json) const;
  std::string HandleToolsCall(std::string_view id_json,
                              std::string_view params_json) const;

  const ToolRegistry& registry_;
};

}  // namespace auramcp::jsonrpc

