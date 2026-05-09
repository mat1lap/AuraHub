#pragma once

#include "chat_message.h"

#include <QAbstractListModel>
#include <vector>

namespace aura::mcp_assistant::chat {

class ChatMessagesModel final : public QAbstractListModel {
  Q_OBJECT

 public:
  explicit ChatMessagesModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void AppendUserMessage(QString markdown);
  void AppendAgentMessage(QString markdown);
  void AppendSystemMessage(QString text);
  void AppendCheckpointCard(QString title, qint64 checkpoint_id);
  void AppendApprovalPrompt(qint64 pending_change_id, QString summary);
  void SetTypingIndicator(bool on);

  [[nodiscard]] QModelIndex IndexForRow(int row) const;

 private:
  [[nodiscard]] int FindTypingRow() const;

  std::vector<ChatMessage> messages_;
  qint64 next_id_{1};
  bool typing_visible_{false};
};

}  // namespace aura::mcp_assistant::chat
