#pragma once

#include <cstdint>
#include <string>

namespace auraclient::model {

using ActionId = std::int64_t;

struct ActionLogEntry final {
  ActionId id{0};
  std::int64_t ts_utc_ms{0};
  std::string tool;
  std::string target;
  std::string status;
  std::string error;
};

}  // namespace auraclient::model

