#include "app/CommandLine.h"
#include "core/Version.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStringList>

int main(int argc, char *argv[])
{
    // Arguments are parsed before any application object exists, so CLI-only
    // flags (--list-ports, --help, errors) work with no display. Qt allows one
    // application object per process, so it is chosen afterwards.
    QStringList args;
    for (int i = 0; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    QCoreApplication::setApplicationName(Version::appName());
    QCoreApplication::setApplicationVersion(Version::appVersion());

    CommandLine cli;
    switch (cli.process(args)) {
    case CommandLine::Result::Exit:
        return cli.exitCode();
    // Hook for later flags (SPEC 6): --send, --probe and --diagnose are
    // non-GUI and would construct a QCoreApplication here and run its loop;
    // --screenshot needs QApplication on the offscreen platform.
    case CommandLine::Result::Gui:
        break;
    }

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return QApplication::exec();
}
