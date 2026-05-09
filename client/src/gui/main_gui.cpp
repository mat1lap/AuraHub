#include "auraclient/gui/main_window.h"
#include "auraclient/mvp/aura_presenter.h"
#include "auraclient/model/local_db_service_qt.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char** argv) {
  QCoreApplication::setApplicationName(QStringLiteral("AuraHub"));
  QCoreApplication::setOrganizationName(QStringLiteral("AuraHub"));

  QApplication app(argc, argv);

  auraclient::gui::MainWindow w;
  auto db = std::make_shared<auraclient::model::LocalDbServiceQt>();
  auraclient::mvp::AuraPresenter presenter(w, db);
  w.SetPresenter(&presenter);

  w.show();
  presenter.OnAppStarted();
  return app.exec();
}

