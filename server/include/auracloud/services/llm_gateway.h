#pragma once

#include <string>

namespace auracloud::services {

class LlmGateway final {
 public:
  // Accepts a JSON request; returns JSON response.
  // Real implementation will inject server-side provider keys.
  std::string Proxy(std::string request_json);
};

}  // namespace auracloud::services

