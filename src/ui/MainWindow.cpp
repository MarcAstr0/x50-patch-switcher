#include "ui/MainWindow.h"

#include "core/Version.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(Version::appName());
    resize(900, 600);
}
