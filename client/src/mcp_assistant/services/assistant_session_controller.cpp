#include "mcp_assistant/services/assistant_session_controller.h"

#include "mcp_assistant/chat/chat_messages_model.h"
#include "mcp_assistant/checkpoint/checkpoint_list_model.h"
#include "mcp_assistant/checkpoint/checkpoint_manager.h"
#include "mcp_assistant/mcp/mcp_client.h"

#include <QJsonObject>

namespace aura::mcp_assistant::services {

AssistantSessionController::AssistantSessionController(
    chat::ChatMessagesModel* chat_model, checkpoint::CheckpointListModel* checkpoint_model,
    checkpoint::CheckpointManager* checkpoint_manager, mcp::McpClient* mcp_client,
    QObject* parent)
    : QObject(parent),
      chat_model_(chat_model),
      checkpoint_model_(checkpoint_model),
      checkpoint_manager_(checkpoint_manager),
      mcp_client_(mcp_client) {
  connect(mcp_client_, &mcp::McpClient::AssistantMessageCompleted, this,
          &AssistantSessionController::OnAssistantCompleted);
  connect(mcp_client_, &mcp::McpClient::AssistantDelta, this,
          &AssistantSessionController::OnAssistantDelta);
  connect(mcp_client_, &mcp::McpClient::TokenUsageEstimate, this,
          &AssistantSessionController::TokenUsageChanged);
  connect(mcp_client_, &mcp::McpClient::ConnectionStateChanged, this,
          [this](bool ok) {
            ConnectionSummaryChanged(ok ? QStringLiteral("connected")
                                        : QStringLiteral("offline / demo"),
                                     ok);
          });
  connect(mcp_client_, &mcp::McpClient::ScheduleReconnectIn, this,
          [this](int ms) {
            ConnectionSummaryChanged(
                QStringLiteral("reconnect in %1 ms").arg(ms), false);
          });

  connect(checkpoint_manager_, &checkpoint::CheckpointManager::CheckpointCreated, this,
          [this](checkpoint::CheckpointRecord rec) {
            checkpoint_model_->UpsertCheckpoint(rec);
          });
  connect(checkpoint_manager_, &checkpoint::CheckpointManager::RollbackFinished, this,
          &AssistantSessionController::OnRollbackFinished);
}

void AssistantSessionController::SetWorkspaceRoot(QString absolute_path) {
  workspace_root_ = std::move(absolute_path);
  checkpoint_manager_->SetWorkspaceRoot(workspace_root_);
}

void AssistantSessionController::SubmitPrompt(QString text) {
  const QString trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  chat_model_->AppendUserMessage(trimmed);
  chat_model_->SetTypingIndicator(true);
  streaming_buffer_.clear();

  if (mcp_client_->IsConnected()) {
    QJsonObject params;
    params.insert(QStringLiteral("text"), trimmed);
    mcp_client_->SendRequest(QStringLiteral("messages/prompt"), params);
    return;
  }

  mcp_client_->EmitDemoAssistantSequence(trimmed);
}

void AssistantSessionController::CancelActiveRequest() {
  mcp_client_->CancelInflight();
  chat_model_->SetTypingIndicator(false);
}

void AssistantSessionController::ApprovePendingChange() {
  if (!pending_.has_value()) {
    return;
  }
  const QString excerpt = pending_->title;
  const checkpoint::CheckpointRecord rec =
      checkpoint_manager_->CreateCheckpoint(excerpt, QStringLiteral("Applied proposal"));

  chat_model_->AppendCheckpointCard(rec.summary, rec.id);
  chat_model_->AppendSystemMessage(
      QStringLiteral("Checkpoint %1 saved.").arg(rec.id));

  ClearPending();
}

void AssistantSessionController::RejectPendingChange() {
  if (!pending_.has_value()) {
    return;
  }
  chat_model_->AppendSystemMessage(QStringLiteral("Changes rejected."));
  ClearPending();
}

void AssistantSessionController::RollbackCheckpoint(qint64 checkpoint_id) {
  checkpoint_manager_->RollbackAsync(checkpoint_id);
}

void AssistantSessionController::ConnectMcp() { mcp_client_->Connect(); }

void AssistantSessionController::DisconnectMcp() { mcp_client_->Disconnect(); }

void AssistantSessionController::OnAssistantCompleted(QString full_text) {
  chat_model_->SetTypingIndicator(false);
  chat_model_->AppendAgentMessage(full_text);
  EnsureDemoApprovalOffer(full_text, QString());
}

void AssistantSessionController::OnAssistantDelta(QString chunk) {
  streaming_buffer_.append(chunk);
  Q_UNUSED(chunk);
}

void AssistantSessionController::OnRollbackFinished(qint64 checkpoint_id, bool ok,
                                                    QString error) {
  if (ok) {
    chat_model_->AppendSystemMessage(
        QStringLiteral("Rolled back to checkpoint %1").arg(checkpoint_id));
    return;
  }
  chat_model_->AppendSystemMessage(
      QStringLiteral("Rollback failed: %1").arg(error));
}

void AssistantSessionController::ClearPending() {
  pending_.reset();
  emit PendingApprovalChanged(false);
}

void AssistantSessionController::EnsureDemoApprovalOffer(const QString& assistant_markdown,
                                                         const QString& user_prompt) {
  Q_UNUSED(user_prompt);
  if (pending_.has_value()) {
    return;
  }
  if (mcp_client_->IsConnected()) {
    return;
  }
  if (!assistant_markdown.contains(QStringLiteral("patch"), Qt::CaseInsensitive) &&
      !assistant_markdown.contains(QStringLiteral("parser"), Qt::CaseInsensitive)) {
    return;
  }

  PendingChange pc;
  pc.id = next_pending_id_++;
  pc.title = QStringLiteral("parser.cpp");
  services::FilePatch fp;
  fp.relative_path = QStringLiteral("parser.cpp");
  fp.before_text = QStringLiteral("// Before\nreturn 0;\n");
  fp.after_text = QStringLiteral("// After\nreturn 42;\n");
  pc.files.push_back(fp);

  pending_ = pc;
  chat_model_->AppendApprovalPrompt(pc.id,
                                    QStringLiteral("Review proposed edits before applying."));
  emit PendingApprovalChanged(true);
}

}  // namespace aura::mcp_assistant::services
