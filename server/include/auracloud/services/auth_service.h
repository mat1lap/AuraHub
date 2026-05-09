#pragma once

#include <string>

namespace auracloud::services {

class AuthService final {
 public:
  // Minimal working implementation:
  // - file-backed storage (stand-in for Postgres)
  // - password hashing via OpenSSL SHA3-256 when available
  // - returns JSON responses
  explicit AuthService(std::string storage_path);

  std::string Register(std::string email, std::string password);  // JSON
  std::string Login(std::string email, std::string password);     // JSON

 private:
  std::string storage_path_;
};

}  // namespace auracloud::services

