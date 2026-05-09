#pragma once

#include "pending_change.h"

#include <QObject>
#include <QString>

#include <optional>

namespace aura::mcp_assistant::chat {
class ChatMessagesModel;
}
namespace aura::mcp_assistant::checkpoint {
class CheckpointListModel;
class CheckpointManager;
}  // namespace aura::mcp_assistant::checkpoint
namespace aura::mcp_assistant::mcp {
class McpClient;
}  // namespace aura::mcp_assistant::mcp

namespace aura::mcp_assistant::services {

/// Coordinates chat UX, MCP lifecycle, approvals, and checkpoints (MVVM "presenter").
class AssistantSessionController final : public QObject {
  Q_OBJECT

 public:
  AssistantSessionController(chat::ChatMessagesModel* chat_model,
                             checkpoint::CheckpointListModel* checkpoint_model,
                             checkpoint::CheckpointManager* checkpoint_manager,
                             mcp::McpClient* mcp_client, QObject* parent = nullptr);

  void SetWorkspaceRoot(QString absolute_path);

  [[nodiscard]] bool HasPendingApproval() const { return pending_.has_value(); }

  [[nodiscard]] PendingChange CurrentPending() const {
    return pending_.value_or(PendingChange{});
  }

 public slots:
  void SubmitPrompt(QString text);
  void CancelActiveRequest();
  void ApprovePendingChange();
  void RejectPendingChange();
  void RollbackCheckpoint(qint64 checkpoint_id);

  void ConnectMcp();
  void DisconnectMcp();

 private slots:
  void OnAssistantCompleted(QString full_text);
  void OnAssistantDelta(QString chunk);
  void OnRollbackFinished(qint64 checkpoint_id, bool ok, QString error);

 signals:
  void PendingApprovalChanged(bool active);
  void TokenUsageChanged(int prompt_tokens, int completion_tokens);
  void ConnectionSummaryChanged(QString text, bool healthy);

 private:
  void ClearPending();
  void EnsureDemoApprovalOffer(const QString& assistant_markdown, const QString& user_prompt);

  chat::ChatMessagesModel* chat_model_{nullptr};
  checkpoint::CheckpointListModel* checkpoint_model_{nullptr};
  checkpoint::CheckpointManager* checkpoint_manager_{nullptr};
  mcp::McpClient* mcp_client_{nullptr};

  QString workspace_root_;
  std::optional<PendingChange> pending_;
  qint64 next_pending_id_{1};

  QString streaming_buffer_;
};

}  // namespace aura::mcp_assistant::services
