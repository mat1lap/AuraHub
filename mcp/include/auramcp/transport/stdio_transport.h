#pragma once

#include <functional>
#include <string>

namespace auramcp::transport {

// IMPORTANT (project invariant):
// - start() MUST be blocking and run the stdio read loop in the caller thread.
// - If start() returns immediately, the MCP process can terminate and the host
//   client will fail to connect.
class StdioTransport final {
 public:
  using OnMessage = std::function<void(std::string_view)>;

  explicit StdioTransport(OnMessage on_message);

  void Start();  // blocking

 private:
  OnMessage on_message_;
};

}  // namespace auramcp::transport

