#include "auracloud/services/llm_gateway.h"

namespace auracloud::services {

std::string LlmGateway::Proxy(std::string /*request_json*/) {
  return R"({"error":"not-implemented"})";
}

}  // namespace auracloud::services

