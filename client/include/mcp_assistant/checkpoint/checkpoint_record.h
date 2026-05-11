#pragma once

#include <QString>

namespace aura::mcp_assistant::checkpoint {

enum class CheckpointStorageKind {
  FilesystemSnapshot,
  GitStashRef,
};

struct CheckpointRecord {
  qint64 id{0};
  qint64 created_epoch_ms{0};
  QString user_prompt_excerpt;
  QString summary;
  QString storage_ref;
  CheckpointStorageKind storage{CheckpointStorageKind::FilesystemSnapshot};
};

}  // namespace aura::mcp_assistant::checkpoint
