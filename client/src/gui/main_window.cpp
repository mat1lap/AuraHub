#include "auraclient/gui/main_window.h"

#include "auraclient/mvp/iaura_presenter.h"

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace auraclient::gui {

namespace {

constexpr char kIntroHtml[] =
    "<p style='margin-top:0'><span style='font-size:14pt;font-weight:600'>Демо «Time Machine»</span></p>"
    "<p><b>Зачем это окно:</b> показать, как AuraHub перед записью в файл сохраняет "
    "<i>снимок предыдущего содержимого</i> в локальной базе SQLite и может вернуть файл "
    "к этому состоянию.</p>"
    "<p><b>Одобрение изменений:</b> любой вызов инструмента <code>edit_file</code> (из этого окна или из "
    "<code>auraclient_mcp_host</code>) сначала попадает в очередь ниже. Пока статус "
    "<code>PENDING</code>, файл на диске <b>не</b> меняется. Нажмите «Одобрить» или «Отклонить». "
    "Без одобрения MCP-процесс ждёт до 15 минут, затем запрос отменяется.</p>"
    "<p><b>Кнопка «Записать демо в файл»</b> — ставит такой же запрос на одобрение; после одобрения "
    "в журнал попадает действие <code>edit_file</code>, в SQLite — снимок старого содержимого, затем "
    "файл обновляется.</p>"
    "<p><b>Кнопка «Откатить выбранное действие»</b> — выделите в журнале строку с "
    "<code>edit_file</code>. Откат восстанавливает файл из снимка.</p>";

QString LimitPreview(const QString& text) {
  constexpr int kMaxChars = 48000;
  if (text.size() <= kMaxChars) {
    return text;
  }
  return text.left(kMaxChars) +
         QString::fromUtf8(u8"\n\n… [обрезано для отображения]");
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setMinimumSize(800, 720);
  setWindowTitle(QString::fromUtf8(u8"AuraHub — Time Machine и одобрение правок"));

  auto* root = new QWidget(this);
  auto* layout = new QVBoxLayout(root);

  intro_ = new QLabel(root);
  intro_->setText(QString::fromUtf8(kIntroHtml));
  intro_->setWordWrap(true);
  intro_->setTextFormat(Qt::RichText);
  intro_->setOpenExternalLinks(false);

  demo_group_ = new QGroupBox(QString::fromUtf8(u8"Демо-файл и что сейчас на диске"), root);
  auto* demo_layout = new QVBoxLayout(demo_group_);

  demo_path_ = new QLabel(root);
  demo_path_->setWordWrap(true);
  demo_path_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  demo_preview_ = new QPlainTextEdit(root);
  demo_preview_->setReadOnly(true);
  demo_preview_->setPlaceholderText(
      QString::fromUtf8(u8"После одобренной записи здесь появится содержимое демо-файла…"));
  {
    QFont mono = demo_preview_->font();
    mono.setFamily(QStringLiteral("monospace"));
    demo_preview_->setFont(mono);
  }

  demo_hint_ = new QLabel(root);
  demo_hint_->setWordWrap(true);
  demo_hint_->setStyleSheet(QStringLiteral("color: palette(mid);"));

  demo_layout->addWidget(new QLabel(QString::fromUtf8(u8"Путь к файлу:"), root));
  demo_layout->addWidget(demo_path_);
  demo_layout->addWidget(
      new QLabel(QString::fromUtf8(u8"Содержимое файла сейчас (после последнего одобренного шага):"),
                 root));
  demo_layout->addWidget(demo_preview_, /*stretch*/ 1);
  demo_layout->addWidget(demo_hint_);

  approval_group_ =
      new QGroupBox(QString::fromUtf8(u8"Очередь одобрения правок файлов (human-in-the-loop)"), root);
  auto* appr_layout = new QVBoxLayout(approval_group_);

  approval_list_ = new QListWidget(approval_group_);
  approval_list_->setMinimumHeight(72);

  auto* diff_labels = new QHBoxLayout();
  diff_labels->addWidget(new QLabel(QString::fromUtf8(u8"Было (снимок до записи):"), root));
  diff_labels->addWidget(new QLabel(QString::fromUtf8(u8"Будет (предлагаемое содержимое):"), root));

  approval_prev_ = new QPlainTextEdit(root);
  approval_next_ = new QPlainTextEdit(root);
  approval_prev_->setReadOnly(true);
  approval_next_->setReadOnly(true);
  approval_prev_->setMinimumHeight(100);
  approval_next_->setMinimumHeight(100);
  {
    QFont mono = approval_prev_->font();
    mono.setFamily(QStringLiteral("monospace"));
    approval_prev_->setFont(mono);
    approval_next_->setFont(mono);
  }

  auto* diff_row = new QHBoxLayout();
  diff_row->addWidget(approval_prev_, 1);
  diff_row->addWidget(approval_next_, 1);

  approve_btn_ = new QPushButton(QString::fromUtf8(u8"Одобрить запись"), root);
  reject_btn_ = new QPushButton(QString::fromUtf8(u8"Отклонить"), root);
  approve_btn_->setToolTip(QString::fromUtf8(
      u8"Разрешить запись на диск и продолжить инструмент edit_file (MCP разблокируется)."));
  reject_btn_->setToolTip(QString::fromUtf8(u8"Отменить предложенную правку; MCP получит ошибку."));
  approve_btn_->setEnabled(false);
  reject_btn_->setEnabled(false);

  auto* appr_btns = new QHBoxLayout();
  appr_btns->addWidget(approve_btn_);
  appr_btns->addWidget(reject_btn_);
  appr_btns->addStretch();

  appr_layout->addWidget(new QLabel(QString::fromUtf8(u8"Ожидают решения:"), root));
  appr_layout->addWidget(approval_list_);
  appr_layout->addLayout(diff_labels);
  appr_layout->addLayout(diff_row);
  appr_layout->addLayout(appr_btns);

  log_group_ = new QGroupBox(QString::fromUtf8(u8"Журнал действий (SQLite)"), root);
  auto* log_layout = new QVBoxLayout(log_group_);

  table_model_ = new ActionLogTableModel(root);
  table_ = new QTableView(root);
  table_->setModel(table_model_);
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setAlternatingRowColors(true);
  table_->verticalHeader()->setVisible(false);

  log_layout->addWidget(table_);

  demo_edit_ = new QPushButton(QString::fromUtf8(u8"Записать демо в файл"), root);
  demo_edit_->setToolTip(QString::fromUtf8(
      u8"Создаёт запрос на правку демо-файла — сначала одобрите его в блоке выше, затем выполнится "
      u8"журнал + снимок + запись."));
  rollback_ = new QPushButton(QString::fromUtf8(u8"Откатить выбранное действие"), root);
  rollback_->setToolTip(QString::fromUtf8(
      u8"Восстановить файл из снимка для выделенной строки (нужно действие edit_file)."));
  rollback_->setEnabled(false);

  auto* btn_row = new QHBoxLayout();
  btn_row->addWidget(demo_edit_);
  btn_row->addWidget(rollback_);
  btn_row->addStretch();

  status_ = new QLabel(QString::fromUtf8(u8"Запуск…"), root);
  status_->setWordWrap(true);

  layout->addWidget(intro_);
  layout->addWidget(demo_group_, 2);
  layout->addWidget(approval_group_, 2);
  layout->addWidget(log_group_, 3);
  layout->addLayout(btn_row);
  layout->addWidget(status_);

  setCentralWidget(root);

  connect(demo_edit_, &QPushButton::clicked, this, &MainWindow::OnDemoEditClicked);
  connect(rollback_, &QPushButton::clicked, this, &MainWindow::OnRollbackClicked);
  connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &MainWindow::OnTableSelectionChanged);

  connect(approval_list_, &QListWidget::currentRowChanged, this,
          &MainWindow::OnApprovalSelectionChanged);
  connect(approve_btn_, &QPushButton::clicked, this, &MainWindow::OnApproveClicked);
  connect(reject_btn_, &QPushButton::clicked, this, &MainWindow::OnRejectClicked);
}

void MainWindow::SetPresenter(mvp::IAuraPresenter* presenter) {
  presenter_ = presenter;
  if (!approval_timer_) {
    approval_timer_ = new QTimer(this);
    connect(approval_timer_, &QTimer::timeout, this, &MainWindow::OnApprovalTimer);
    approval_timer_->start(300);
  }
}

void MainWindow::SetRollbackEnabled(bool enabled) {
  rollback_allowed_by_db_ = enabled;
  UpdateRollbackButtonState();
}

void MainWindow::ShowStatusMessage(const char* message) {
  status_->setText(QString::fromUtf8(message));
}

void MainWindow::SetActionLogRows(std::vector<model::ActionLogEntry> rows) {
  table_model_->SetRows(std::move(rows));
  table_->resizeColumnsToContents();
  UpdateRollbackButtonState();
}

void MainWindow::SetDemoPanel(const char* absolute_path_utf8, const char* file_preview_utf8,
                              const char* hint_utf8) {
  demo_path_->setText(QString::fromUtf8(absolute_path_utf8));
  demo_preview_->setPlainText(QString::fromUtf8(file_preview_utf8));
  demo_hint_->setText(QString::fromUtf8(hint_utf8));
}

void MainWindow::SetPendingApprovals(std::vector<model::ApprovalEntry> rows) {
  approval_rows_cache_ = std::move(rows);

  std::int64_t prev_sel = -1;
  if (approval_list_->currentItem()) {
    prev_sel = approval_list_->currentItem()->data(Qt::UserRole).toLongLong();
  }

  approval_list_->clear();
  for (const auto& e : approval_rows_cache_) {
    auto* item = new QListWidgetItem(
        QString::fromUtf8(u8"#%1 — %2")
            .arg(static_cast<qlonglong>(e.id))
            .arg(QString::fromStdString(e.path)),
        approval_list_);
    item->setData(Qt::UserRole, QVariant(static_cast<qlonglong>(e.id)));
  }

  int row_to_sel = 0;
  if (prev_sel >= 0) {
    for (int i = 0; i < approval_list_->count(); ++i) {
      if (approval_list_->item(i)->data(Qt::UserRole).toLongLong() == prev_sel) {
        row_to_sel = i;
        break;
      }
    }
  }

  if (!approval_rows_cache_.empty()) {
    approval_list_->setCurrentRow(row_to_sel);
  } else {
    UpdateApprovalDiffFromSelection();
  }
}

void MainWindow::SetDemoEditInProgress(bool in_progress) {
  demo_edit_->setEnabled(!in_progress);
}

ActionLogTableModel& MainWindow::Model() { return *table_model_; }

void MainWindow::UpdateRollbackButtonState() {
  const bool has_row =
      table_->selectionModel() && table_->selectionModel()->hasSelection();
  rollback_->setEnabled(rollback_allowed_by_db_ && has_row);
}

void MainWindow::OnTableSelectionChanged() { UpdateRollbackButtonState(); }

void MainWindow::OnApprovalTimer() {
  if (!presenter_) return;
  presenter_->OnApprovalUiTick();
}

void MainWindow::OnApprovalSelectionChanged() { UpdateApprovalDiffFromSelection(); }

void MainWindow::UpdateApprovalDiffFromSelection() {
  QListWidgetItem* item = approval_list_->currentItem();
  if (!item) {
    approval_prev_->clear();
    approval_next_->clear();
    approve_btn_->setEnabled(false);
    reject_btn_->setEnabled(false);
    return;
  }

  const auto id = item->data(Qt::UserRole).toLongLong();
  const model::ApprovalEntry* entry = nullptr;
  for (const auto& e : approval_rows_cache_) {
    if (e.id == id) {
      entry = &e;
      break;
    }
  }

  if (!entry) {
    approval_prev_->clear();
    approval_next_->clear();
    approve_btn_->setEnabled(false);
    reject_btn_->setEnabled(false);
    return;
  }

  approval_prev_->setPlainText(LimitPreview(QString::fromStdString(entry->previous_content)));
  approval_next_->setPlainText(LimitPreview(QString::fromStdString(entry->proposed_content)));
  approve_btn_->setEnabled(true);
  reject_btn_->setEnabled(true);
}

void MainWindow::OnApproveClicked() {
  if (!presenter_) return;
  QListWidgetItem* item = approval_list_->currentItem();
  if (!item) return;
  presenter_->OnApprovePending(item->data(Qt::UserRole).toLongLong());
}

void MainWindow::OnRejectClicked() {
  if (!presenter_) return;
  QListWidgetItem* item = approval_list_->currentItem();
  if (!item) return;
  presenter_->OnRejectPending(item->data(Qt::UserRole).toLongLong());
}

void MainWindow::OnDemoEditClicked() {
  if (!presenter_) return;
  presenter_->OnDemoEditFileClicked();
}

void MainWindow::OnRollbackClicked() {
  if (!presenter_) return;
  const auto indexes = table_->selectionModel()->selectedRows();
  if (indexes.isEmpty()) {
    status_->setText(QString::fromUtf8(
        u8"Сначала выделите строку в журнале (клик по строке таблицы)."));
    return;
  }

  const int row = indexes.first().row();
  const auto id = table_model_->ActionIdAtRow(row);
  presenter_->OnRollbackClicked(id);
}

}  // namespace auraclient::gui
