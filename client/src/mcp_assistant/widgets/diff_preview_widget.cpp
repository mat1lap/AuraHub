#include "mcp_assistant/widgets/diff_preview_widget.h"

#include <QHBoxLayout>
#include <QScrollBar>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace aura::mcp_assistant::widgets {

DiffPreviewWidget::DiffPreviewWidget(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  header_ = new QLabel(QStringLiteral("Diff preview"), this);
  header_->setObjectName(QStringLiteral("DiffHeader"));
  root->addWidget(header_);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  file_tree_ = new QTreeWidget(this);
  file_tree_->setHeaderHidden(true);
  file_tree_->setMaximumWidth(260);
  connect(file_tree_, &QTreeWidget::currentItemChanged, this,
          &DiffPreviewWidget::OnFileSelectionChanged);

  left_ = new QPlainTextEdit(this);
  right_ = new QPlainTextEdit(this);
  for (auto* e : {left_, right_}) {
    e->setReadOnly(true);
    e->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont f = e->font();
    f.setFamily(QStringLiteral("JetBrains Mono"));
    if (!f.exactMatch()) {
      f.setFamily(QStringLiteral("Monospace"));
    }
    e->setFont(f);
  }

  splitter->addWidget(file_tree_);
  auto* diff_split = new QSplitter(Qt::Horizontal, this);
  diff_split->addWidget(left_);
  diff_split->addWidget(right_);
  splitter->addWidget(diff_split);
  root->addWidget(splitter, 1);

  auto* actions = new QHBoxLayout();
  auto* accept = new QPushButton(tr("Accept"), this);
  auto* reject = new QPushButton(tr("Reject"), this);
  connect(accept, &QPushButton::clicked, this, &DiffPreviewWidget::Accepted);
  connect(reject, &QPushButton::clicked, this, &DiffPreviewWidget::Rejected);
  actions->addStretch(1);
  actions->addWidget(reject);
  actions->addWidget(accept);
  root->addLayout(actions);

  connect(left_->verticalScrollBar(), &QScrollBar::valueChanged, this,
          &DiffPreviewWidget::OnLeftScroll);
  connect(right_->verticalScrollBar(), &QScrollBar::valueChanged, this,
          &DiffPreviewWidget::OnRightScroll);

  Clear();
}

void DiffPreviewWidget::SetPendingChange(const services::PendingChange& change) {
  pending_ = change;
  file_tree_->clear();
  for (const auto& f : pending_.files) {
    auto* item = new QTreeWidgetItem(QStringList{f.relative_path});
    item->setData(0, Qt::UserRole, f.relative_path);
    file_tree_->addTopLevelItem(item);
  }
  if (!pending_.files.isEmpty()) {
    file_tree_->setCurrentItem(file_tree_->topLevelItem(0));
  }
  header_->setText(tr("Pending changes — %1").arg(pending_.title));
  LoadCurrentFile();
}

void DiffPreviewWidget::Clear() {
  pending_ = {};
  file_tree_->clear();
  left_->clear();
  right_->clear();
  header_->setText(tr("No pending changes"));
}

void DiffPreviewWidget::OnFileSelectionChanged() { LoadCurrentFile(); }

void DiffPreviewWidget::LoadCurrentFile() {
  auto* item = file_tree_->currentItem();
  if (item == nullptr) {
    left_->clear();
    right_->clear();
    return;
  }
  const QString path = item->data(0, Qt::UserRole).toString();
  for (const auto& f : pending_.files) {
    if (f.relative_path == path) {
      left_->setPlainText(f.before_text);
      right_->setPlainText(f.after_text);
      return;
    }
  }
}

void DiffPreviewWidget::OnLeftScroll(int value) {
  if (syncing_scroll_guard_ != 0) {
    return;
  }
  ++syncing_scroll_guard_;
  right_->verticalScrollBar()->setValue(value);
  --syncing_scroll_guard_;
}

void DiffPreviewWidget::OnRightScroll(int value) {
  if (syncing_scroll_guard_ != 0) {
    return;
  }
  ++syncing_scroll_guard_;
  left_->verticalScrollBar()->setValue(value);
  --syncing_scroll_guard_;
}

}  // namespace aura::mcp_assistant::widgets
