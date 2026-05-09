#pragma once

#include "auraclient/model/action_log_entry.h"
#include "auraclient/model/approval_entry.h"

#include <optional>
#include <string>
#include <vector>

namespace auraclient::model {

struct FileSnapshot final {
  std::string path;
  std::string content;
};

class ILocalDbService {
 public:
  virtual ~ILocalDbService() = default;

  virtual bool EnsureSchema() = 0;

  virtual ActionId InsertAction(std::string tool, std::string target) = 0;
  virtual bool SetActionStatus(ActionId id, std::string status,
                               std::optional<std::string> error) = 0;

  virtual bool InsertSnapshot(ActionId action_id, FileSnapshot snapshot) = 0;
  virtual std::optional<FileSnapshot> GetSnapshot(ActionId action_id) const = 0;

  virtual std::vector<ActionLogEntry> ListRecentActions(int limit) const = 0;

  /// Creates a pending approval row (GUI / ResolveApproval must approve or reject).
  virtual ApprovalRequestId InsertApprovalRequest(std::string path,
                                                  std::string previous_content,
                                                  std::string proposed_content) = 0;

  virtual std::optional<ApprovalEntry> GetApproval(ApprovalRequestId id) const = 0;

  /// Marks a pending request approved or rejected; no-op if not pending.
  virtual bool ResolveApproval(ApprovalRequestId id, bool approve,
                               std::optional<std::string> reason) = 0;

  virtual std::vector<ApprovalEntry> ListPendingApprovals() const = 0;
};

}  // namespace auraclient::model

