#include "auracloud/services/auth_service.h"

#include <iostream>
#include <string>

namespace {

int Usage() {
  std::cerr << "Usage:\n"
            << "  auracloud register <email> <password>\n"
            << "  auracloud login <email> <password>\n";
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) return Usage();

  const std::string cmd = argv[1];
  auracloud::services::AuthService auth("auracloud_users.json");

  if (cmd == "register") {
    if (argc != 4) return Usage();
    std::cout << auth.Register(argv[2], argv[3]) << "\n";
    return 0;
  }

  if (cmd == "login") {
    if (argc != 4) return Usage();
    std::cout << auth.Login(argv[2], argv[3]) << "\n";
    return 0;
  }

  return Usage();
}

