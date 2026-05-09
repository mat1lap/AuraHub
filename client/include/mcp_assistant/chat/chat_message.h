#pragma once

#include <QString>

#include <optional>

namespace aura::mcp_assistant::chat {

enum class ChatMessageKind {
  User = 0,
  Agent,
  System,
  Checkpoint,
  ApprovalPrompt,
  TypingIndicator,
};

struct ChatMessage {
  qint64 id{0};
  qint64 created_epoch_ms{0};
  ChatMessageKind kind{ChatMessageKind::Agent};
  QString markdown_body;
  /// Optional: checkpoint card in-chat.
  std::optional<qint64> checkpoint_id;
  QString checkpoint_title;
  /// Links approval UI to pending change id.
  std::optional<qint64> pending_change_id;
};

}  // namespace aura::mcp_assistant::chat
