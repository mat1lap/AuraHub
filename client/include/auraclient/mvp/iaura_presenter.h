#pragma once

#include "auraclient/model/approval_entry.h"

#include <cstdint>

namespace auraclient::mvp {

using ActionId = std::int64_t;

class IAuraPresenter {
 public:
  virtual ~IAuraPresenter() = default;

  virtual void OnAppStarted() = 0;
  virtual void OnDemoEditFileClicked() = 0;
  virtual void OnRollbackClicked(ActionId id) = 0;

  virtual void OnApprovalUiTick() = 0;
  virtual void OnApprovePending(model::ApprovalRequestId id) = 0;
  virtual void OnRejectPending(model::ApprovalRequestId id) = 0;
};

}  // namespace auraclient::mvp

