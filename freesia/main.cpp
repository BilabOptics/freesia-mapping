#include <QApplication>
#include <QDesktopWidget>
#include <string>

#include "common.h"
#include "mainwindow.h"


int main(int argc, char *argv[]) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  if (argc == 2) {
    Common::i()->p_atlasIndex = std::stoi(argv[1]);
  }

  QApplication a(argc, argv);
  MainWindow w;

  w.setGeometry(0, 0, 1200, 700);
  w.move(QApplication::desktop()->screen()->rect().center() -
         w.rect().center());

  w.showMaximized();

  return a.exec();
}
