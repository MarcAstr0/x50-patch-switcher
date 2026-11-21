#include "app/CommandLine.h"

#include "core/Version.h"
#include "midi/RtMidiBackend.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

namespace {
void printList(QTextStream &out, const QString &title, const QStringList &names)
{
    out << title << QStringLiteral("\n");
    if (names.isEmpty()) {
        out << QStringLiteral("  (none)\n");
        return;
    }
    for (int i = 0; i < names.size(); ++i)
        out << QStringLiteral("  %1: %2\n").arg(i).arg(names.at(i));
}
} // namespace

CommandLine::Result CommandLine::process(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(Version::appName());
    // addHelpOption also registers --help-all (Qt's own option name).
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption listPorts(QStringLiteral("list-ports"),
                                       QStringLiteral("Print output and input port names and exit."));
    parser.addOption(listPorts);
    const QCommandLineOption configDir(QStringLiteral("config-dir"),
                                       QStringLiteral("Use another config directory."),
                                       QStringLiteral("dir"));
    parser.addOption(configDir);
    const QCommandLineOption portOpt(QStringLiteral("port"),
                                     QStringLiteral("MIDI output port, substring of its name (default \"X50\")."),
                                     QStringLiteral("substr"));
    parser.addOption(portOpt);
    const QCommandLineOption inPortOpt(QStringLiteral("in-port"),
                                       QStringLiteral("MIDI input port substring (default: same as --port)."),
                                       QStringLiteral("substr"));
    parser.addOption(inPortOpt);
    const QCommandLineOption channelOpt(QStringLiteral("channel"),
                                        QStringLiteral("Global channel 1-16, used when the X50 does not confirm its own."),
                                        QStringLiteral("1-16"));
    parser.addOption(channelOpt);
    const QCommandLineOption sendOpt(QStringLiteral("send"),
                                     QStringLiteral("Send one sound and exit, e.g. \"PROG A000\" or \"COMBI C127\"."),
                                     QStringLiteral("address"));
    parser.addOption(sendOpt);
    const QCommandLineOption probeOpt(QStringLiteral("probe"),
                                      QStringLiteral("Run the X50 probe, print the checklist, exit 0 if all pass."));
    parser.addOption(probeOpt);
    const QCommandLineOption diagnoseOpt(QStringLiteral("diagnose"),
                                         QStringLiteral("Probe, read every bank (read only), write report.txt and .syx captures to <dir>."),
                                         QStringLiteral("dir"));
    parser.addOption(diagnoseOpt);
    const QCommandLineOption fakeOpt(QStringLiteral("fake-midi"),
                                     QStringLiteral("Use the simulated X50 instead of real MIDI ports."));
    parser.addOption(fakeOpt);
    // --screenshot <png> [--screenshot-state <name>] arrives with M3: it needs
    // QApplication on the offscreen platform (see main.cpp).

    if (!parser.parse(arguments)) {
        QTextStream(stderr) << parser.errorText() << QStringLiteral("\n");
        m_exitCode = 2;
        return Result::Exit;
    }
    if (parser.isSet(helpOption) || parser.isSet(QStringLiteral("help-all"))) {
        // No application object exists yet, so Qt prints <executable_name>.
        QString help = parser.helpText();
        const QString exe = arguments.isEmpty() ? QString() : QFileInfo(arguments.first()).completeBaseName();
        help.replace(QStringLiteral("<executable_name>"),
                     exe.isEmpty() ? QStringLiteral("x50-patch-switcher") : exe);
        help += QStringLiteral(
            "\nExit codes:\n"
            "  0  ok\n"
            "  1  --probe: a check failed; --diagnose: a step failed or no X50 answered;\n"
            "     --list-ports: port enumeration failed\n"
            "  2  bad address, bad option value or unknown option\n"
            "  3  no MIDI port matched\n"
            "  4  --send: port busy or lost, or the X50 refused (NAK); --diagnose: could not write files\n"
            "  5  a microX answered (this app supports the X50 only); nothing was sent\n");
        QTextStream(stdout) << help;
        m_exitCode = 0;
        return Result::Exit;
    }
    if (parser.isSet(listPorts)) {
        const RtMidiBackend::Ports ports = RtMidiBackend::listPorts();
        QTextStream out(stdout);
        printList(out, QStringLiteral("Outputs:"), ports.outputs);
        printList(out, QStringLiteral("Inputs:"), ports.inputs);
        if (!ports.error.isEmpty())
            QTextStream(stderr) << QStringLiteral("MIDI error: ") << ports.error << QStringLiteral("\n");
        m_exitCode = ports.error.isEmpty() ? 0 : 1;
        return Result::Exit;
    }
    m_options = CliOptions();
    m_options.configDir = parser.value(configDir);
    m_options.port = parser.value(portOpt);
    m_options.inPort = parser.value(inPortOpt);
    m_options.fakeMidi = parser.isSet(fakeOpt);
    auto usageError = [this](const QString &text) {
        QTextStream(stderr) << text << QStringLiteral("\n");
        m_exitCode = ExitCode::BadUsage;
        return Result::Exit;
    };
    if (parser.isSet(channelOpt)) {
        bool ok = false;
        const int ch = parser.value(channelOpt).toInt(&ok);
        if (!ok || ch < 1 || ch > 16)
            return usageError(QStringLiteral("--channel must be a number from 1 to 16"));
        m_options.channel = ch;
    }
    const int jobs = int(parser.isSet(sendOpt)) + int(parser.isSet(probeOpt)) + int(parser.isSet(diagnoseOpt));
    if (jobs > 1)
        return usageError(QStringLiteral("Use only one of --send, --probe and --diagnose"));
    if (parser.isSet(sendOpt)) {
        const QString text = parser.value(sendOpt);
        const SoundAddress::ParseResult pr = SoundAddress::parse(text);
        if (!pr.ok())
            return usageError(QStringLiteral("Bad address \"%1\": %2").arg(text, pr.error));
        if (pr.matches.size() > 1)
            return usageError(QStringLiteral("Address \"%1\" is ambiguous (%2 or %3): say PROG or COMBI")
                                  .arg(text, pr.matches.at(0).format(), pr.matches.at(1).format()));
        m_options.job = CliOptions::Job::Send;
        m_options.address = pr.matches.first();
        return Result::Run;
    }
    if (parser.isSet(probeOpt)) {
        m_options.job = CliOptions::Job::Probe;
        return Result::Run;
    }
    if (parser.isSet(diagnoseOpt)) {
        const QString dir = parser.value(diagnoseOpt);
        if (dir.trimmed().isEmpty())
            return usageError(QStringLiteral("--diagnose needs a directory"));
        m_options.job = CliOptions::Job::Diagnose;
        m_options.diagnoseDir = QDir(dir).absolutePath();
        return Result::Run;
    }
    return Result::Gui;
}
