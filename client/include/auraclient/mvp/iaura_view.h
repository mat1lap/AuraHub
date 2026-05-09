#pragma once

#include "auraclient/model/action_log_entry.h"
#include "auraclient/model/approval_entry.h"

#include <cstdint>
#include <vector>

namespace auraclient::mvp {

using ActionId = std::int64_t;

class IAuraView {
 public:
  virtual ~IAuraView() = default;

  virtual void SetRollbackEnabled(bool enabled) = 0;
  virtual void ShowStatusMessage(const char* message) = 0;
  virtual void SetActionLogRows(std::vector<model::ActionLogEntry> rows) = 0;

  /// Updates the demo area: path to the demo file, current on-disk preview, short hint (UTF-8).
  virtual void SetDemoPanel(const char* absolute_path_utf8, const char* file_preview_utf8,
                            const char* hint_utf8) = 0;

  virtual void SetPendingApprovals(std::vector<model::ApprovalEntry> rows) = 0;

  /// Disables demo write while an edit is blocked on human approval or I/O.
  virtual void SetDemoEditInProgress(bool in_progress) = 0;
};

}  // namespace auraclient::mvp

