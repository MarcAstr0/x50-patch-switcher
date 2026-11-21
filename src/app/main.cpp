#include "app/CliRunner.h"
#include "app/CommandLine.h"
#include "app/DiagnoseRunner.h"
#include "app/ScreenshotRunner.h"
#include "core/Catalogue.h"
#include "core/ConfigLocation.h"
#include "core/Library.h"
#include "core/Version.h"
#include "midi/X50Link.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <memory>

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
        QCoreApplication app(argc, argv);
        if (cli.options().job == CliOptions::Job::Diagnose)
            return DiagnoseRunner(cli.options()).run();
        return CliRunner(cli.options()).run();
    }
    case CommandLine::Result::Gui:
        break;
    }

    if (cli.options().job == CliOptions::Job::Screenshot) {
        // QApplication on the offscreen platform at the requested scale; both are
        // environment variables Qt reads when the application object is created.
        ScreenshotRunner::prepareEnvironment(cli.options());
        QApplication shotApp(argc, argv);
        return ScreenshotRunner::run(cli.options());
    }

    QApplication app(argc, argv);
    Theme::loadFonts();
    Theme::applyStyleSheet(app);
    // Library, catalogue, MIDI link and window. Declaration order is destruction order
    // reversed: the window goes first, then the link, then its backend.
    Library library;
    library.load(ConfigLocation::resolveForApp(cli.configDir()));
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &library, [&library]() { library.flush(); });
    Catalogue catalogue;
    QString catalogueError;
    if (!catalogue.loadFromResource(&catalogueError))
        qWarning("%s", qPrintable(catalogueError));
    std::unique_ptr<MidiBackend> backend = CliCommon::makeBackend(cli.options().fakeMidi);
    X50Link link(backend.get());
    // Settings (T3.4) will store the ports; until then the CLI or the default "X50" decides.
    const QString outSub = cli.options().port.isEmpty() ? CliCommon::defaultPortSubstring() : cli.options().port;
    link.setPortNames(outSub, cli.options().inPort.isEmpty() ? outSub : cli.options().inPort);
    if (cli.options().channel > 0)
        link.setManualChannel(cli.options().channel);
    MainWindow window(&catalogue, &library, &link);
    window.show();
    link.start();  // never blocks: connecting and probing run on the event loop
    const int rc = QApplication::exec();
    link.stop();
    library.flush();
    return rc;
}
