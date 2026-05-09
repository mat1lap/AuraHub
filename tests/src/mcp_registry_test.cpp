#include "auramcp/tool_registry.h"

#include <gtest/gtest.h>

TEST(McpToolRegistry, UnknownToolReturnsError) {
  auramcp::ToolRegistry r;
  const auto res = r.Call("missing", "null");
  EXPECT_TRUE(res.is_error);
}

TEST(McpToolRegistry, RegisteredToolCanBeCalled) {
  auramcp::ToolRegistry r;
  ASSERT_TRUE(r.Register(
      auramcp::ToolRegistry::ToolInfo{.name = "echo",
                                     .description = "d",
                                     .input_schema_json = "{}"},
      [](std::string_view args) {
        return auramcp::ToolResult::Ok(std::string(args));
      }));

  const auto res = r.Call("echo", R"({"x":1})");
  EXPECT_FALSE(res.is_error);
  EXPECT_EQ(res.content, R"({"x":1})");
}

