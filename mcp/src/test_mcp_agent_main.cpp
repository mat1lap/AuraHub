// Stdio JSON-RPC "test agent": on messages/prompt appends to a workspace file and returns
// assistant_markdown + optional diff for the MCP Assistant GUI approval flow.
//
// Env:
//   AURA_TEST_AGENT_FILE  — relative path under workspace (default: aura_test_agent.log)
//   AURA_WORKSPACE        — absolute root for file paths (default: process cwd)
//
// Methods:
//   initialize / initialized — minimal MCP-style handshake (no-op for notifications)
//   messages/prompt          — params: { "text": "..." }

#include "auramcp/jsonrpc/json_rpc_parser.h"
#include "auramcp/tool_result.h"

#include <nlohmann/json.hpp>

#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

namespace fs = std::filesystem;

std::string Trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
    s.remove_suffix(1);
  }
  return std::string(s);
}

std::string ReadFileOrEmpty(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) {
    return {};
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

void AppendUtf8Line(const fs::path& p, std::string_view line) {
  std::ofstream out(p, std::ios::app | std::ios::binary);
  out.write(line.data(), static_cast<std::streamsize>(line.size()));
}

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

std::string MakeError(std::string_view id_json, int code, std::string_view message) {
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

fs::path WorkspaceRoot() {
  if (const char* w = std::getenv("AURA_WORKSPACE")) {
    return fs::path(w);
  }
  return fs::current_path();
}

fs::path TargetLogPath() {
  const char* rel = std::getenv("AURA_TEST_AGENT_FILE");
  const std::string file =
      (rel != nullptr && rel[0] != '\0') ? std::string(rel) : std::string("aura_test_agent.log");
  const fs::path p(file);
  if (p.is_absolute()) {
    return p;
  }
  return WorkspaceRoot() / p;
}

std::string RelPathForUi(const fs::path& absolute_path) {
  std::error_code ec;
  const fs::path base = fs::weakly_canonical(WorkspaceRoot(), ec);
  const fs::path abs = fs::weakly_canonical(absolute_path, ec);
  const fs::path rel = fs::relative(abs, base, ec);
  if (!ec && !rel.empty()) {
    for (const auto& part : rel) {
      if (part == "..") {
        return abs.filename().string();
      }
    }
    return rel.generic_string();
  }
  return abs.filename().string();
}

constexpr std::size_t kMaxDiffChars = 12000;

std::string TruncateForJson(std::string s) {
  if (s.size() > kMaxDiffChars) {
    s.resize(kMaxDiffChars);
    s.append("\n… [truncated for preview]");
  }
  return s;
}

std::string Iso8601Utc() {
  const std::time_t t = std::time(nullptr);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[64]{};
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return std::string(buf);
}

std::string HandlePrompt(std::string_view id_json, std::string_view params_json) {
  try {
    const auto params = nlohmann::json::parse(std::string(params_json));
    if (!params.is_object()) {
      return MakeError(id_json, -32602, "Invalid params");
    }
    const std::string user_text = params.value("text", "");
    const fs::path path = TargetLogPath();
    const fs::path parent = path.parent_path();
    if (!parent.empty()) {
      fs::create_directories(parent);
    }

    const std::string before = ReadFileOrEmpty(path);
    const std::string stamp = Iso8601Utc();
    const std::string line = "[" + stamp + "] " + user_text + "\n";
    AppendUtf8Line(path, line);
    const std::string after = ReadFileOrEmpty(path);

    const std::string rel_ui = RelPathForUi(path);

    nlohmann::json files = nlohmann::json::array();
    nlohmann::json f;
    f["path"] = rel_ui;
    f["before"] = TruncateForJson(before);
    f["after"] = TruncateForJson(after);
    files.push_back(std::move(f));

    nlohmann::json diff;
    diff["title"] = "Test agent log append";
    diff["files"] = std::move(files);

    nlohmann::json result;
    result["assistant_markdown"] =
        std::string("**Test agent** appended one line to `" + rel_ui + "`.\n\n") +
        "Timestamp: `" + stamp + "`\n\n"
                                 "Use **Accept** to create a checkpoint (GUI), or reject.";
    result["modified_files"] = nlohmann::json::array({rel_ui});
    result["needs_approval"] = true;
    result["diff"] = std::move(diff);

    return MakeResult(id_json, result);
  } catch (...) {
    return MakeError(id_json, -32603, "Internal error");
  }
}

std::string HandleInitialize(std::string_view id_json, std::string_view /*params_json*/) {
  nlohmann::json result;
  result["protocolVersion"] = "2024-11-05";
  result["capabilities"] = nlohmann::json::object();
  result["serverInfo"] = {{"name", "aurahub_test_mcp_agent"}, {"version", "0.1.0"}};
  return MakeResult(id_json, result);
}

}  // namespace

int main() {
  std::ios::sync_with_stdio(false);
  std::string line;
  while (std::getline(std::cin, line)) {
    line = Trim(line);
    if (line.empty()) {
      continue;
    }

    auramcp::ToolResult parse_err;
    const auto parsed = auramcp::jsonrpc::JsonRpcParser::Parse(line, &parse_err);
    if (!parsed.has_value()) {
      std::cout << parse_err.content << std::endl;
      continue;
    }

    if (parsed->method == "notifications/initialized" ||
        parsed->method == "initialized") {
      if (parsed->id_json != "null") {
        std::cout << MakeResult(parsed->id_json, nlohmann::json::object()) << std::endl;
      }
      continue;
    }

    if (parsed->method == "initialize") {
      std::cout << HandleInitialize(parsed->id_json, parsed->params_json) << std::endl;
      continue;
    }
    if (parsed->method == "messages/prompt") {
      std::cout << HandlePrompt(parsed->id_json, parsed->params_json) << std::endl;
      continue;
    }
    if (parsed->method == "ping") {
      std::cout << MakeResult(parsed->id_json, nlohmann::json{{"pong", true}}) << std::endl;
      continue;
    }

    std::cout << MakeError(parsed->id_json, -32601, "Method not found") << std::endl;
  }
  return 0;
}
