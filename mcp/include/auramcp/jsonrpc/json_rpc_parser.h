#pragma once

#include "auramcp/tool_result.h"

#include <optional>
#include <string>
#include <string_view>

namespace auramcp::jsonrpc {

// Minimal JSON-RPC 2.0 parser that converts input into the subset of requests
// AuraMCP needs. Parsing is non-throwing: failures return ToolResult::Error
// containing a JSON-RPC error response body.
struct ParsedRequest final {
  std::string id_json;     // raw json for id (e.g. "1", "\"abc\"", "null")
  std::string method;      // e.g. "tools/list", "tools/call"
  std::string params_json; // raw json object or "null"
};

class JsonRpcParser final {
 public:
  static std::optional<ParsedRequest> Parse(std::string_view input,
                                           ToolResult* error_response);
};

}  // namespace auramcp::jsonrpc

