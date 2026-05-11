#pragma once

#include <QStyledItemDelegate>

namespace aura::mcp_assistant::chat {

/// Renders chat bubbles with QTextDocument (Markdown) for scalable transcripts.
class ChatDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit ChatDelegate(QObject* parent = nullptr);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                               const QModelIndex& index) const override;
};

}  // namespace aura::mcp_assistant::chat
