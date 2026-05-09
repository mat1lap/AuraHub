#include "auramcp/jsonrpc/router.h"

#include "auramcp/jsonrpc/json_rpc_parser.h"

#include <nlohmann/json.hpp>

namespace auramcp::jsonrpc {
namespace {

std::string MakeResult(std::string_view id_json, const nlohmann::json& result) {
  nlohmann::json j;
  j["jsonrpc"] = "2.0";
  try {
    j["id"] = nlohmann::json::parse(std::string(id_json));
  } catch (...) {
    j["id"] = nullptr;
  }
  j["result"] = result;
  return j.dump();
}

std::string MakeError(std::string_view id_json, int code,
                      std::string_view message) {
  nlohmann::json j;
  j["jsonrpc"] = "2.0";
  try {
    j["id"] = nlohmann::json::parse(std::string(id_json));
  } catch (...) {
    j["id"] = nullptr;
  }
  j["error"] = {{"code", code}, {"message", message}};
  return j.dump();
}

}  // namespace

Router::Router(const ToolRegistry& registry) : registry_(registry) {}

std::string Router::Handle(std::string_view request_json) const {
  ToolResult parse_error;
  const auto parsed = JsonRpcParser::Parse(request_json, &parse_error);
  if (!parsed.has_value()) {
    return parse_error.content;
  }

  if (parsed->method == "tools/list") {
    return HandleToolsList(parsed->id_json);
  }
  if (parsed->method == "tools/call") {
    return HandleToolsCall(parsed->id_json, parsed->params_json);
  }

  return MakeError(parsed->id_json, -32601, "Method not found");
}

std::string Router::HandleToolsList(std::string_view id_json) const {
  nlohmann::json tools = nlohmann::json::array();
  for (const auto& [name, info] : registry_.Tools()) {
    nlohmann::json t;
    t["name"] = info.name;
    t["description"] = info.description;
    // inputSchema is required by MCP clients; keep it minimal for now.
    if (!info.input_schema_json.empty()) {
      try {
        t["inputSchema"] = nlohmann::json::parse(info.input_schema_json);
      } catch (...) {
        t["inputSchema"] = nlohmann::json::object();
      }
    } else {
      t["inputSchema"] = nlohmann::json::object();
    }
    tools.push_back(std::move(t));
  }
  return MakeResult(id_json, nlohmann::json{{"tools", std::move(tools)}});
}

std::string Router::HandleToolsCall(std::string_view id_json,
                                    std::string_view params_json) const {
  try {
    const auto p = nlohmann::json::parse(std::string(params_json));
    if (!p.is_object()) return MakeError(id_json, -32602, "Invalid params");

    const auto name_it = p.find("name");
    if (name_it == p.end() || !name_it->is_string()) {
      return MakeError(id_json, -32602, "Invalid params");
    }
    const std::string name = name_it->get<std::string>();

    // "arguments" can be any JSON; pass raw string to tool.
    const auto args_it = p.find("arguments");
    const std::string args = (args_it == p.end()) ? "null" : args_it->dump();

    const auto r = registry_.Call(name, args);
    if (r.is_error) {
      return MakeError(id_json, -32000, r.content);
    }
    return MakeResult(id_json, nlohmann::json{{"content", r.content}});
  } catch (...) {
    return MakeError(id_json, -32602, "Invalid params");
  }
}

}  // namespace auramcp::jsonrpc

