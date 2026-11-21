#include "app/CommandLine.h"
#include "core/Version.h"
#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(Version::appName());
    QApplication::setApplicationVersion(Version::appVersion());

    CommandLine cli;
    if (!cli.process(QApplication::arguments()))
        return cli.exitCode();

    MainWindow window;
    window.show();
    return QApplication::exec();
}
