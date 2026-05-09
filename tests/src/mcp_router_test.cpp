#include "auramcp/jsonrpc/router.h"
#include "auramcp/tool_registry.h"

#include <gtest/gtest.h>

TEST(McpRouter, ToolsCallEchoReturnsResult) {
  auramcp::ToolRegistry reg;
  ASSERT_TRUE(reg.Register(
      auramcp::ToolRegistry::ToolInfo{.name = "echo",
                                     .description = "d",
                                     .input_schema_json = "{}"},
      [](std::string_view args) {
        return auramcp::ToolResult::Ok(std::string(args));
      }));
  auramcp::jsonrpc::Router router(reg);

  const std::string resp = router.Handle(
      R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"echo","arguments":{"hello":"world"}}})");
  EXPECT_NE(resp.find("\"result\""), std::string::npos);
  EXPECT_NE(resp.find("\"content\""), std::string::npos);
}

TEST(McpRouter, UnknownMethodReturnsMethodNotFound) {
  auramcp::ToolRegistry reg;
  auramcp::jsonrpc::Router router(reg);

  const std::string resp =
      router.Handle(R"({"jsonrpc":"2.0","id":1,"method":"nope"})");
  EXPECT_NE(resp.find("-32601"), std::string::npos);
}

TEST(McpRouter, ToolsListReturnsRegisteredTools) {
  auramcp::ToolRegistry reg;
  ASSERT_TRUE(reg.Register(
      auramcp::ToolRegistry::ToolInfo{.name = "echo",
                                     .description = "d",
                                     .input_schema_json = "{}"},
      [](std::string_view) { return auramcp::ToolResult::Ok("ok"); }));
  auramcp::jsonrpc::Router router(reg);

  const std::string resp =
      router.Handle(R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})");
  EXPECT_NE(resp.find("\"tools\""), std::string::npos);
  EXPECT_NE(resp.find("\"name\":\"echo\""), std::string::npos);
}

