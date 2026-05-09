#pragma once

#include "auraclient/model/ilocal_db_service.h"
#include "auraclient/model/mcp_engine.h"
#include "auraclient/mvp/iaura_presenter.h"
#include "auraclient/mvp/iaura_view.h"

#include <memory>

namespace auraclient::mvp {

class AuraPresenter final : public IAuraPresenter {
 public:
  AuraPresenter(IAuraView& view, std::shared_ptr<model::ILocalDbService> db);

  void OnAppStarted() override;
  void OnDemoEditFileClicked() override;
  void OnRollbackClicked(ActionId id) override;

  void OnApprovalUiTick() override;
  void OnApprovePending(model::ApprovalRequestId id) override;
  void OnRejectPending(model::ApprovalRequestId id) override;

 private:
  void RefreshActionLog();
  void RefreshDemoPanel(const std::string& hint_utf8);
  static std::string DemoFilePath();
  void UpdatePendingApprovalsView();

  IAuraView& view_;
  std::shared_ptr<model::ILocalDbService> db_;
  model::McpEngine mcp_;
  std::string pending_fingerprint_;
};

}  // namespace auraclient::mvp

