#include "app/CliRunner.h"
#include "app/CommandLine.h"
#include "app/DiagnoseRunner.h"
#include "core/ConfigLocation.h"
#include "core/Library.h"
#include "core/Version.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
// On Windows argv is in the ANSI code page, which mangles non-ANSI user names in paths;
// the wide command line is exact.
QStringList commandLineArguments(int argc, char *argv[])
{
    QStringList args;
#ifdef Q_OS_WIN
    int wargc = 0;
    if (LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc)) {
        for (int i = 0; i < wargc; ++i)
            args << QString::fromWCharArray(wargv[i]);
        LocalFree(wargv);
    }
    if (!args.isEmpty())
        return args;
#endif
    for (int i = 0; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    return args;
}
} // namespace

int main(int argc, char *argv[])
{
    // Arguments are parsed before any application object exists, so CLI-only
    // flags (--list-ports, --help, errors) work with no display. Qt allows one
    // application object per process, so it is chosen afterwards.
    const QStringList args = commandLineArguments(argc, argv);
    QCoreApplication::setApplicationName(Version::appName());
    QCoreApplication::setApplicationVersion(Version::appVersion());

    CommandLine cli;
    switch (cli.process(args)) {
    case CommandLine::Result::Exit:
        return cli.exitCode();
    case CommandLine::Result::Run: {
        // --send, --probe, --diagnose: no widgets, but X50Link needs an event loop.
        // Hook for M3: --screenshot needs QApplication on the offscreen platform.
        QCoreApplication app(argc, argv);
        if (cli.options().job == CliOptions::Job::Diagnose)
            return DiagnoseRunner(cli.options()).run();
        return CliRunner(cli.options()).run();
    }
    case CommandLine::Result::Gui:
        break;
    }

    QApplication app(argc, argv);
    // The library is loaded here until MainWindow takes it over (M3); it only
    // writes when something changes, and is flushed on exit.
    Library library;
    library.load(ConfigLocation::resolveForApp(cli.configDir()));
    if (!library.loadError().isEmpty())
        qWarning("%s", qPrintable(library.loadError()));
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &library, [&library]() { library.flush(); });
    MainWindow window;
    window.show();
    const int rc = QApplication::exec();
    library.flush();
    return rc;
}
