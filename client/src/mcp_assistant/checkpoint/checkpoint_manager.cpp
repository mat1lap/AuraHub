#include "mcp_assistant/checkpoint/checkpoint_manager.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrent>

#include <memory>

namespace aura::mcp_assistant::checkpoint {

namespace {

bool ShouldSkipDir(const QString& name) {
  return name == QStringLiteral(".git") || name == QStringLiteral("build") ||
         name == QStringLiteral("cmake-build-debug") || name == QStringLiteral("node_modules");
}

[[nodiscard]] bool RelPathHasSkippedComponent(const QString& rel) {
  const QStringList parts =
      rel.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  for (const QString& part : parts) {
    if (ShouldSkipDir(part)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool IsSafeRelativePath(QString rel) {
  if (rel.isEmpty() || rel.contains(QLatin1String(".."))) {
    return false;
  }
  const QFileInfo fi(rel);
  if (fi.isAbsolute()) {
    return false;
  }
  return !RelPathHasSkippedComponent(rel);
}

}  // namespace

CheckpointManager::CheckpointManager(QObject* parent) : QObject(parent) {
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  snapshot_store_root_ = base + QStringLiteral("/mcp_assistant_snapshots");
  QDir().mkpath(snapshot_store_root_);
}

void CheckpointManager::SetWorkspaceRoot(QString absolute_path) {
  workspace_root_ = std::move(absolute_path);
}

QString CheckpointManager::SnapshotDirForId(qint64 id) const {
  return snapshot_store_root_ + QStringLiteral("/cp_%1").arg(id);
}

void CheckpointManager::CreateCheckpointAsync(QString user_prompt_excerpt, QString summary,
                                              QStringList relative_paths_under_workspace) {
  if (workspace_root_.isEmpty()) {
    emit CheckpointCreationFailed(QStringLiteral("Workspace root is not set."));
    return;
  }

  const qint64 id = next_id_++;
  const QString snap_dir = SnapshotDirForId(id);
  const qint64 created_ms = QDateTime::currentMSecsSinceEpoch();
  const QString workspace = workspace_root_;

  QStringList paths_clean;
  paths_clean.reserve(relative_paths_under_workspace.size());
  for (QString rel : relative_paths_under_workspace) {
    rel = rel.trimmed();
    if (IsSafeRelativePath(rel)) {
      paths_clean.push_back(rel);
    }
  }

  auto record_out = std::make_shared<CheckpointRecord>();

  auto future = QtConcurrent::run(
      [=, prompt = std::move(user_prompt_excerpt), sum = std::move(summary)]() mutable
      -> std::pair<bool, QString> {
        CheckpointRecord rec;
        rec.id = id;
        rec.created_epoch_ms = created_ms;
        rec.user_prompt_excerpt = std::move(prompt);
        rec.summary = std::move(sum);
        rec.storage_ref = snap_dir;
        rec.storage = CheckpointStorageKind::FilesystemSnapshot;

        QDir().mkpath(snap_dir);
        const QString tree = snap_dir + QStringLiteral("/tree");

        if (paths_clean.isEmpty()) {
          QDir().mkpath(tree);
          QFile marker(tree + QStringLiteral("/.checkpoint_empty"));
          if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            marker.write("\n");
          }
          *record_out = rec;
          return {true, QString()};
        }

        QDir().mkpath(tree);
        for (const QString& rel : paths_clean) {
          const QString src = QDir(workspace).filePath(rel);
          const QString dst = tree + QLatin1Char('/') + rel;
          QDir().mkpath(QFileInfo(dst).absolutePath());
          if (!QFile::exists(src)) {
            continue;
          }
          if (QFile::exists(dst)) {
            QFile::remove(dst);
          }
          if (!QFile::copy(src, dst)) {
            return {false, QStringLiteral("Failed to snapshot: %1").arg(rel)};
          }
        }
        *record_out = rec;
        return {true, QString()};
      });

  auto* watcher = new QFutureWatcher<std::pair<bool, QString>>(this);
  connect(watcher, &QFutureWatcher<std::pair<bool, QString>>::finished, this,
          [this, watcher, record_out]() {
            const auto [ok, err] = watcher->result();
            watcher->deleteLater();
            if (!ok) {
              emit CheckpointCreationFailed(err);
              return;
            }
            emit CheckpointCreated(*record_out);
          });
  watcher->setFuture(future);
}

void CheckpointManager::RollbackAsync(qint64 checkpoint_id) {
  const QString snap = SnapshotDirForId(checkpoint_id);
  const QString tree = snap + QStringLiteral("/tree");

  auto future = QtConcurrent::run([this, checkpoint_id, tree]() -> std::pair<bool, QString> {
    if (!QDir(tree).exists()) {
      return {false, QStringLiteral("Snapshot data missing.")};
    }
    if (workspace_root_.isEmpty()) {
      return {false, QStringLiteral("Workspace root is not set.")};
    }
    QDirIterator it(tree, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      const QString src_path = it.next();
      QFileInfo fi(src_path);
      const QString rel = QDir(tree).relativeFilePath(src_path);
      if (rel == QStringLiteral(".checkpoint_empty")) {
        continue;
      }
      const QString dst_path = workspace_root_ + QLatin1Char('/') + rel;
      if (fi.isDir()) {
        QDir().mkpath(dst_path);
        continue;
      }
      QDir().mkpath(QFileInfo(dst_path).absolutePath());
      if (QFile::exists(dst_path)) {
        QFile::remove(dst_path);
      }
      if (!QFile::copy(src_path, dst_path)) {
        return {false, QStringLiteral("Failed to restore file: %1").arg(rel)};
      }
    }
    return {true, QString()};
  });

  auto* watcher = new QFutureWatcher<std::pair<bool, QString>>(this);
  connect(watcher, &QFutureWatcher<std::pair<bool, QString>>::finished, this,
          [this, watcher, checkpoint_id]() {
            const auto result = watcher->result();
            emit RollbackFinished(checkpoint_id, result.first,
                                  result.second);
            watcher->deleteLater();
          });
  watcher->setFuture(future);
}

}  // namespace aura::mcp_assistant::checkpoint
