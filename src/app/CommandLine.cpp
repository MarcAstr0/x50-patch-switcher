#include "app/CommandLine.h"

#include "core/Version.h"
#include "midi/RtMidiBackend.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
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

bool CommandLine::process(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(Version::appName());
    parser.addHelpOption();
    const QCommandLineOption listPorts(QStringLiteral("list-ports"),
                                       QStringLiteral("Print output and input port names and exit."));
    parser.addOption(listPorts);

    if (!parser.parse(arguments)) {
        QTextStream(stderr) << parser.errorText() << QStringLiteral("\n");
        m_exitCode = 2;
        return false;
    }
    if (parser.isSet(QStringLiteral("help"))) {
        QTextStream(stdout) << parser.helpText();
        m_exitCode = 0;
        return false;
    }
    if (parser.isSet(listPorts)) {
        const RtMidiBackend::Ports ports = RtMidiBackend::listPorts();
        QTextStream out(stdout);
        printList(out, QStringLiteral("Outputs:"), ports.outputs);
        printList(out, QStringLiteral("Inputs:"), ports.inputs);
        if (!ports.error.isEmpty())
            QTextStream(stderr) << QStringLiteral("MIDI error: ") << ports.error << QStringLiteral("\n");
        m_exitCode = 0;
        return false;
    }
    return true;
}
