#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace auraclient::model {

using ApprovalRequestId = std::int64_t;

inline constexpr char kApprovalStatusPending[] = "PENDING";
inline constexpr char kApprovalStatusApproved[] = "APPROVED";
inline constexpr char kApprovalStatusRejected[] = "REJECTED";

struct ApprovalEntry final {
  ApprovalRequestId id = 0;
  std::int64_t created_utc_ms = 0;
  std::string path;
  std::string previous_content;
  std::string proposed_content;
  std::string status;
  std::optional<std::string> reject_reason;
};

}  // namespace auraclient::model
