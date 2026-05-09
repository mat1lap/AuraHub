#include "mcp_assistant/chat/chat_delegate.h"

#include "mcp_assistant/chat/chat_message.h"
#include "mcp_assistant/models/chat_roles.h"

#include <QApplication>
#include <QDateTime>
#include <QPainter>
#include <QStyle>
#include <QTextDocument>

namespace aura::mcp_assistant::chat {

ChatDelegate::ChatDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void ChatDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                         const QModelIndex& index) const {
  namespace ChatRoles = aura::mcp_assistant::models::ChatRoles;

  const int kind = index.data(ChatRoles::KindRole).toInt();
  const QString body = index.data(ChatRoles::BodyRole).toString();
  const qint64 created_ms = index.data(ChatRoles::CreatedMsRole).toLongLong();

  const int max_width = qMax(120, option.rect.width() - 32);

  QTextDocument doc;
  doc.setMarkdown(body);
  doc.setDefaultFont(option.font);
  doc.setTextWidth(static_cast<qreal>(max_width));

  const QSize doc_size = doc.size().toSize();

  QRect bubble = option.rect.adjusted(8, 6, -8, -6);
  const bool user = kind == static_cast<int>(ChatMessageKind::User);
  const bool typing = kind == static_cast<int>(ChatMessageKind::TypingIndicator);
  const bool system = kind == static_cast<int>(ChatMessageKind::System);
  const bool checkpoint = kind == static_cast<int>(ChatMessageKind::Checkpoint);

  int bubble_height = doc_size.height() + 28;
  if (checkpoint) {
    bubble_height += 40;  // Space for inline rollback widget.
  }
  if (typing) {
    bubble_height = 40;
  }

  if (user) {
    bubble.setLeft(qMax(bubble.left(), bubble.right() - doc_size.width() - 40));
  } else if (system) {
    const int shrink = bubble.width() / 6;
    bubble.adjust(shrink, 0, -shrink, 0);
  }

  bubble.setHeight(qMax(typing ? 36 : 44, bubble_height));

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  QColor bg(48, 52, 60);
  if (user) {
    bg = QColor(59, 130, 246);
  } else if (system) {
    bg = QColor(40, 44, 52);
  } else if (checkpoint) {
    bg = QColor(34, 84, 61);
  } else if (kind == static_cast<int>(ChatMessageKind::ApprovalPrompt)) {
    bg = QColor(59, 54, 35);
  }

  painter->setPen(Qt::NoPen);
  painter->setBrush(bg);
  painter->drawRoundedRect(bubble, 10, 10);

  const QRect text_area = typing ? bubble.adjusted(12, 8, -12, -8) : bubble.adjusted(12, 8, -12, -26);

  painter->translate(text_area.topLeft());
  const QRect clip(0, 0, text_area.width(), text_area.height());
  doc.drawContents(painter, clip);
  painter->resetTransform();

  const QString time =
      QDateTime::fromMSecsSinceEpoch(created_ms).toString(QStringLiteral("hh:mm"));
  QFont small = option.font;
  small.setPointSizeF(qMax(8.0, option.font.pointSizeF() - 1.0));
  painter->setFont(small);
  painter->setPen(QColor(210, 218, 235));
  painter->drawText(bubble.adjusted(12, bubble.height() - 22, -12, -6), Qt::AlignRight, time);

  painter->restore();
}

QSize ChatDelegate::sizeHint(const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
  namespace ChatRoles = aura::mcp_assistant::models::ChatRoles;

  const int kind = index.data(ChatRoles::KindRole).toInt();
  const QString body = index.data(ChatRoles::BodyRole).toString();

  const int view_w = option.rect.width() > 0 ? option.rect.width() : 640;
  const int max_width = qMax(120, view_w - 32);

  QTextDocument doc;
  doc.setMarkdown(body);
  doc.setDefaultFont(option.font);
  doc.setTextWidth(static_cast<qreal>(max_width));

  const bool typing = kind == static_cast<int>(ChatMessageKind::TypingIndicator);
  const bool checkpoint = kind == static_cast<int>(ChatMessageKind::Checkpoint);

  int height = static_cast<int>(doc.size().height()) + 40;
  if (checkpoint) {
    height += 44;
  }
  if (typing) {
    height = 44;
  }
  height = qBound(44, height, 8000);
  return QSize(view_w, height);
}

}  // namespace aura::mcp_assistant::chat
