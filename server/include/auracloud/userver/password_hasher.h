#pragma once

#include <string>

namespace auracloud::security {

struct PasswordHash final {
  std::string salt;
  std::string hash;
};

PasswordHash HashPasswordSha3_256(const std::string& password,
                                  const std::string& salt);

}  // namespace auracloud::security

