#include "auraclient/mvp/aura_presenter.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#ifdef QT_CORE_LIB
#include <QApplication>
#include <QDir>
#include <QMetaObject>
#include <QStandardPaths>
#endif

namespace auraclient::mvp {

namespace {

constexpr std::size_t kPreviewMaxBytes = 24000;

std::string ReadFileUtf8Limited(const std::string& path, std::size_t max_bytes) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return {};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string s = ss.str();
  if (s.size() > max_bytes) {
    s.resize(max_bytes);
    s += "\n\n… [файл длиннее — показано начало]";
  }
  return s;
}

}  // namespace

AuraPresenter::AuraPresenter(IAuraView& view,
                             std::shared_ptr<model::ILocalDbService> db)
    : view_(view), db_(std::move(db)), mcp_(db_) {}

void AuraPresenter::RefreshDemoPanel(const std::string& hint_utf8) {
  const std::string path = DemoFilePath();
  view_.SetDemoPanel(path.c_str(), ReadFileUtf8Limited(path, kPreviewMaxBytes).c_str(),
                      hint_utf8.c_str());
}

void AuraPresenter::RefreshActionLog() {
  if (!db_) return;
  view_.SetActionLogRows(db_->ListRecentActions(/*limit=*/200));
}

void AuraPresenter::UpdatePendingApprovalsView() {
  if (!db_ || !db_->EnsureSchema()) return;
  auto rows = db_->ListPendingApprovals();
  std::string fp;
  fp.reserve(rows.size() * 24);
  for (const auto& e : rows) {
    fp.append(std::to_string(e.id));
    fp.push_back(':');
    fp.append(e.status);
    fp.push_back(';');
  }
  if (fp == pending_fingerprint_) return;
  pending_fingerprint_ = std::move(fp);
  view_.SetPendingApprovals(std::move(rows));
}

void AuraPresenter::OnAppStarted() {
  if (!db_ || !db_->EnsureSchema()) {
    view_.ShowStatusMessage("SQLite init failed");
    view_.SetRollbackEnabled(false);
    pending_fingerprint_.clear();
    view_.SetPendingApprovals({});
    RefreshDemoPanel(
        "База не открыта — превью недоступно. Проверьте права на каталог данных приложения.");
    return;
  }
  view_.ShowStatusMessage(
      "Готово. Любая запись в файл требует одобрения в блоке «Очередь одобрения». Для демо "
      "нажмите «Записать демо в файл», затем «Одобрить запись».");
  view_.SetRollbackEnabled(true);
  RefreshActionLog();
  pending_fingerprint_.clear();
  UpdatePendingApprovalsView();
  RefreshDemoPanel(
      "Шаг 1: нажмите «Записать демо в файл» — появится запрос в очереди одобрения. Шаг 2: "
      "«Одобрить запись». После этого в SQLite сохранится снимок старого содержимого и файл "
      "обновится.");
}

std::string AuraPresenter::DemoFilePath() {
#ifdef QT_CORE_LIB
  const auto base =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  QDir().mkpath(base);
  return QDir(base).filePath("demo_edited.txt").toStdString();
#else
  return (std::filesystem::temp_directory_path() / "aurahub_demo_edited.txt")
      .string();
#endif
}

void AuraPresenter::OnDemoEditFileClicked() {
  if (!db_) return;

  const std::string path = DemoFilePath();

  const auto now_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  const std::string content = "AuraHub demo edit\nms=" + std::to_string(now_ms) + "\n";

#ifdef QT_CORE_LIB
  view_.SetDemoEditInProgress(true);
  view_.ShowStatusMessage(
      "Создан запрос на запись — одобрите его в блоке «Очередь одобрения» (иначе поток ждёт "
      "до таймаута).");

  std::thread([this, path, content]() {
    const auto r = mcp_.EditFile(path, content);
    QMetaObject::invokeMethod(
        qApp,
        [this, r]() {
          view_.SetDemoEditInProgress(false);
          if (r.is_error) {
            view_.ShowStatusMessage(r.content.c_str());
            RefreshDemoPanel(
                "Запись не выполнена — см. статус. Превью — текущее состояние файла на диске.");
          } else {
            view_.ShowStatusMessage(
                "После одобрения запись выполнена: журнал, снимок в SQLite, файл обновлён.");
            RefreshDemoPanel(
                "Можно откатить через журнал или снова нажать «Записать демо в файл».");
          }
          RefreshActionLog();
          pending_fingerprint_.clear();
          UpdatePendingApprovalsView();
        },
        Qt::QueuedConnection);
  }).detach();
#else
  const auto r = mcp_.EditFile(path, content);
  if (r.is_error) {
    view_.ShowStatusMessage(r.content.c_str());
  } else {
    view_.ShowStatusMessage("ok");
  }
  RefreshActionLog();
#endif
}

void AuraPresenter::OnRollbackClicked(ActionId id) {
  if (!db_) {
    view_.ShowStatusMessage("DB not available");
    return;
  }

  const auto r = mcp_.RollbackAction(id);
  if (r.is_error) {
    view_.ShowStatusMessage(r.content.c_str());
    RefreshDemoPanel(
        "Откат не выполнен — см. сообщение внизу. Выберите строку с действием edit_file.");
  } else {
    view_.ShowStatusMessage(
        "Откат выполнен: файл на диске восстановлен из снимка для выбранного действия; "
        "статус действия в журнале обновлён.");
    RefreshDemoPanel(
        "Файл восстановлен из снимка. Можно снова нажать «Записать демо в файл», чтобы записать "
        "новую версию и новый снимок.");
  }
  RefreshActionLog();
}

void AuraPresenter::OnApprovalUiTick() { UpdatePendingApprovalsView(); }

void AuraPresenter::OnApprovePending(model::ApprovalRequestId id) {
  if (!db_ || !db_->EnsureSchema()) return;
  if (db_->ResolveApproval(id, true, std::nullopt)) {
    view_.ShowStatusMessage("Одобрено — если MCP или демо ждали, запись продолжится.");
  } else {
    view_.ShowStatusMessage(
        "Не удалось одобрить (запрос уже закрыт или не найден). Обновите список.");
  }
  pending_fingerprint_.clear();
  UpdatePendingApprovalsView();
}

void AuraPresenter::OnRejectPending(model::ApprovalRequestId id) {
  if (!db_ || !db_->EnsureSchema()) return;
  if (db_->ResolveApproval(id, false, std::nullopt)) {
    view_.ShowStatusMessage("Отклонено — ожидающий MCP получит ошибку.");
  } else {
    view_.ShowStatusMessage("Не удалось отклонить (запрос уже закрыт или не найден).");
  }
  pending_fingerprint_.clear();
  UpdatePendingApprovalsView();
}

}  // namespace auraclient::mvp

