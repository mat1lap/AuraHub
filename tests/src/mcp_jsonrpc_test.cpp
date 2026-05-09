#include "auramcp/jsonrpc/json_rpc_parser.h"

#include <gtest/gtest.h>

TEST(McpJsonRpcParser, ParseValidToolsCall) {
  auramcp::ToolResult err;
  const auto req = auramcp::jsonrpc::JsonRpcParser::Parse(
      R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"echo","arguments":{"x":1}}})",
      &err);

  ASSERT_TRUE(req.has_value());
  EXPECT_EQ(req->id_json, "1");
  EXPECT_EQ(req->method, "tools/call");
  EXPECT_NE(req->params_json.find("\"name\""), std::string::npos);
}

TEST(McpJsonRpcParser, ParseErrorReturnsJsonRpcErrorResponse) {
  auramcp::ToolResult err;
  const auto req = auramcp::jsonrpc::JsonRpcParser::Parse("{not-json", &err);
  EXPECT_FALSE(req.has_value());
  EXPECT_TRUE(err.is_error);
  EXPECT_NE(err.content.find("\"error\""), std::string::npos);
  EXPECT_NE(err.content.find("-32700"), std::string::npos);
}

