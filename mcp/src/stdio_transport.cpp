#include "auramcp/transport/stdio_transport.h"

#include <iostream>
#include <string>

namespace auramcp::transport {

StdioTransport::StdioTransport(OnMessage on_message)
    : on_message_(std::move(on_message)) {}

void StdioTransport::Start() {
  std::string line;
  while (std::getline(std::cin, line)) {
    on_message_(line);
  }
}

}  // namespace auramcp::transport

