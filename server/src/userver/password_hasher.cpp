#include "auracloud/userver/password_hasher.h"

#include <string>

#if defined(AURAHUB_HAVE_OPENSSL)
#include <openssl/evp.h>
#endif

namespace auracloud::security {
namespace {

std::string BytesToHex(const unsigned char* bytes, unsigned int len) {
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (unsigned int i = 0; i < len; ++i) {
    out.push_back(kHex[(bytes[i] >> 4) & 0xF]);
    out.push_back(kHex[bytes[i] & 0xF]);
  }
  return out;
}

}  // namespace

PasswordHash HashPasswordSha3_256(const std::string& password,
                                  const std::string& salt) {
  PasswordHash out;
  out.salt = salt;

#if defined(AURAHUB_HAVE_OPENSSL)
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) return out;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;

  if (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) != 1) {
    EVP_MD_CTX_free(ctx);
    return out;
  }
  EVP_DigestUpdate(ctx, salt.data(), salt.size());
  EVP_DigestUpdate(ctx, ":", 1);
  EVP_DigestUpdate(ctx, password.data(), password.size());
  if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
    EVP_MD_CTX_free(ctx);
    return out;
  }
  EVP_MD_CTX_free(ctx);

  out.hash = BytesToHex(digest, digest_len);
  return out;
#else
  const std::hash<std::string> h;
  out.hash = std::to_string(h(salt + ":" + password));
  return out;
#endif
}

}  // namespace auracloud::security

