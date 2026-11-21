#include "app/CliRunner.h"

#include "midi/FakeX50.h"
#include "midi/RtMidiBackend.h"

#include <QCoreApplication>
#include <QtGlobal>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>

namespace CliCommon {

std::unique_ptr<MidiBackend> makeBackend(bool fake)
{
    if (fake) {
        auto f = std::make_unique<FakeX50>();
        // Hidden test hook (tests/test_cli.cpp): the simulated device answers as a microX.
        if (qEnvironmentVariableIsSet("X50_FAKE_MICROX"))
            f->setMicroX(true);
        return f;
    }
    return std::make_unique<RtMidiBackend>();
}

QString defaultPortSubstring() { return QStringLiteral("X50"); }

QString hex(const QByteArray &bytes)
{
    return QString::fromLatin1(bytes.toHex(' ').toUpper());
}

QString joinNames(const QStringList &names)
{
    if (names.isEmpty())
        return QStringLiteral("(none)");
    QStringList parts;
    for (int i = 0; i < names.size(); ++i)
        parts << QStringLiteral("%1: \"%2\"").arg(i).arg(names.at(i));
    return parts.join(QStringLiteral(", "));
}

bool anyContains(const QStringList &names, const QString &substring)
{
    for (const QString &n : names)
        if (n.contains(substring, Qt::CaseInsensitive))
            return true;
    return false;
}

} // namespace CliCommon

namespace {
QTextStream &utf8(QTextStream &s)
{
    s.setEncoding(QStringConverter::Utf8);
    return s;
}

void printChecklist(QTextStream &out, const X50Link::ProbeResult &result)
{
    for (const X50Link::CheckItem &item : result.items) {
        const QChar glyph = item.status == X50Link::CheckItem::Status::Ok     ? QChar(0x2713)
                            : item.status == X50Link::CheckItem::Status::Failed ? QChar(0x2717)
                                                                                : QLatin1Char('!');
        out << glyph << QLatin1Char(' ') << item.text;
        if (!item.fix.isEmpty() && !item.text.contains(QStringLiteral("GLOBAL 1")))
            out << QStringLiteral(": ") << item.fix;
        out << QLatin1Char('\n');
        if (!item.raw.isEmpty())
            out << QStringLiteral("    raw bytes: ") << CliCommon::hex(item.raw) << QLatin1Char('\n');
    }
}
} // namespace

CliRunner::CliRunner(const CliOptions &options, QObject *parent) : QObject(parent), m_options(options) {}

int CliRunner::run()
{
    QTimer::singleShot(0, this, &CliRunner::begin);
    QCoreApplication::exec();
    return m_exit;
}

void CliRunner::finish(int code)
{
    if (m_done)
        return;
    m_done = true;
    m_exit = code;
    if (m_link)
        m_link->stop();
    QCoreApplication::exit(code);
}

void CliRunner::begin()
{
    QTextStream err(stderr);
    utf8(err);
    m_backend = CliCommon::makeBackend(m_options.fakeMidi);
    const QString outSub = m_options.port.isEmpty() ? CliCommon::defaultPortSubstring() : m_options.port;
    const QString inSub = m_options.inPort.isEmpty() ? outSub : m_options.inPort;

    const MidiBackend::Ports ports = m_backend->ports();
    if (!ports.error.isEmpty()) {
        err << QStringLiteral("MIDI is unavailable: ") << ports.error << QLatin1Char('\n');
        finish(ExitCode::Failed);
        return;
    }
    if (!CliCommon::anyContains(ports.outputs, outSub) || !CliCommon::anyContains(ports.inputs, inSub)) {
        err << QStringLiteral("No MIDI port matches \"%1\" (output) / \"%2\" (input).\n").arg(outSub, inSub)
            << QStringLiteral("Outputs: ") << CliCommon::joinNames(ports.outputs) << QLatin1Char('\n')
            << QStringLiteral("Inputs: ") << CliCommon::joinNames(ports.inputs) << QLatin1Char('\n')
            << QStringLiteral("Use --port and --in-port, or --list-ports to see the names.\n");
        finish(ExitCode::NoPort);
        return;
    }

    m_link = std::make_unique<X50Link>(m_backend.get());
    m_link->setPortNames(outSub, inSub);
    if (m_options.channel > 0)
        m_link->setManualChannel(m_options.channel);
    connect(m_link.get(), &X50Link::probeFinished, this, &CliRunner::onProbeFinished);
    connect(m_link.get(), &X50Link::sent, this, [this](const SoundAddress &a) {
        QTextStream out(stdout);
        utf8(out);
        out << QStringLiteral("Sent %1 on channel %2\n").arg(a.format()).arg(m_link->channel());
        finish(ExitCode::Ok);
    });
    connect(m_link.get(), &X50Link::sendFailed, this, [this](const SoundAddress &a, const QString &reason) {
        QTextStream e(stderr);
        utf8(e);
        e << QStringLiteral("Could not send %1: %2\n").arg(a.format(), reason);
        finish(ExitCode::SendFailed);
    });

    m_link->start();
    if (m_link->state() == X50Link::State::Disconnected) {
        // Ports exist but the open failed (busy, or the driver refused).
        err << m_link->statusText() << QLatin1Char('\n');
        finish(m_options.job == CliOptions::Job::Probe ? ExitCode::Failed : ExitCode::SendFailed);
    }
}

void CliRunner::onProbeFinished(const X50Link::ProbeResult &result)
{
    QTextStream out(stdout);
    utf8(out);
    QTextStream err(stderr);
    utf8(err);

    if (m_link->state() == X50Link::State::Disconnected) {
        // The port died during the probe.
        printChecklist(err, result);
        finish(m_options.job == CliOptions::Job::Probe ? ExitCode::Failed : ExitCode::SendFailed);
        return;
    }

    if (m_options.job == CliOptions::Job::Probe) {
        printChecklist(out, result);
        out.flush();
        // A microX answer is its own exit code, checked before the generic outcome.
        finish(result.microX ? ExitCode::MicroX : result.allOk() ? ExitCode::Ok : ExitCode::Failed);
        return;
    }

    // --send
    if (m_link->state() == X50Link::State::Refused) {
        err << m_link->statusText() << QLatin1Char('\n');
        finish(ExitCode::MicroX);
        return;
    }
    if (!result.x50Confirmed) {
        err << QStringLiteral("Warning: could not confirm an X50; sending on channel %1 with Bank Map %2.\n")
                   .arg(m_link->channel())
                   .arg(m_link->bankMap() == BankMap::Gm ? QStringLiteral("GM") : QStringLiteral("KORG"));
    }
    m_link->send(m_options.address);  // answers with sent() or sendFailed()
}
