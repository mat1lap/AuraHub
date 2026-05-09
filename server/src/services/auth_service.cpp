#include "auracloud/services/auth_service.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <utility>

#if defined(AURAHUB_HAVE_OPENSSL)
#include <openssl/evp.h>
#endif

namespace auracloud::services {

namespace {

std::string HashPasswordSha3_256(const std::string& password,
                                const std::string& salt_hex) {
#if defined(AURAHUB_HAVE_OPENSSL)
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) return {};
  unsigned char out[EVP_MAX_MD_SIZE];
  unsigned int out_len = 0;

  const EVP_MD* md = EVP_sha3_256();
  if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
    EVP_MD_CTX_free(ctx);
    return {};
  }
  EVP_DigestUpdate(ctx, salt_hex.data(), salt_hex.size());
  EVP_DigestUpdate(ctx, ":", 1);
  EVP_DigestUpdate(ctx, password.data(), password.size());
  if (EVP_DigestFinal_ex(ctx, out, &out_len) != 1) {
    EVP_MD_CTX_free(ctx);
    return {};
  }
  EVP_MD_CTX_free(ctx);

  static const char kHex[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(out_len * 2);
  for (unsigned int i = 0; i < out_len; ++i) {
    hex.push_back(kHex[(out[i] >> 4) & 0xF]);
    hex.push_back(kHex[out[i] & 0xF]);
  }
  return hex;
#else
  // Minimal fallback (not cryptographically strong). Kept only for builds
  // without OpenSSL. Production will use bcrypt/sha3 on Postgres.
  const std::hash<std::string> h;
  return std::to_string(h(salt_hex + ":" + password));
#endif
}

std::string MakeSaltHex(const std::string& email) {
  // Deterministic minimal salt for MVP; replace with random salt in production.
  static const std::hash<std::string> h;
  return std::to_string(h(email));
}

nlohmann::json LoadDbOrEmpty(const std::string& path) {
  std::ifstream in(path);
  if (!in.is_open()) return nlohmann::json::object({{"users", nlohmann::json::object()}});
  try {
    nlohmann::json j;
    in >> j;
    if (!j.is_object()) return {{"users", nlohmann::json::object()}};
    if (!j.contains("users") || !j["users"].is_object()) j["users"] = nlohmann::json::object();
    return j;
  } catch (...) {
    return {{"users", nlohmann::json::object()}};
  }
}

bool SaveDb(const std::string& path, const nlohmann::json& j) {
  std::ofstream out(path, std::ios::trunc);
  if (!out.is_open()) return false;
  out << j.dump(2);
  return static_cast<bool>(out);
}

std::string ErrorJson(std::string_view code, std::string_view message) {
  return nlohmann::json{{"ok", false}, {"code", code}, {"message", message}}.dump();
}

}  // namespace

AuthService::AuthService(std::string storage_path)
    : storage_path_(std::move(storage_path)) {}

std::string AuthService::Register(std::string email, std::string password) {
  if (email.empty() || password.empty()) {
    return ErrorJson("invalid_input", "email/password required");
  }

  auto db = LoadDbOrEmpty(storage_path_);
  auto& users = db["users"];
  if (users.contains(email)) {
    return ErrorJson("already_exists", "user already exists");
  }

  const std::string salt = MakeSaltHex(email);
  const std::string hash = HashPasswordSha3_256(password, salt);
  if (hash.empty()) return ErrorJson("hash_failed", "password hash failed");

  users[email] = nlohmann::json{{"salt", salt}, {"hash", hash}};
  if (!SaveDb(storage_path_, db)) {
    return ErrorJson("io_failed", "failed to write storage");
  }

  return nlohmann::json{{"ok", true}}.dump();
}

std::string AuthService::Login(std::string email, std::string password) {
  if (email.empty() || password.empty()) {
    return ErrorJson("invalid_input", "email/password required");
  }

  const auto db = LoadDbOrEmpty(storage_path_);
  const auto& users = db["users"];
  if (!users.contains(email)) {
    return ErrorJson("not_found", "user not found");
  }
  const auto& u = users[email];
  if (!u.is_object() || !u.contains("salt") || !u.contains("hash")) {
    return ErrorJson("corrupt", "user record corrupt");
  }

  const std::string salt = u["salt"].get<std::string>();
  const std::string want = u["hash"].get<std::string>();
  const std::string got = HashPasswordSha3_256(password, salt);
  if (got != want) return ErrorJson("unauthorized", "invalid credentials");

  // Minimal token for MVP; replace with real JWT in AuraCloud(userver) phase.
  const std::hash<std::string> h;
  const std::string token = std::to_string(h(email + ":" + want));
  return nlohmann::json{{"ok", true}, {"token", token}}.dump();
}

}  // namespace auracloud::services

