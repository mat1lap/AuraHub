#include "auracloud/services/sync_service.h"

namespace auracloud::services {

std::string SyncService::GetSettingsJson(std::string /*user_id*/) {
  return "{}";
}

void SyncService::PutSettingsJson(std::string /*user_id*/,
                                 std::string /*settings_json*/) {}

}  // namespace auracloud::services

