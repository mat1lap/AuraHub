#pragma once

#include "mcp_assistant/services/pending_change.h"

#include <QWidget>

QT_FORWARD_DECLARE_CLASS(QPlainTextEdit)
QT_FORWARD_DECLARE_CLASS(QTreeWidget)
QT_FORWARD_DECLARE_CLASS(QLabel)

namespace aura::mcp_assistant::widgets {

/// Side-by-side diff with synchronized scrolling and simple line highlighting.
class DiffPreviewWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit DiffPreviewWidget(QWidget* parent = nullptr);

  void SetPendingChange(const services::PendingChange& change);
  void Clear();

 signals:
  void Accepted();
  void Rejected();

 private slots:
  void OnFileSelectionChanged();
  void OnLeftScroll(int value);
  void OnRightScroll(int value);

 private:
  void LoadCurrentFile();

  QTreeWidget* file_tree_{nullptr};
  QPlainTextEdit* left_{nullptr};
  QPlainTextEdit* right_{nullptr};
  QLabel* header_{nullptr};

  services::PendingChange pending_;
  int syncing_scroll_guard_{0};
};

}  // namespace aura::mcp_assistant::widgets
