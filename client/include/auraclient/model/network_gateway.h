#pragma once

#include <string>

namespace auraclient::model {

class NetworkGateway final {
 public:
  // Placeholder: real version will be a REST client to AuraCloud.
  std::string GetHealth();
};

}  // namespace auraclient::model

