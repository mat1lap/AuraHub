#include "mcp_assistant/views/main_window.h"

#include "mcp_assistant/chat/chat_messages_model.h"
#include "mcp_assistant/checkpoint/checkpoint_list_model.h"
#include "mcp_assistant/models/chat_roles.h"
#include "mcp_assistant/services/assistant_session_controller.h"
#include "mcp_assistant/widgets/diff_preview_widget.h"

#include "mcp_assistant/chat/chat_message.h"

#include <QAbstractItemModel>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QEvent>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace aura::mcp_assistant::views {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(tr("AuraHub MCP Assistant"));
  resize(1280, 820);

  auto* central = new QWidget(this);
  setCentralWidget(central);

  auto* root = new QVBoxLayout(central);

  auto* top_split = new QSplitter(Qt::Horizontal, this);

  checkpoint_view_ = new QListView(this);
  checkpoint_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  checkpoint_view_->setUniformItemSizes(true);

  auto* chat_column = new QWidget(this);
  auto* chat_layout = new QVBoxLayout(chat_column);
  chat_view_ = new QListView(this);
  chat_view_->setSelectionMode(QAbstractItemView::NoSelection);
  chat_view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  chat_view_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  chat_delegate_ = std::make_unique<chat::ChatDelegate>(chat_view_);
  chat_view_->setItemDelegate(chat_delegate_.get());

  diff_ = new widgets::DiffPreviewWidget(this);
  diff_->setMinimumHeight(220);

  auto* inner_split = new QSplitter(Qt::Vertical, this);
  inner_split->addWidget(chat_view_);
  inner_split->addWidget(diff_);
  inner_split->setStretchFactor(0, 3);
  inner_split->setStretchFactor(1, 2);

  chat_layout->addWidget(inner_split, 1);

  top_split->addWidget(checkpoint_view_);
  top_split->addWidget(chat_column);
  top_split->setStretchFactor(0, 2);
  top_split->setStretchFactor(1, 5);

  root->addWidget(top_split, 1);

  auto* prompt_row = new QHBoxLayout();
  prompt_ = new QPlainTextEdit(this);
  prompt_->setPlaceholderText(tr("Message… (Enter send, Shift+Enter newline)"));
  prompt_->setFixedHeight(110);
  prompt_->installEventFilter(this);

  send_btn_ = new QPushButton(tr("Send"), this);
  cancel_btn_ = new QPushButton(tr("Cancel"), this);
  connect_btn_ = new QPushButton(tr("Connect MCP"), this);
  connect_btn_->setCheckable(true);

  prompt_row->addWidget(prompt_, 1);
  prompt_row->addWidget(send_btn_, 0);
  prompt_row->addWidget(cancel_btn_, 0);
  prompt_row->addWidget(connect_btn_, 0);

  root->addLayout(prompt_row);

  auto* toolbar = new QToolBar(this);
  auto* rollback_btn = new QPushButton(tr("Rollback selected checkpoint"), this);
  toolbar->addWidget(rollback_btn);
  addToolBar(Qt::TopToolBarArea, toolbar);

  connect(send_btn_, &QPushButton::clicked, this, &MainWindow::OnSendClicked);
  connect(cancel_btn_, &QPushButton::clicked, this, &MainWindow::OnCancelClicked);
  connect(connect_btn_, &QPushButton::toggled, this, &MainWindow::OnConnectToggled);
  connect(rollback_btn, &QPushButton::clicked, this,
          &MainWindow::OnCheckpointRollbackClicked);

  connect(diff_, &widgets::DiffPreviewWidget::Accepted, this, &MainWindow::OnDiffAccepted);
  connect(diff_, &widgets::DiffPreviewWidget::Rejected, this, &MainWindow::OnDiffRejected);

  status_connection_ = new QLabel(tr("offline / demo"), this);
  status_tokens_ = new QLabel(tr("tokens —"), this);
  reconnect_badge_ = new QLabel(tr(" "), this);
  reconnect_badge_->setMinimumWidth(120);
  reconnect_badge_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  statusBar()->addPermanentWidget(status_connection_, 1);
  statusBar()->addPermanentWidget(status_tokens_, 1);
  statusBar()->addPermanentWidget(reconnect_badge_, 0);

  diff_->Clear();
  diff_->setEnabled(false);
}

void MainWindow::SetSessionController(services::AssistantSessionController* controller) {
  controller_ = controller;
  if (controller_ == nullptr) {
    return;
  }
  connect(controller_, &services::AssistantSessionController::PendingApprovalChanged, this,
          &MainWindow::OnPendingApprovalChanged);
  connect(controller_, &services::AssistantSessionController::TokenUsageChanged, this,
          [this](int p, int c) {
            status_tokens_->setText(tr("tokens ~ %1 / %2").arg(p).arg(c));
          });
  connect(controller_, &services::AssistantSessionController::ConnectionSummaryChanged, this,
          [this](QString text, bool healthy) {
            status_connection_->setText(text);
            reconnect_badge_->setStyleSheet(
                healthy ? QStringLiteral("color:#5bd37d;font-weight:600;")
                        : QStringLiteral("color:#ffb020;font-weight:600;"));
            reconnect_badge_->setText(healthy ? tr("● live") : tr("● reconnecting"));
          });
}

void MainWindow::SetChatModel(chat::ChatMessagesModel* model) {
  chat_model_ = model;
  chat_view_->setModel(chat_model_);
  connect(chat_model_, &QAbstractItemModel::rowsInserted, this,
          &MainWindow::OnChatRowsInserted);
  connect(chat_model_, &QAbstractItemModel::rowsInserted, this,
          [this](const QModelIndex&, int, int) { ScrollChatToBottom(); });
}

void MainWindow::SetCheckpointModel(checkpoint::CheckpointListModel* model) {
  checkpoint_model_ = model;
  checkpoint_view_->setModel(checkpoint_model_);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (watched == prompt_ && event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
      if (ke->modifiers().testFlag(Qt::ShiftModifier)) {
        return QMainWindow::eventFilter(watched, event);
      }
      OnSendClicked();
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::OnSendClicked() {
  if (controller_ == nullptr) {
    return;
  }
  const QString text = prompt_->toPlainText();
  prompt_->clear();
  controller_->SubmitPrompt(text);
}

void MainWindow::OnCancelClicked() {
  if (controller_ != nullptr) {
    controller_->CancelActiveRequest();
  }
}

void MainWindow::OnConnectToggled(bool checked) {
  if (controller_ == nullptr) {
    return;
  }
  if (checked) {
    controller_->ConnectMcp();
    return;
  }
  controller_->DisconnectMcp();
}

void MainWindow::OnPendingApprovalChanged(bool active) {
  diff_->setEnabled(active);
  if (!active || controller_ == nullptr) {
    diff_->Clear();
    return;
  }
  diff_->SetPendingChange(controller_->CurrentPending());
}

void MainWindow::OnCheckpointRollbackClicked() {
  if (controller_ == nullptr || checkpoint_model_ == nullptr) {
    return;
  }
  const QModelIndex idx = checkpoint_view_->currentIndex();
  if (!idx.isValid()) {
    QMessageBox::information(this, tr("Rollback"), tr("Select a checkpoint."));
    return;
  }
  const qint64 id =
      idx.data(checkpoint::roles::IdRole).toLongLong();
  const auto reply =
      QMessageBox::question(this, tr("Rollback"),
                            tr("Rollback workspace to checkpoint %1?").arg(id));
  if (reply != QMessageBox::Yes) {
    return;
  }
  controller_->RollbackCheckpoint(id);
}

void MainWindow::OnDiffAccepted() {
  if (controller_ != nullptr) {
    controller_->ApprovePendingChange();
  }
}

void MainWindow::OnDiffRejected() {
  if (controller_ != nullptr) {
    controller_->RejectPendingChange();
  }
}

void MainWindow::OnChatRowsInserted(const QModelIndex& parent, int first, int last) {
  Q_UNUSED(parent);
  if (chat_model_ == nullptr || chat_view_ == nullptr) {
    return;
  }
  namespace ChatRoles = aura::mcp_assistant::models::ChatRoles;
  for (int row = first; row <= last; ++row) {
    const QModelIndex ix = chat_model_->index(row, 0);
    const int kind = ix.data(ChatRoles::KindRole).toInt();
    if (kind != static_cast<int>(chat::ChatMessageKind::Checkpoint)) {
      continue;
    }
    const qint64 cp_id = ix.data(ChatRoles::CheckpointIdRole).toLongLong();
    auto* btn = new QPushButton(tr("Rollback to this checkpoint"));
    connect(btn, &QPushButton::clicked, this, [this, cp_id]() {
      if (controller_ == nullptr) {
        return;
      }
      const auto reply =
          QMessageBox::question(this, tr("Rollback"),
                                tr("Rollback workspace to checkpoint %1?").arg(cp_id));
      if (reply != QMessageBox::Yes) {
        return;
      }
      controller_->RollbackCheckpoint(cp_id);
    });
    chat_view_->setIndexWidget(ix, btn);
  }
}

void MainWindow::ScrollChatToBottom() {
  if (chat_model_ == nullptr || chat_view_ == nullptr) {
    return;
  }
  const int rows = chat_model_->rowCount(QModelIndex());
  if (rows <= 0) {
    return;
  }
  const QModelIndex last = chat_model_->index(rows - 1, 0);
  chat_view_->scrollTo(last, QAbstractItemView::PositionAtBottom);
}

}  // namespace aura::mcp_assistant::views
