#pragma once

#include "checkpoint_record.h"

#include <QAbstractListModel>
#include <vector>

namespace aura::mcp_assistant::checkpoint {

namespace roles {
inline constexpr int IdRole = Qt::UserRole + 1;
inline constexpr int CreatedMsRole = Qt::UserRole + 2;
inline constexpr int SummaryRole = Qt::UserRole + 3;
inline constexpr int PromptExcerptRole = Qt::UserRole + 4;
}  // namespace roles

class CheckpointListModel final : public QAbstractListModel {
  Q_OBJECT

 public:
  explicit CheckpointListModel(QObject* parent = nullptr);

  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void UpsertCheckpoint(const CheckpointRecord& record);

  [[nodiscard]] QModelIndex IndexOfCheckpointId(qint64 id) const;

 private:
  std::vector<CheckpointRecord> rows_;
};

}  // namespace aura::mcp_assistant::checkpoint
