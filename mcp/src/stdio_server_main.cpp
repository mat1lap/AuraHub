#include "auramcp/jsonrpc/router.h"
#include "auramcp/tool_registry.h"
#include "auramcp/transport/stdio_transport.h"

#include <iostream>
#include <string>

int main() {
  auramcp::ToolRegistry registry;
  registry.Register(
      auramcp::ToolRegistry::ToolInfo{
          .name = "echo",
          .description = "Returns arguments as JSON string (debug tool).",
          .input_schema_json = R"({"type":"object"})"},
      [](std::string_view args) { return auramcp::ToolResult::Ok(std::string(args)); });

  const auramcp::jsonrpc::Router router(registry);

  auramcp::transport::StdioTransport transport([&](std::string_view msg) {
    const std::string response = router.Handle(msg);
    std::cout << response << std::endl;
  });

  transport.Start();
  return 0;
}

