#include "auraclient/gui/action_log_table_model.h"

#include <QDateTime>
#include <QLocale>
#include <QString>
#include <QTimeZone>

namespace auraclient::gui {

ActionLogTableModel::ActionLogTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int ActionLogTableModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int ActionLogTableModel::columnCount(const QModelIndex& parent) const {
  (void)parent;
  return 5;  // id, ts, tool, target, status
}

QVariant ActionLogTableModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || role != Qt::DisplayRole) return {};
  if (index.row() < 0 || index.row() >= static_cast<int>(rows_.size())) return {};

  const auto& r = rows_[static_cast<size_t>(index.row())];
  switch (index.column()) {
    case 0:
      return QVariant::fromValue<qlonglong>(r.id);
    case 1: {
      const auto dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(r.ts_utc_ms),
                                                      QTimeZone::utc())
                          .toLocalTime();
      return QStringLiteral("%1 (%2)")
          .arg(QLocale().toString(dt, QLocale::ShortFormat))
          .arg(static_cast<qulonglong>(r.ts_utc_ms));
    }
    case 2:
      return QString::fromStdString(r.tool);
    case 3:
      return QString::fromStdString(r.target);
    case 4:
      return QString::fromStdString(r.status);
    default:
      return {};
  }
}

QVariant ActionLogTableModel::headerData(int section, Qt::Orientation orientation,
                                        int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
  switch (section) {
    case 0:
      return QStringLiteral("ID");
    case 1:
      return QStringLiteral("Время (локально)");
    case 2:
      return QStringLiteral("Инструмент");
    case 3:
      return QStringLiteral("Файл");
    case 4:
      return QStringLiteral("Статус");
    default:
      return {};
  }
}

void ActionLogTableModel::SetRows(std::vector<model::ActionLogEntry> rows) {
  beginResetModel();
  rows_ = std::move(rows);
  endResetModel();
}

model::ActionId ActionLogTableModel::ActionIdAtRow(int row) const {
  if (row < 0 || row >= static_cast<int>(rows_.size())) return 0;
  return rows_[static_cast<size_t>(row)].id;
}

}  // namespace auraclient::gui

