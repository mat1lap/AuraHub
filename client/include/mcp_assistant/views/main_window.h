#pragma once

#include "mcp_assistant/chat/chat_delegate.h"

#include <QMainWindow>

#include <memory>

QT_FORWARD_DECLARE_CLASS(QCloseEvent)
QT_FORWARD_DECLARE_CLASS(QListView)
QT_FORWARD_DECLARE_CLASS(QPlainTextEdit)
QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QPushButton)

namespace aura::mcp_assistant::chat {
class ChatMessagesModel;
}  // namespace aura::mcp_assistant::chat
namespace aura::mcp_assistant::checkpoint {
class CheckpointListModel;
}  // namespace aura::mcp_assistant::checkpoint
namespace aura::mcp_assistant::services {
class AssistantSessionController;
}  // namespace aura::mcp_assistant::services
namespace aura::mcp_assistant::widgets {
class DiffPreviewWidget;
}  // namespace aura::mcp_assistant::widgets

namespace aura::mcp_assistant::views {

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);

  void SetSessionController(services::AssistantSessionController* controller);
  void SetChatModel(chat::ChatMessagesModel* model);
  void SetCheckpointModel(checkpoint::CheckpointListModel* model);

 protected:
  void closeEvent(QCloseEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void OnSendClicked();
  void OnCancelClicked();
  void OnConnectToggled(bool checked);
  void OnPendingApprovalChanged(bool active);
  void OnInteractionGateChanged(bool can_send_message);
  void OnCheckpointRollbackClicked();
  void OnDiffAccepted();
  void OnDiffRejected();
  void OnChatRowsInserted(const QModelIndex& parent, int first, int last);

 private:
  void ScrollChatToBottom();
  void RefreshApprovalUi();

  services::AssistantSessionController* controller_{nullptr};
  chat::ChatMessagesModel* chat_model_{nullptr};
  checkpoint::CheckpointListModel* checkpoint_model_{nullptr};

  QListView* chat_view_{nullptr};
  QListView* checkpoint_view_{nullptr};
  QPlainTextEdit* prompt_{nullptr};
  QPushButton* send_btn_{nullptr};
  QPushButton* cancel_btn_{nullptr};
  QPushButton* connect_btn_{nullptr};
  widgets::DiffPreviewWidget* diff_{nullptr};

  QLabel* status_connection_{nullptr};
  QLabel* status_tokens_{nullptr};
  QLabel* reconnect_badge_{nullptr};

  std::unique_ptr<chat::ChatDelegate> chat_delegate_{};
};

}  // namespace aura::mcp_assistant::views
