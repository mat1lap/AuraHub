#pragma once

#include "auraclient/model/ilocal_db_service.h"

#include <QSqlDatabase>

#include <optional>
#include <string>
#include <vector>

namespace auraclient::model {

class LocalDbServiceQt final : public ILocalDbService {
 public:
  LocalDbServiceQt();
  ~LocalDbServiceQt() override;

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
  bool EnsureOpen();
  bool Exec(const char* sql) const;

  // Connection name allows multiple DBs in process if needed.
  QString connection_name_;
  mutable QSqlDatabase db_;
};

}  // namespace auraclient::model

