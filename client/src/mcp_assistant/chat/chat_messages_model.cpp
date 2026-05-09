#include "mcp_assistant/chat/chat_messages_model.h"

#include "mcp_assistant/models/chat_roles.h"

#include <QDateTime>

namespace aura::mcp_assistant::chat {

ChatMessagesModel::ChatMessagesModel(QObject* parent) : QAbstractListModel(parent) {}

int ChatMessagesModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(messages_.size());
}

QVariant ChatMessagesModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(messages_.size())) {
    return {};
  }
  const ChatMessage& m = messages_[static_cast<std::size_t>(index.row())];
  namespace ChatRoles = aura::mcp_assistant::models::ChatRoles;
  switch (role) {
    case Qt::DisplayRole:
      return m.markdown_body;
    case ChatRoles::KindRole:
      return static_cast<int>(m.kind);
    case ChatRoles::BodyRole:
      return m.markdown_body;
    case ChatRoles::CreatedMsRole:
      return m.created_epoch_ms;
    case ChatRoles::CheckpointIdRole:
      return m.checkpoint_id.value_or(-1);
    case ChatRoles::CheckpointTitleRole:
      return m.checkpoint_title;
    case ChatRoles::PendingChangeIdRole:
      return m.pending_change_id.value_or(-1);
    case ChatRoles::MessageIdRole:
      return QVariant::fromValue(m.id);
    default:
      return {};
  }
}

QHash<int, QByteArray> ChatMessagesModel::roleNames() const {
  namespace ChatRoles = aura::mcp_assistant::models::ChatRoles;
  QHash<int, QByteArray> r;
  r.insert(ChatRoles::KindRole, "kind");
  r.insert(ChatRoles::BodyRole, "body");
  r.insert(ChatRoles::CreatedMsRole, "createdMs");
  r.insert(ChatRoles::CheckpointIdRole, "checkpointId");
  r.insert(ChatRoles::CheckpointTitleRole, "checkpointTitle");
  r.insert(ChatRoles::PendingChangeIdRole, "pendingChangeId");
  r.insert(ChatRoles::MessageIdRole, "messageId");
  return r;
}

void ChatMessagesModel::AppendUserMessage(QString markdown) {
  ChatMessage m;
  m.id = next_id_++;
  m.created_epoch_ms = QDateTime::currentMSecsSinceEpoch();
  m.kind = ChatMessageKind::User;
  m.markdown_body = std::move(markdown);
  const int row = static_cast<int>(messages_.size());
  beginInsertRows(QModelIndex(), row, row);
  messages_.push_back(std::move(m));
  endInsertRows();
}

void ChatMessagesModel::AppendAgentMessage(QString markdown) {
  ChatMessage m;
  m.id = next_id_++;
  m.created_epoch_ms = QDateTime::currentMSecsSinceEpoch();
  m.kind = ChatMessageKind::Agent;
  m.markdown_body = std::move(markdown);
  const int row = static_cast<int>(messages_.size());
  beginInsertRows(QModelIndex(), row, row);
  messages_.push_back(std::move(m));
  endInsertRows();
}

void ChatMessagesModel::AppendSystemMessage(QString text) {
  ChatMessage m;
  m.id = next_id_++;
  m.created_epoch_ms = QDateTime::currentMSecsSinceEpoch();
  m.kind = ChatMessageKind::System;
  m.markdown_body = std::move(text);
  const int row = static_cast<int>(messages_.size());
  beginInsertRows(QModelIndex(), row, row);
  messages_.push_back(std::move(m));
  endInsertRows();
}

void ChatMessagesModel::AppendCheckpointCard(QString title, qint64 checkpoint_id) {
  ChatMessage m;
  m.id = next_id_++;
  m.created_epoch_ms = QDateTime::currentMSecsSinceEpoch();
  m.kind = ChatMessageKind::Checkpoint;
  m.checkpoint_id = checkpoint_id;
  m.checkpoint_title = std::move(title);
  m.markdown_body =
      QStringLiteral("✔ Changes applied successfully.\n\n%1").arg(m.checkpoint_title);
  const int row = static_cast<int>(messages_.size());
  beginInsertRows(QModelIndex(), row, row);
  messages_.push_back(std::move(m));
  endInsertRows();
}

void ChatMessagesModel::AppendApprovalPrompt(qint64 pending_change_id, QString summary) {
  ChatMessage m;
  m.id = next_id_++;
  m.created_epoch_ms = QDateTime::currentMSecsSinceEpoch();
  m.kind = ChatMessageKind::ApprovalPrompt;
  m.pending_change_id = pending_change_id;
  m.markdown_body = std::move(summary);
  const int row = static_cast<int>(messages_.size());
  beginInsertRows(QModelIndex(), row, row);
  messages_.push_back(std::move(m));
  endInsertRows();
}

void ChatMessagesModel::SetTypingIndicator(bool on) {
  if (typing_visible_ == on) {
    return;
  }
  if (on) {
    ChatMessage m;
    m.id = next_id_++;
    m.created_epoch_ms = QDateTime::currentMSecsSinceEpoch();
    m.kind = ChatMessageKind::TypingIndicator;
    m.markdown_body = QStringLiteral("…");
    const int row = static_cast<int>(messages_.size());
    beginInsertRows(QModelIndex(), row, row);
    messages_.push_back(std::move(m));
    endInsertRows();
    typing_visible_ = true;
    return;
  }
  const int row = FindTypingRow();
  if (row < 0) {
    typing_visible_ = false;
    return;
  }
  beginRemoveRows(QModelIndex(), row, row);
  messages_.erase(messages_.begin() + row);
  endRemoveRows();
  typing_visible_ = false;
}

QModelIndex ChatMessagesModel::IndexForRow(int row) const {
  return index(row, 0);
}

int ChatMessagesModel::FindTypingRow() const {
  for (int i = 0; i < static_cast<int>(messages_.size()); ++i) {
    if (messages_[static_cast<std::size_t>(i)].kind == ChatMessageKind::TypingIndicator) {
      return i;
    }
  }
  return -1;
}

}  // namespace aura::mcp_assistant::chat
