#pragma once

#include <string>

namespace auracloud::services {

class SyncService final {
 public:
  std::string GetSettingsJson(std::string user_id);
  void PutSettingsJson(std::string user_id, std::string settings_json);
};

}  // namespace auracloud::services

