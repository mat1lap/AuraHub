#pragma once

#include "auraclient/model/ilocal_db_service.h"

#include <unordered_map>

namespace auraclient::model {

class LocalDbService final : public ILocalDbService {
 public:
  bool EnsureSchema() override;

  ActionId InsertAction(std::string tool, std::string target) override;
  bool SetActionStatus(ActionId id, std::string status,
                       std::optional<std::string> error) override;

  bool InsertSnapshot(ActionId action_id, FileSnapshot snapshot) override;
  std::optional<FileSnapshot> GetSnapshot(ActionId action_id) const override;

  std::vector<ActionLogEntry> ListRecentActions(int limit) const override;

  ApprovalRequestId InsertApprovalRequest(std::string path,
                                          std::string previous_content,
                                          std::string proposed_content) override;
  std::optional<ApprovalEntry> GetApproval(ApprovalRequestId id) const override;
  bool ResolveApproval(ApprovalRequestId id, bool approve,
                       std::optional<std::string> reason) override;
  std::vector<ApprovalEntry> ListPendingApprovals() const override;

 private:
  ActionId next_id_{1};
  ApprovalRequestId next_approval_id_{1};
  std::optional<FileSnapshot> last_snapshot_;
  std::unordered_map<ApprovalRequestId, ApprovalEntry> approvals_;
};

}  // namespace auraclient::model

