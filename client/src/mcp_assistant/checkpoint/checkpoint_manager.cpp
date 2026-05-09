#include "mcp_assistant/checkpoint/checkpoint_manager.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrent>

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

bool CopyRecursiveFiltered(const QString& src_root, const QString& dst_root) {
  QDir().mkpath(dst_root);
  QDirIterator it(src_root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString path = it.next();
    QFileInfo fi(path);
    const QString rel = QDir(src_root).relativeFilePath(path);
    if (RelPathHasSkippedComponent(rel)) {
      continue;
    }
    if (fi.isDir()) {
      QDir().mkpath(dst_root + QLatin1Char('/') + rel);
      continue;
    }
    const QString dest_path = dst_root + QLatin1Char('/') + rel;
    QDir().mkpath(QFileInfo(dest_path).absolutePath());
    if (QFile::exists(dest_path)) {
      QFile::remove(dest_path);
    }
    if (!QFile::copy(path, dest_path)) {
      return false;
    }
  }
  return true;
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

CheckpointRecord CheckpointManager::CreateCheckpoint(QString user_prompt_excerpt,
                                                    QString summary) {
  CheckpointRecord rec;
  rec.id = next_id_++;
  rec.created_epoch_ms =
      QDateTime::currentMSecsSinceEpoch();
  rec.user_prompt_excerpt = std::move(user_prompt_excerpt);
  rec.summary = std::move(summary);
  rec.storage_ref = SnapshotDirForId(rec.id);
  rec.storage = CheckpointStorageKind::FilesystemSnapshot;

  const QString snap_dir = rec.storage_ref;
  QDir().mkpath(snap_dir);

  if (!workspace_root_.isEmpty()) {
    CopyRecursiveFiltered(workspace_root_, snap_dir + QStringLiteral("/tree"));
  }

  emit CheckpointCreated(rec);
  return rec;
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
