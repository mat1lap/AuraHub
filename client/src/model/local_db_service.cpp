#include "auraclient/model/local_db_service.h"

namespace auraclient::model {

bool LocalDbService::EnsureSchema() { return true; }

ActionId LocalDbService::InsertAction(std::string /*tool*/,
                                     std::string /*target*/) {
  return next_id_++;
}

bool LocalDbService::SetActionStatus(ActionId /*id*/, std::string /*status*/,
                                     std::optional<std::string> /*error*/) {
  return true;
}

bool LocalDbService::InsertSnapshot(ActionId /*action_id*/,
                                   FileSnapshot snapshot) {
  last_snapshot_ = std::move(snapshot);
  return true;
}

std::optional<FileSnapshot> LocalDbService::GetSnapshot(ActionId /*id*/) const {
  return last_snapshot_;
}

std::vector<ActionLogEntry> LocalDbService::ListRecentActions(int /*limit*/) const {
  return {};
}

ApprovalRequestId LocalDbService::InsertApprovalRequest(std::string path,
                                                      std::string previous_content,
                                                      std::string proposed_content) {
  ApprovalEntry e;
  e.id = next_approval_id_++;
  e.created_utc_ms = 0;
  e.path = std::move(path);
  e.previous_content = std::move(previous_content);
  e.proposed_content = std::move(proposed_content);
  // Non-interactive stub: auto-approve so McpEngine proceeds without a GUI.
  e.status = kApprovalStatusApproved;
  approvals_[e.id] = e;
  return e.id;
}

std::optional<ApprovalEntry> LocalDbService::GetApproval(ApprovalRequestId id) const {
  const auto it = approvals_.find(id);
  if (it == approvals_.end()) return std::nullopt;
  return it->second;
}

bool LocalDbService::ResolveApproval(ApprovalRequestId id, bool approve,
                                   std::optional<std::string> reason) {
  auto it = approvals_.find(id);
  if (it == approvals_.end()) return false;
  if (it->second.status != kApprovalStatusPending) return false;
  it->second.status =
      approve ? kApprovalStatusApproved : kApprovalStatusRejected;
  if (!approve && reason.has_value()) {
    it->second.reject_reason = *reason;
  }
  return true;
}

std::vector<ApprovalEntry> LocalDbService::ListPendingApprovals() const {
  return {};
}

}  // namespace auraclient::model

