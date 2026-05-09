#include "auraclient/model/local_db_service_qt.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QString>
#include <QVariant>

namespace auraclient::model {
namespace {

QString DefaultDbPath() {
  const auto base =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  QDir().mkpath(base);
  return QDir(base).filePath("aurahub.sqlite");
}

QString HashHex(const QByteArray& bytes) {
  const auto h = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
  return QString::fromLatin1(h.toHex());
}

}  // namespace

LocalDbServiceQt::LocalDbServiceQt() {
  connection_name_ =
      QString("aurahub_%1").arg(reinterpret_cast<quintptr>(this));
}

LocalDbServiceQt::~LocalDbServiceQt() {
  const auto name = connection_name_;
  if (db_.isValid()) {
    db_.close();
  }
  db_ = {};
  QSqlDatabase::removeDatabase(name);
}

bool LocalDbServiceQt::EnsureOpen() {
  if (db_.isValid() && db_.isOpen()) return true;

  db_ = QSqlDatabase::addDatabase("QSQLITE", connection_name_);
  db_.setDatabaseName(DefaultDbPath());
  if (!db_.open()) return false;

  QSqlQuery prag(db_);
  (void)prag.exec(QStringLiteral("PRAGMA journal_mode=WAL;"));
  return true;
}

bool LocalDbServiceQt::Exec(const char* sql) const {
  QSqlQuery q(db_);
  return q.exec(QString::fromUtf8(sql));
}

bool LocalDbServiceQt::EnsureSchema() {
  if (!EnsureOpen()) return false;

  // Baseline schema: action_log + file_snapshot.
  // For now migrations are "create if not exists".
  const bool ok1 = Exec(R"sql(
CREATE TABLE IF NOT EXISTS action_log (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  ts_utc_ms    INTEGER NOT NULL,
  tool         TEXT    NOT NULL,
  target       TEXT    NOT NULL,
  status       TEXT    NOT NULL,
  error        TEXT
);
)sql");

  const bool ok2 = Exec(R"sql(
CREATE TABLE IF NOT EXISTS file_snapshot (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  action_id     INTEGER NOT NULL,
  path          TEXT    NOT NULL,
  kind          TEXT    NOT NULL,
  content       BLOB    NOT NULL,
  content_hash  TEXT,
  FOREIGN KEY(action_id) REFERENCES action_log(id)
);
)sql");

  const bool ok3 = Exec(R"sql(
CREATE INDEX IF NOT EXISTS idx_file_snapshot_action_id
  ON file_snapshot(action_id);
)sql");

  const bool ok4 = Exec(R"sql(
CREATE TABLE IF NOT EXISTS approval_request (
  id                INTEGER PRIMARY KEY AUTOINCREMENT,
  created_utc_ms    INTEGER NOT NULL,
  path              TEXT    NOT NULL,
  previous_content  BLOB    NOT NULL,
  proposed_content  BLOB    NOT NULL,
  status            TEXT    NOT NULL,
  resolved_utc_ms   INTEGER,
  reject_reason     TEXT
);
)sql");

  const bool ok5 = Exec(R"sql(
CREATE INDEX IF NOT EXISTS idx_approval_request_status
  ON approval_request(status);
)sql");

  return ok1 && ok2 && ok3 && ok4 && ok5;
}

ActionId LocalDbServiceQt::InsertAction(std::string tool, std::string target) {
  if (!EnsureSchema()) return 0;

  QSqlQuery q(db_);
  q.prepare(R"sql(
INSERT INTO action_log(ts_utc_ms, tool, target, status, error)
VALUES (?, ?, ?, 'STARTED', NULL);
)sql");
  q.addBindValue(QDateTime::currentMSecsSinceEpoch());
  q.addBindValue(QString::fromStdString(tool));
  q.addBindValue(QString::fromStdString(target));

  if (!q.exec()) return 0;
  return q.lastInsertId().toLongLong();
}

bool LocalDbServiceQt::SetActionStatus(ActionId id, std::string status,
                                      std::optional<std::string> error) {
  if (!EnsureSchema()) return false;

  QSqlQuery q(db_);
  q.prepare(R"sql(
UPDATE action_log
SET status = ?, error = ?
WHERE id = ?;
)sql");
  q.addBindValue(QString::fromStdString(status));
  if (error.has_value()) {
    q.addBindValue(QString::fromStdString(*error));
  } else {
    q.addBindValue(QVariant(QMetaType::fromType<QString>()));
  }
  q.addBindValue(static_cast<qlonglong>(id));
  return q.exec();
}

bool LocalDbServiceQt::InsertSnapshot(ActionId action_id, FileSnapshot snapshot) {
  if (!EnsureSchema()) return false;

  const QByteArray content =
      QByteArray::fromStdString(snapshot.content);
  const QString hash = HashHex(content);

  QSqlQuery q(db_);
  q.prepare(R"sql(
INSERT INTO file_snapshot(action_id, path, kind, content, content_hash)
VALUES (?, ?, 'FULL_BLOB', ?, ?);
)sql");
  q.addBindValue(static_cast<qlonglong>(action_id));
  q.addBindValue(QString::fromStdString(snapshot.path));
  q.addBindValue(content);
  q.addBindValue(hash);
  return q.exec();
}

std::optional<FileSnapshot> LocalDbServiceQt::GetSnapshot(ActionId action_id) const {
  if (!db_.isValid() || !db_.isOpen()) return std::nullopt;

  QSqlQuery q(db_);
  q.prepare(R"sql(
SELECT path, content
FROM file_snapshot
WHERE action_id = ?
ORDER BY id DESC
LIMIT 1;
)sql");
  q.addBindValue(static_cast<qlonglong>(action_id));
  if (!q.exec()) return std::nullopt;
  if (!q.next()) return std::nullopt;

  FileSnapshot s;
  s.path = q.value(0).toString().toStdString();
  s.content = q.value(1).toByteArray().toStdString();
  return s;
}

std::vector<ActionLogEntry> LocalDbServiceQt::ListRecentActions(int limit) const {
  if (!db_.isValid() || !db_.isOpen()) return {};

  QSqlQuery q(db_);
  q.prepare(R"sql(
SELECT id, ts_utc_ms, tool, target, status, COALESCE(error, '')
FROM action_log
ORDER BY id DESC
LIMIT ?;
)sql");
  q.addBindValue(limit);
  if (!q.exec()) return {};

  std::vector<ActionLogEntry> out;
  while (q.next()) {
    ActionLogEntry e;
    e.id = q.value(0).toLongLong();
    e.ts_utc_ms = q.value(1).toLongLong();
    e.tool = q.value(2).toString().toStdString();
    e.target = q.value(3).toString().toStdString();
    e.status = q.value(4).toString().toStdString();
    e.error = q.value(5).toString().toStdString();
    out.push_back(std::move(e));
  }
  return out;
}

ApprovalRequestId LocalDbServiceQt::InsertApprovalRequest(std::string path,
                                                        std::string previous_content,
                                                        std::string proposed_content) {
  if (!EnsureSchema()) return 0;

  QSqlQuery q(db_);
  q.prepare(R"sql(
INSERT INTO approval_request(
  created_utc_ms, path, previous_content, proposed_content, status)
VALUES (?, ?, ?, ?, ?);
)sql");
  q.addBindValue(QDateTime::currentMSecsSinceEpoch());
  q.addBindValue(QString::fromStdString(path));
  q.addBindValue(QByteArray::fromStdString(previous_content));
  q.addBindValue(QByteArray::fromStdString(proposed_content));
  q.addBindValue(QString::fromLatin1(kApprovalStatusPending));
  if (!q.exec()) return 0;
  return q.lastInsertId().toLongLong();
}

std::optional<ApprovalEntry> LocalDbServiceQt::GetApproval(ApprovalRequestId id) const {
  if (!db_.isValid() || !db_.isOpen()) return std::nullopt;

  QSqlQuery q(db_);
  q.prepare(R"sql(
SELECT id, created_utc_ms, path, previous_content, proposed_content, status,
       COALESCE(reject_reason, '')
FROM approval_request
WHERE id = ?;
)sql");
  q.addBindValue(static_cast<qlonglong>(id));
  if (!q.exec() || !q.next()) return std::nullopt;

  ApprovalEntry e;
  e.id = q.value(0).toLongLong();
  e.created_utc_ms = q.value(1).toLongLong();
  e.path = q.value(2).toString().toStdString();
  e.previous_content = q.value(3).toByteArray().toStdString();
  e.proposed_content = q.value(4).toByteArray().toStdString();
  e.status = q.value(5).toString().toStdString();
  const std::string rr = q.value(6).toString().toStdString();
  if (!rr.empty()) {
    e.reject_reason = rr;
  }
  return e;
}

bool LocalDbServiceQt::ResolveApproval(ApprovalRequestId id, bool approve,
                                       std::optional<std::string> reason) {
  if (!EnsureSchema()) return false;

  QSqlQuery q(db_);
  q.prepare(R"sql(
UPDATE approval_request
SET status = ?,
    resolved_utc_ms = ?,
    reject_reason = ?
WHERE id = ? AND status = ?;
)sql");
  q.addBindValue(QString::fromStdString(
      approve ? kApprovalStatusApproved : kApprovalStatusRejected));
  q.addBindValue(QDateTime::currentMSecsSinceEpoch());
  if (!approve && reason.has_value()) {
    q.addBindValue(QString::fromStdString(*reason));
  } else {
    q.addBindValue(QVariant(QMetaType::fromType<QString>()));
  }
  q.addBindValue(static_cast<qlonglong>(id));
  q.addBindValue(QString::fromLatin1(kApprovalStatusPending));
  if (!q.exec()) return false;
  return q.numRowsAffected() > 0;
}

std::vector<ApprovalEntry> LocalDbServiceQt::ListPendingApprovals() const {
  if (!db_.isValid() || !db_.isOpen()) return {};

  QSqlQuery q(db_);
  q.prepare(R"sql(
SELECT id, created_utc_ms, path, previous_content, proposed_content, status,
       COALESCE(reject_reason, '')
FROM approval_request
WHERE status = ?
ORDER BY id ASC;
)sql");
  q.addBindValue(QString::fromLatin1(kApprovalStatusPending));

  if (!q.exec()) return {};

  std::vector<ApprovalEntry> out;
  while (q.next()) {
    ApprovalEntry e;
    e.id = q.value(0).toLongLong();
    e.created_utc_ms = q.value(1).toLongLong();
    e.path = q.value(2).toString().toStdString();
    e.previous_content = q.value(3).toByteArray().toStdString();
    e.proposed_content = q.value(4).toByteArray().toStdString();
    e.status = q.value(5).toString().toStdString();
    const std::string rr = q.value(6).toString().toStdString();
    if (!rr.empty()) {
      e.reject_reason = rr;
    }
    out.push_back(std::move(e));
  }
  return out;
}

}  // namespace auraclient::model

