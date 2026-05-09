#pragma once

#include "auraclient/model/action_log_entry.h"

#include <QAbstractTableModel>

#include <vector>

namespace auraclient::gui {

class ActionLogTableModel final : public QAbstractTableModel {
 public:
  explicit ActionLogTableModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  void SetRows(std::vector<model::ActionLogEntry> rows);
  model::ActionId ActionIdAtRow(int row) const;

 private:
  std::vector<model::ActionLogEntry> rows_;
};

}  // namespace auraclient::gui

