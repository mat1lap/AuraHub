#include "mcp_assistant/checkpoint/checkpoint_list_model.h"

#include <QDateTime>

namespace aura::mcp_assistant::checkpoint {

CheckpointListModel::CheckpointListModel(QObject* parent) : QAbstractListModel(parent) {}

int CheckpointListModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(rows_.size());
}

QVariant CheckpointListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(rows_.size())) {
    return {};
  }
  const CheckpointRecord& r = rows_[static_cast<std::size_t>(index.row())];
  switch (role) {
    case Qt::DisplayRole:
      return QStringLiteral("%1 — %2")
          .arg(QDateTime::fromMSecsSinceEpoch(r.created_epoch_ms).toString(
              QStringLiteral("yyyy-MM-dd hh:mm")))
          .arg(r.summary);
    case Qt::ToolTipRole:
      return r.user_prompt_excerpt;
    case roles::IdRole:
      return QVariant::fromValue(r.id);
    case roles::CreatedMsRole:
      return r.created_epoch_ms;
    case roles::SummaryRole:
      return r.summary;
    case roles::PromptExcerptRole:
      return r.user_prompt_excerpt;
    default:
      return {};
  }
}

QHash<int, QByteArray> CheckpointListModel::roleNames() const {
  QHash<int, QByteArray> r;
  r.insert(roles::IdRole, "checkpointId");
  r.insert(roles::CreatedMsRole, "createdMs");
  r.insert(roles::SummaryRole, "summary");
  r.insert(roles::PromptExcerptRole, "prompt");
  return r;
}

void CheckpointListModel::UpsertCheckpoint(const CheckpointRecord& record) {
  for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
    if (rows_[static_cast<std::size_t>(i)].id == record.id) {
      rows_[static_cast<std::size_t>(i)] = record;
      const QModelIndex idx = index(i, 0);
      emit dataChanged(idx, idx);
      return;
    }
  }
  const int row = static_cast<int>(rows_.size());
  beginInsertRows(QModelIndex(), row, row);
  rows_.push_back(record);
  endInsertRows();
}

QModelIndex CheckpointListModel::IndexOfCheckpointId(qint64 id) const {
  for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
    if (rows_[static_cast<std::size_t>(i)].id == id) {
      return index(i, 0);
    }
  }
  return {};
}

}  // namespace aura::mcp_assistant::checkpoint
