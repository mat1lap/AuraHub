# AuraHub: Error Memory & Known Workarounds (Memex)

This document tracks persistent errors, architectural decisions, and specific fixes for the AuraHub project. 
Agent Instruction: Consult this file before suggesting fixes. Update this file whenever a recurring issue is solved or a new architectural constraint is discovered.

## Known Issues & Solutions

### 1. MCP Server Exits Immediately (StdioTransport)
- Symptom: Running the MCP binary returns immediately to the shell. The host client (e.g., OpenCode) says command not found or fails to connect.
- Cause: StdioTransport::start() spawns a background thread for reading and returns control to main(), causing the process to exit immediately.
- Solution: start() must be a blocking function that loops std::getline(std::cin, line) in the main thread. Do not use std::thread for stdio transport.

### 2. Server Crash on Unknown Tool / Bad JSON
- Symptom: Server terminates with SIGABRT during tools/call or when receiving malformed JSON.
- Cause: Using throw std::out_of_range in the Tool Registry or letting nlohmann::json::parse_error bubble up.
- Solution: The AuraMCP architecture is non-throwing. Return a ToolResult with isError = true instead of throwing. All JSON parsing must happen safely inside the Router via jsonrpc::JsonRpcParser::parse(), wrapped in a try-catch block that returns a standard JSON-RPC -32700 Parse error.

### 3. ToolRegistry Concurrency Bottlenecks
- Symptom: Slow performance or deadlocks when multiple requests hit the MCP server.
- Cause: std::mutex being locked during the execute phase.
- Solution: Registration phase and Execution phase are separated. Tools are registered sequentially on startup. During execution (after server.run()), the registry is treated as Read-Only. Do not introduce write locks into execute() or get_all_metadata().

## [Template for New Entries]
- Symptom: [What went wrong]
- Cause: [Why it went wrong]
- Solution: [How to fix it permanently within the project's architectural constraints]