#include "auramcp/jsonrpc/json_rpc_parser.h"

#include <nlohmann/json.hpp>

namespace auramcp::jsonrpc {
namespace {

std::string MakeError(std::string_view id_json, int code,
                      std::string_view message) {
  nlohmann::json j;
  j["jsonrpc"] = "2.0";
  if (id_json == "missing") {
    j["id"] = nullptr;
  } else {
    // id_json is expected to be valid json already; if it's not, default to null.
    try {
      j["id"] = nlohmann::json::parse(std::string(id_json));
    } catch (...) {
      j["id"] = nullptr;
    }
  }
  j["error"] = {{"code", code}, {"message", message}};
  return j.dump();
}

}  // namespace

std::optional<ParsedRequest> JsonRpcParser::Parse(std::string_view input,
                                                 ToolResult* error_response) {
  if (!error_response) return std::nullopt;

  try {
    const auto j = nlohmann::json::parse(input.begin(), input.end());
    if (!j.is_object()) {
      *error_response =
          ToolResult::Error(MakeError("missing", -32600, "Invalid Request"));
      return std::nullopt;
    }

    const auto jsonrpc_it = j.find("jsonrpc");
    if (jsonrpc_it == j.end() || !jsonrpc_it->is_string() ||
        jsonrpc_it->get<std::string>() != "2.0") {
      *error_response =
          ToolResult::Error(MakeError("missing", -32600, "Invalid Request"));
      return std::nullopt;
    }

    ParsedRequest out;

    const auto id_it = j.find("id");
    if (id_it == j.end()) {
      out.id_json = "null";
    } else {
      out.id_json = id_it->dump();
    }

    const auto method_it = j.find("method");
    if (method_it == j.end() || !method_it->is_string()) {
      *error_response =
          ToolResult::Error(MakeError(out.id_json, -32600, "Invalid Request"));
      return std::nullopt;
    }
    out.method = method_it->get<std::string>();

    const auto params_it = j.find("params");
    out.params_json = (params_it == j.end()) ? "null" : params_it->dump();

    return out;
  } catch (...) {
    *error_response =
        ToolResult::Error(MakeError("missing", -32700, "Parse error"));
    return std::nullopt;
  }
}

}  // namespace auramcp::jsonrpc

