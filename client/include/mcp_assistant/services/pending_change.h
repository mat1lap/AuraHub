#pragma once

#include <QString>
#include <QVector>

namespace aura::mcp_assistant::services {

struct FilePatch {
  QString relative_path;
  QString before_text;
  QString after_text;
};

struct PendingChange {
  qint64 id{0};
  QString title;
  QVector<FilePatch> files;
};

}  // namespace aura::mcp_assistant::services
