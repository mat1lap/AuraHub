#pragma once

#include <QString>

namespace aura::mcp_assistant::workspace {

/// Resolved project directory used for checkpoints, git detection, and MCP cwd.
struct WorkspaceRoot {
  QString absolute_path;

  [[nodiscard]] bool IsValid() const { return !absolute_path.isEmpty(); }
};

}  // namespace aura::mcp_assistant::workspace
