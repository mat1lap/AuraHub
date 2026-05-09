#pragma once

#include "auraclient/gui/action_log_table_model.h"
#include "auraclient/mvp/iaura_view.h"

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QGroupBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTableView;
class QTimer;
QT_END_NAMESPACE

namespace auraclient::mvp {
class IAuraPresenter;
}

namespace auraclient::gui {

class MainWindow final : public QMainWindow, public mvp::IAuraView {
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  void SetPresenter(mvp::IAuraPresenter* presenter);

  void SetRollbackEnabled(bool enabled) override;
  void ShowStatusMessage(const char* message) override;
  void SetActionLogRows(std::vector<model::ActionLogEntry> rows) override;
  void SetDemoPanel(const char* absolute_path_utf8, const char* file_preview_utf8,
                    const char* hint_utf8) override;

  void SetPendingApprovals(std::vector<model::ApprovalEntry> rows) override;
  void SetDemoEditInProgress(bool in_progress) override;

  ActionLogTableModel& Model();

 private:
  void OnDemoEditClicked();
  void OnRollbackClicked();
  void OnTableSelectionChanged();
  void UpdateRollbackButtonState();

  void OnApprovalTimer();
  void OnApproveClicked();
  void OnRejectClicked();
  void OnApprovalSelectionChanged();
  void UpdateApprovalDiffFromSelection();

 private:
  mvp::IAuraPresenter* presenter_{nullptr};  // non-owning
  ActionLogTableModel* table_model_{nullptr};
  QTableView* table_{nullptr};
  QLabel* intro_{nullptr};
  QGroupBox* demo_group_{nullptr};
  QLabel* demo_path_{nullptr};
  QPlainTextEdit* demo_preview_{nullptr};
  QLabel* demo_hint_{nullptr};
  QGroupBox* log_group_{nullptr};
  QPushButton* demo_edit_{nullptr};
  QPushButton* rollback_{nullptr};
  QLabel* status_{nullptr};
  bool rollback_allowed_by_db_{false};

  QTimer* approval_timer_{nullptr};
  QGroupBox* approval_group_{nullptr};
  QListWidget* approval_list_{nullptr};
  QPlainTextEdit* approval_prev_{nullptr};
  QPlainTextEdit* approval_next_{nullptr};
  QPushButton* approve_btn_{nullptr};
  QPushButton* reject_btn_{nullptr};
  std::vector<model::ApprovalEntry> approval_rows_cache_;
};

}  // namespace auraclient::gui

