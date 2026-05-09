#include "mcp_assistant/app/application.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>

namespace aura::mcp_assistant::app {

void ApplyAssistantTheme(QApplication* app) {
  app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

  QPalette palette;
  palette.setColor(QPalette::Window, QColor(30, 30, 34));
  palette.setColor(QPalette::WindowText, Qt::white);
  palette.setColor(QPalette::Base, QColor(24, 24, 28));
  palette.setColor(QPalette::AlternateBase, QColor(36, 36, 42));
  palette.setColor(QPalette::ToolTipBase, QColor(240, 240, 245));
  palette.setColor(QPalette::ToolTipText, QColor(20, 20, 24));
  palette.setColor(QPalette::Text, QColor(235, 235, 240));
  palette.setColor(QPalette::Button, QColor(46, 46, 52));
  palette.setColor(QPalette::ButtonText, Qt::white);
  palette.setColor(QPalette::BrightText, Qt::red);
  palette.setColor(QPalette::Highlight, QColor(59, 130, 246));
  palette.setColor(QPalette::HighlightedText, Qt::white);
  app->setPalette(palette);

  QFile qss(QStringLiteral(":/themes/dark.qss"));
  if (qss.open(QIODevice::ReadOnly)) {
    app->setStyleSheet(QString::fromUtf8(qss.readAll()));
  }
}

}  // namespace aura::mcp_assistant::app
