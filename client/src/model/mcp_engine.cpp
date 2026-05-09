#include "auraclient/model/mcp_engine.h"

#include "auraclient/model/approval_entry.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <fstream>
#include <sstream>
#include <thread>

namespace auraclient::model {

namespace {

constexpr int kApprovalPollMs = 50;
constexpr int kApprovalWaitTimeoutMs = 900000;  // 15 minutes

bool WaitForApprovalResolution(const std::shared_ptr<ILocalDbService>& db,
                               ApprovalRequestId id, std::string* reject_reason_out,
                               bool* timed_out) {
  using clock = std::chrono::steady_clock;
  const auto deadline =
      clock::now() + std::chrono::milliseconds(kApprovalWaitTimeoutMs);
  if (timed_out) {
    *timed_out = false;
  }
  while (clock::now() < deadline) {
    const auto row = db->GetApproval(id);
    if (!row.has_value()) {
      return false;
    }
    if (row->status == kApprovalStatusApproved) {
      return true;
    }
    if (row->status == kApprovalStatusRejected) {
      if (reject_reason_out != nullptr) {
        *reject_reason_out = row->reject_reason.value_or("rejected by user");
      }
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kApprovalPollMs));
  }
  if (timed_out) {
    *timed_out = true;
  }
  return false;
}

}  // namespace

McpEngine::McpEngine(std::shared_ptr<ILocalDbService> db) : db_(std::move(db)) {}

bool McpEngine::ReadFileToString(const std::string& path, std::string* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  return true;
}

bool McpEngine::WriteStringToFile(const std::string& path,
                                 const std::string& content) {
  std::filesystem::path p(path);
  std::error_code ec;
  std::filesystem::create_directories(p.parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) return false;
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  return static_cast<bool>(out);
}

auramcp::ToolResult McpEngine::RunTool(std::string tool_name,
                                      std::string /*json_args*/) {
  // Placeholder: in real implementation this delegates to AuraMCP tool router.
  return auramcp::ToolResult::Ok("tool=" + tool_name);
}

auramcp::ToolResult McpEngine::EditFile(std::string path,
                                       std::string new_content) {
  if (!db_ || !db_->EnsureSchema()) {
    return auramcp::ToolResult::Error("db not available");
  }

  std::string previous;
  (void)ReadFileToString(path, &previous);  // missing file => empty snapshot basis

  const ApprovalRequestId req_id =
      db_->InsertApprovalRequest(path, previous, new_content);
  if (req_id == 0) {
    return auramcp::ToolResult::Error("failed to create approval request");
  }

  std::string reject_reason;
  bool timed_out = false;
  if (!WaitForApprovalResolution(db_, req_id, &reject_reason, &timed_out)) {
    if (timed_out) {
      (void)db_->ResolveApproval(
          req_id, false,
          std::optional<std::string>("timed out waiting for approval"));
      return auramcp::ToolResult::Error(
          "approval timed out (no response in allowed window)");
    }
    return auramcp::ToolResult::Error(reject_reason);
  }

  const auto approved_row = db_->GetApproval(req_id);
  if (!approved_row.has_value()) {
    return auramcp::ToolResult::Error("approval record missing after approve");
  }
  const std::string snapshot_before = approved_row->previous_content;

  const auto action_id = db_->InsertAction("edit_file", path);
  if (action_id == 0) {
    return auramcp::ToolResult::Error("failed to insert action");
  }

  if (!db_->InsertSnapshot(action_id, FileSnapshot{path, snapshot_before})) {
    (void)db_->SetActionStatus(action_id, "FAILED", "snapshot insert failed");
    return auramcp::ToolResult::Error("snapshot insert failed");
  }

  if (!WriteStringToFile(path, new_content)) {
    (void)db_->SetActionStatus(action_id, "FAILED", "write failed");
    return auramcp::ToolResult::Error("write failed");
  }

  (void)db_->SetActionStatus(action_id, "SUCCESS", std::nullopt);
  return auramcp::ToolResult::Ok("ok");
}

auramcp::ToolResult McpEngine::RollbackAction(ActionId action_id) {
  if (!db_ || !db_->EnsureSchema()) {
    return auramcp::ToolResult::Error("db not available");
  }

  const auto snap = db_->GetSnapshot(action_id);
  if (!snap.has_value()) {
    return auramcp::ToolResult::Error("no snapshot for action");
  }

  if (!WriteStringToFile(snap->path, snap->content)) {
    return auramcp::ToolResult::Error("rollback write failed");
  }

  if (!db_->SetActionStatus(action_id, "ROLLED_BACK", std::nullopt)) {
    return auramcp::ToolResult::Error("rolled back, but status update failed");
  }

  return auramcp::ToolResult::Ok("ok");
}

}  // namespace auraclient::model

