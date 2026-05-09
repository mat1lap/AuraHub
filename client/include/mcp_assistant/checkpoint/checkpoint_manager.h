#pragma once

#include "checkpoint_record.h"

#include <QObject>
#include <QString>

namespace aura::mcp_assistant::checkpoint {

/// Hybrid strategy: prefer git metadata when repo detected; always persist FS snapshot under
/// QStandardPaths for safe rollback without destructive git operations in v1.
class CheckpointManager final : public QObject {
  Q_OBJECT

 public:
  explicit CheckpointManager(QObject* parent = nullptr);

  void SetWorkspaceRoot(QString absolute_path);

  [[nodiscard]] QString WorkspaceRoot() const { return workspace_root_; }

  /// Captures a filesystem snapshot and registers metadata.
  CheckpointRecord CreateCheckpoint(QString user_prompt_excerpt, QString summary);

  /// Asynchronous rollback from snapshot `checkpoint_id`.
  void RollbackAsync(qint64 checkpoint_id);

 signals:
  void CheckpointCreated(CheckpointRecord record);
  void RollbackFinished(qint64 checkpoint_id, bool ok, QString error_message);

 private:
  QString workspace_root_;
  QString snapshot_store_root_;
  qint64 next_id_{1};

  [[nodiscard]] QString SnapshotDirForId(qint64 id) const;
};

}  // namespace aura::mcp_assistant::checkpoint
