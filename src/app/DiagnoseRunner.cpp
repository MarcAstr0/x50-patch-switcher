#include "app/DiagnoseRunner.h"

#include "app/CliRunner.h"
#include "core/Version.h"
#include "midi/X50Protocol.h"

#include <RtMidi.h>

#include <string>
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QStringConverter>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>

using CliCommon::hex;

namespace {

QString secs(qint64 ms)
{
    return QStringLiteral("%1 s").arg(ms / 1000.0, 0, 'f', 3);
}

QString modeName(X50Protocol::Mode m)
{
    using M = X50Protocol::Mode;
    switch (m) {
    case M::CombiPlay: return QStringLiteral("COMBI PLAY");
    case M::CombiEdit: return QStringLiteral("COMBI EDIT");
    case M::ProgPlay: return QStringLiteral("PROG PLAY");
    case M::ProgEdit: return QStringLiteral("PROG EDIT");
    case M::Multi: return QStringLiteral("MULTI");
    case M::Global: return QStringLiteral("GLOBAL");
    case M::Demo: return QStringLiteral("DEMO");
    }
    return QStringLiteral("?");
}

QString stateName(X50Link::State s)
{
    switch (s) {
    case X50Link::State::Disconnected: return QStringLiteral("disconnected");
    case X50Link::State::Probing: return QStringLiteral("probing");
    case X50Link::State::Connected: return QStringLiteral("connected (X50 confirmed)");
    case X50Link::State::Manual: return QStringLiteral("manual (X50 not confirmed)");
    case X50Link::State::Refused: return QStringLiteral("refused (microX)");
    }
    return QStringLiteral("?");
}

QString compilerText()
{
#if defined(_MSC_VER)
    return QStringLiteral("MSVC %1").arg(_MSC_VER);
#elif defined(__VERSION__)
    return QString::fromLatin1(__VERSION__);
#else
    return QStringLiteral("unknown compiler");
#endif
}

QString yesNo(bool b) { return b ? QStringLiteral("on") : QStringLiteral("OFF"); }

} // namespace

DiagnoseRunner::DiagnoseRunner(const CliOptions &options, QObject *parent) : QObject(parent), m_options(options) {}

QString DiagnoseRunner::fileNameForLabel(const QString &label)
{
    // "Program A" -> prog-A.syx, "Combination B" -> combi-B.syx, "Global" -> global.syx
    if (label.startsWith(QStringLiteral("Program ")))
        return QStringLiteral("prog-%1.syx").arg(label.mid(8));
    if (label.startsWith(QStringLiteral("Combination ")))
        return QStringLiteral("combi-%1.syx").arg(label.mid(12));
    QString safe;
    for (const QChar c : label)
        safe += c.isLetterOrNumber() ? c.toLower() : QLatin1Char('-');
    return safe + QStringLiteral(".syx");
}

int DiagnoseRunner::run()
{
    QTimer::singleShot(0, this, &DiagnoseRunner::begin);
    QCoreApplication::exec();
    return m_exit;
}

void DiagnoseRunner::begin()
{
    m_total.start();
    if (!QDir().mkpath(m_options.diagnoseDir)) {
        QTextStream(stderr) << QStringLiteral("Could not create the folder %1\n").arg(m_options.diagnoseDir);
        m_exit = ExitCode::SendFailed;
        QCoreApplication::exit(m_exit);
        return;
    }
    m_backendOwner = std::make_unique<LoggingBackend>(CliCommon::makeBackend(m_options.fakeMidi).release());
    m_backend = m_backendOwner.get();

    // Safety: the only messages a diagnose run may send are read-only requests,
    // built by X50Protocol for every channel. Anything else (Mode Change,
    // program select, ...) is refused by the backend wrapper.
    QSet<QByteArray> allowed;
    allowed.insert(X50Protocol::deviceInquiryRequest());
    for (int ch = 1; ch <= 16; ++ch) {
        allowed.insert(X50Protocol::modeAndStateRequest(ch).value);
        allowed.insert(X50Protocol::globalDumpRequest(ch).value);
        for (int b = 0; b < 4; ++b)
            allowed.insert(X50Protocol::programBankDumpRequest(ch, b).value);
        for (int b = 0; b < 3; ++b)
            allowed.insert(X50Protocol::combinationBankDumpRequest(ch, b).value);
    }
    m_backend->setAllowedSends(allowed);
    m_backend->setRestricted(true);

    m_ports = m_backend->ports();
    planAttempts();
    nextAttempt();
}

void DiagnoseRunner::planAttempts()
{
    const QString x50 = CliCommon::defaultPortSubstring();
    auto addAttempt = [this](const QString &out, const QString &in, const QString &why) {
        for (const Attempt &a : qAsConst(m_attempts))
            if (a.outSub == out && a.inSub == in)
                return;
        Attempt a;
        a.outSub = out;
        a.inSub = in;
        a.why = why;
        m_attempts.append(a);
    };

    if (!m_options.port.isEmpty() || !m_options.inPort.isEmpty()) {
        // Only the omitted side falls back: to the other side's substring, else "X50".
        const QString out = m_options.port.isEmpty() ? x50 : m_options.port;
        const QString in = !m_options.inPort.isEmpty() ? m_options.inPort : out;
        m_choiceNote = QStringLiteral("--port and/or --in-port was given, so only that pair is tried.");
        addAttempt(out, in, QStringLiteral("chosen with --port / --in-port"));
        return;
    }
    QStringList outX, inX;
    for (const QString &n : qAsConst(m_ports.outputs))
        if (n.contains(x50, Qt::CaseInsensitive))
            outX << n;
    for (const QString &n : qAsConst(m_ports.inputs))
        if (n.contains(x50, Qt::CaseInsensitive))
            inX << n;
    if (outX.size() == 1 && inX.size() == 1) {
        m_choiceNote = QStringLiteral("Exactly one output and one input have \"X50\" in their name, so that pair is used.");
        addAttempt(outX.first(), inX.first(), QStringLiteral("the only ports named X50"));
        return;
    }
    m_choiceNote = QStringLiteral(
        "No single X50-named pair (%1 outputs and %2 inputs contain \"X50\"). Every output is paired with "
        "the input of the same name, else the input with the same position in the list, and a Device "
        "Inquiry (a read-only question) is sent on each pair, 1 s each, until one answers.")
                       .arg(outX.size()).arg(inX.size());
    for (int i = 0; i < m_ports.outputs.size(); ++i) {
        const QString &out = m_ports.outputs.at(i);
        QString in;
        QString why;
        if (m_ports.inputs.contains(out)) {
            in = out;
            why = QStringLiteral("output %1 and an input have the same name").arg(i);
        } else if (i < m_ports.inputs.size()) {
            in = m_ports.inputs.at(i);
            why = QStringLiteral("output %1 paired with the input at the same position").arg(i);
        } else {
            continue;
        }
        addAttempt(out, in, why);
    }
}

void DiagnoseRunner::nextAttempt()
{
    if (m_link) {
        m_link->stop();
        m_link.release()->deleteLater();
    }
    ++m_current;
    QTimer::singleShot(0, this, [this]() {
        if (m_current < m_attempts.size())
            startAttempt();
        else
            finishRun();
    });
}

void DiagnoseRunner::startAttempt()
{
    Attempt &a = m_attempts[m_current];
    a.noPort = !CliCommon::anyContains(m_ports.outputs, a.outSub) || !CliCommon::anyContains(m_ports.inputs, a.inSub);
    m_backend->addEvent(QStringLiteral("attempt %1: output \"%2\", input \"%3\"").arg(m_current + 1).arg(a.outSub, a.inSub));
    if (a.noPort) {
        a.result = QStringLiteral("no port name contains these substrings");
        nextAttempt();
        return;
    }
    m_link = std::make_unique<X50Link>(m_backend);
    m_link->setPortNames(a.outSub, a.inSub);
    if (m_options.channel > 0)
        m_link->setManualChannel(m_options.channel);
    connect(m_link.get(), &X50Link::probeFinished, this, &DiagnoseRunner::onProbeFinished);
    connect(m_link.get(), &X50Link::readProgress, this, &DiagnoseRunner::onReadProgress);
    connect(m_link.get(), &X50Link::readFinished, this, &DiagnoseRunner::onReadFinished);
    connect(m_link.get(), &X50Link::readFailed, this, &DiagnoseRunner::onReadFailed);
    connect(m_link.get(), &X50Link::errorOccurred, this, [this](const QString &message, const QByteArray &raw) {
        QString line = QStringLiteral("+%1 attempt %2: %3").arg(secs(m_total.elapsed())).arg(m_current + 1).arg(message);
        if (!raw.isEmpty())
            line += QStringLiteral("\n      raw bytes (%1): %2").arg(raw.size()).arg(hex(raw));
        m_errors << line;
    });
    m_attemptClock.start();
    m_link->start();
    if (m_link->state() == X50Link::State::Disconnected) {
        a.result = QStringLiteral("could not open the ports: %1").arg(m_link->statusText());
        nextAttempt();
    }
}

void DiagnoseRunner::onProbeFinished(const X50Link::ProbeResult &result)
{
    Attempt &a = m_attempts[m_current];
    a.probe = result;
    a.probeMs = m_attemptClock.elapsed();
    a.state = m_link->state();
    a.portName = m_link->connectedPortName();
    a.answered = result.inquiry.has_value();
    if (a.state == X50Link::State::Disconnected) {
        a.result = QStringLiteral("the port was lost during the probe");
        nextAttempt();
        return;
    }
    if (!a.answered) {
        a.result = QStringLiteral("no reply to the Device Inquiry");
        nextAttempt();
        return;
    }
    if (result.microX) {
        m_microX = true;
        a.result = QStringLiteral("a microX answered; banks not read");
        finishRun();
        return;
    }
    a.result = result.x50Confirmed
                   ? QStringLiteral("X50 answered")
                   : QStringLiteral("a device answered but is not confirmed as an X50 (member code %1)")
                         .arg(result.inquiry->memberCode, 2, 16, QLatin1Char('0'));
    m_readAttempted = true;
    m_stepClock.start();
    m_link->readNames(true);
}

void DiagnoseRunner::onReadProgress(int, int, const QString &completedBank)
{
    if (completedBank.isEmpty())
        return;
    m_bankTimes.append({completedBank, m_stepClock.elapsed()});
    m_stepClock.restart();
}

void DiagnoseRunner::onReadFinished(const X50Link::ReadResult &result)
{
    m_readErrors = result.errors;
    m_readComplete = result.complete;
    finishRun();
}

void DiagnoseRunner::onReadFailed(const QString &bank, const QString &reason, const QByteArray &raw)
{
    QString line = QStringLiteral("%1: %2").arg(bank, reason);
    if (!raw.isEmpty())
        line += QStringLiteral(" (raw bytes: %1)").arg(hex(raw));
    m_readErrors << line;
    m_readComplete = false;
    finishRun();
}

bool DiagnoseRunner::writeOne(const QString &name, const QByteArray &bytes, bool text, QString *error)
{
    QFile f(QDir(m_options.diagnoseDir).filePath(name));
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Truncate;
    if (text)
        mode |= QIODevice::Text;
    if (!f.open(mode)) {
        *error = QStringLiteral("could not write %1: %2").arg(f.fileName(), f.errorString());
        return false;
    }
    const bool ok = f.write(bytes) == bytes.size() && f.flush();
    f.close();
    if (!ok || f.error() != QFileDevice::NoError) {
        *error = QStringLiteral("could not write %1: %2").arg(f.fileName(), f.errorString());
        return false;
    }
    return true;
}

bool DiagnoseRunner::writeFiles()
{
    bool allOk = true;
    QString error;
    // The report goes first, so something readable exists even if a .syx fails.
    if (!writeOne(QStringLiteral("report.txt"), buildReport().toUtf8(), true, &error)) {
        m_fileErrors << error;
        allOk = false;
    }
    QSet<QString> used;
    for (const X50Link::RawDump &d : qAsConst(m_dumps)) {
        QString name = fileNameForLabel(d.label);
        for (int n = 2; used.contains(name); ++n)
            name = fileNameForLabel(d.label).replace(QStringLiteral(".syx"), QStringLiteral("-%1.syx").arg(n));
        used.insert(name);
        if (writeOne(name, d.bytes, false, &error)) {
            m_writtenFiles << name;
        } else {
            m_fileErrors << error;
            allOk = false;
        }
    }
    if (!allOk)
        m_exit = ExitCode::SendFailed;
    // Rewrite the report so it lists the files and any write errors.
    if (!writeOne(QStringLiteral("report.txt"), buildReport().toUtf8(), true, &error)) {
        if (!m_fileErrors.contains(error))
            m_fileErrors << error;
        m_exit = ExitCode::SendFailed;
        allOk = false;
    }
    return allOk;
}

void DiagnoseRunner::finishRun()
{
    if (m_finished)
        return;
    m_finished = true;
    if (m_link) {
        m_dumps = m_link->rawDumps();
        m_link->stop();
    }

    bool anyPort = false;
    bool x50Read = false;
    for (const Attempt &a : qAsConst(m_attempts)) {
        if (!a.noPort)
            anyPort = true;
        if (a.answered && a.probe.x50Confirmed)
            x50Read = true;
    }
    if (m_microX)
        m_exit = ExitCode::MicroX;
    else if (!m_ports.error.isEmpty())
        m_exit = ExitCode::Failed;  // could not even list the ports
    else if (!anyPort)
        m_exit = ExitCode::NoPort;
    else if (x50Read && m_readAttempted && m_readComplete)
        m_exit = ExitCode::Ok;
    else
        m_exit = ExitCode::Failed;

    const bool written = writeFiles();
    QTextStream out(stdout);
    out.setEncoding(QStringConverter::Utf8);
    if (!written) {
        QTextStream err(stderr);
        for (const QString &e : qAsConst(m_fileErrors))
            err << e << QLatin1Char('\n');
    }
    out << QStringLiteral("Report: %1\n").arg(QDir(m_options.diagnoseDir).filePath(QStringLiteral("report.txt")));
    out << QStringLiteral("%1 bank file(s) saved.\n").arg(m_writtenFiles.size());
    if (m_exit != ExitCode::Ok)
        out << QStringLiteral("The run did not complete cleanly (exit code %1); see the report.\n").arg(m_exit);
    out.flush();
    QCoreApplication::exit(m_exit);
}

QString DiagnoseRunner::buildReport() const
{
    QStringList L;
    const QString rule = QStringLiteral("------------------------------------------------------------");
    auto section = [&](const QString &title) {
        L << QString() << rule << title << rule;
    };

    QString result;
    switch (m_exit) {
    case ExitCode::Ok: result = QStringLiteral("OK: an X50 answered and every bank was read."); break;
    case ExitCode::NoPort: result = QStringLiteral("NO PORT: no MIDI port could be matched."); break;
    case ExitCode::SendFailed: result = QStringLiteral("FILE WRITE FAILED: see Files below."); break;
    default:
        result = m_microX ? QStringLiteral("A microX answered; this app supports the X50 only.")
                          : QStringLiteral("INCOMPLETE: see Probe and Banks below.");
        break;
    }
    L << QStringLiteral("X50 Patch Switcher diagnostic report")
      << QStringLiteral("Result: %1").arg(result)
      << QStringLiteral("Exit code: %1").arg(m_exit)
      << QStringLiteral("Generated: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate))
      << QStringLiteral("Total time: %1").arg(secs(m_total.elapsed()));

    section(QStringLiteral("Program"));
    L << QStringLiteral("App: %1 %2 (%3 build, %4 %5)")
             .arg(Version::appName(), Version::appVersion(),
#ifdef NDEBUG
                  QStringLiteral("release"),
#else
                  QStringLiteral("debug"),
#endif
                  QString::fromLatin1(__DATE__), QString::fromLatin1(__TIME__))
      << QStringLiteral("Compiler: %1").arg(compilerText())
      << QStringLiteral("OS: %1 (%2 %3, %4)")
             .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion(),
                  QSysInfo::currentCpuArchitecture())
      << QStringLiteral("Qt: running %1, built with %2").arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR));
    {
        QStringList apis;
        std::vector<RtMidi::Api> list;
        RtMidi::getCompiledApi(list);
        for (RtMidi::Api api : list)
            apis << QString::fromStdString(RtMidi::getApiName(api));
        L << QStringLiteral("RtMidi: %1, compiled APIs: %2")
                 .arg(QString::fromStdString(RtMidi::getVersion()), apis.join(QStringLiteral(", ")));
    }
    L << QStringLiteral("MIDI backend: %1")
             .arg(m_options.fakeMidi ? QStringLiteral("simulated X50 (--fake-midi)") : QStringLiteral("RtMidi (real ports)"));
    if (m_options.channel > 0)
        L << QStringLiteral("--channel %1 was given (used only if the X50 does not confirm its own channel)").arg(m_options.channel);

    section(QStringLiteral("MIDI ports found"));
    L << QStringLiteral("Outputs: %1").arg(CliCommon::joinNames(m_ports.outputs))
      << QStringLiteral("Inputs: %1").arg(CliCommon::joinNames(m_ports.inputs));
    if (!m_ports.error.isEmpty())
        L << QStringLiteral("Port enumeration error: %1").arg(m_ports.error);

    section(QStringLiteral("Port choice"));
    L << m_choiceNote;
    if (m_attempts.isEmpty())
        L << QStringLiteral("No output/input pair could be formed, so nothing was tried.");
    for (int i = 0; i < m_attempts.size(); ++i) {
        const Attempt &a = m_attempts.at(i);
        if (a.result.isEmpty())
            L << QStringLiteral("Attempt %1: output \"%2\", input \"%3\" (%4): not tried (an earlier pair answered)")
                     .arg(i + 1).arg(a.outSub, a.inSub, a.why);
        else
            L << QStringLiteral("Attempt %1: output \"%2\", input \"%3\" (%4): %5")
                     .arg(i + 1).arg(a.outSub, a.inSub, a.why, a.result);
    }

    for (int i = 0; i < m_attempts.size(); ++i) {
        const Attempt &a = m_attempts.at(i);
        if (a.probeMs < 0)
            continue;
        section(QStringLiteral("Probe, attempt %1").arg(i + 1));
        L << QStringLiteral("Port opened: \"%1\"").arg(a.portName)
          << QStringLiteral("State after the probe: %1").arg(stateName(a.state))
          << QStringLiteral("Probe time (Device Inquiry and Mode and State): %1").arg(secs(a.probeMs))
          << QStringLiteral("Checklist:");
        for (const X50Link::CheckItem &item : a.probe.items) {
            const QString tag = item.status == X50Link::CheckItem::Status::Ok     ? QStringLiteral("[ OK ]")
                                : item.status == X50Link::CheckItem::Status::Failed ? QStringLiteral("[FAIL]")
                                                                                    : QStringLiteral("[WARN]");
            L << QStringLiteral("  %1 %2").arg(tag, item.text);
            if (!item.fix.isEmpty())
                L << QStringLiteral("         fix: %1").arg(item.fix);
            if (!item.raw.isEmpty())
                L << QStringLiteral("         raw bytes (%1): %2").arg(item.raw.size()).arg(hex(item.raw));
        }
        if (a.probe.inquiry) {
            const auto &q = *a.probe.inquiry;
            L << QStringLiteral("Device Inquiry reply: member code %1, system %2 (major %3, minor %4, release %5, build %6), global channel %7")
                     .arg(q.memberCode, 2, 16, QLatin1Char('0')).arg(a.probe.versionText)
                     .arg(q.major).arg(q.minor).arg(q.release).arg(q.build).arg(q.channel);
        }
        if (a.probe.state) {
            const auto &s = *a.probe.state;
            static const char *const bankLetters = "ABCDE";
            L << QStringLiteral("Mode and State reply: mode %1, Bank Map %2").arg(modeName(s.mode),
                   s.bankMap == BankMap::Gm ? QStringLiteral("GM") : QStringLiteral("KORG"))
              << QStringLiteral("  enables: Program Change %1, Bank Change %2, Combination Change %3, Aftertouch %4, Control Change %5")
                     .arg(yesNo(s.enableProgramChange), yesNo(s.enableBankChange), yesNo(s.enableCombinationChange),
                          yesNo(s.enableAftertouch), yesNo(s.enableControlChange))
              << QStringLiteral("  current program: bank %1 number %2; current combination: bank %3 number %4")
                     .arg(QLatin1Char(bankLetters[qBound(0, s.programBank, 4)])).arg(s.programNumber)
                     .arg(QLatin1Char(bankLetters[qBound(0, s.combinationBank, 4)])).arg(s.combinationNumber);
        }
    }

    section(QStringLiteral("Banks read (read-only requests)"));
    if (!m_readAttempted) {
        L << QStringLiteral("No banks were read: no X50 (or unknown device) answered the probe.");
    } else {
        for (const BankTiming &bt : m_bankTimes) {
            const X50Link::RawDump *dump = nullptr;
            for (const X50Link::RawDump &d : m_dumps)
                if (d.label == bt.label)
                    dump = &d;
            QString status = QStringLiteral("ok");
            for (const QString &e : m_readErrors)
                if (e.startsWith(bt.label + QStringLiteral(": ")))
                    status = e.mid(bt.label.size() + 2);
            L << QStringLiteral("%1: %2, %3, %4")
                     .arg(bt.label, dump ? QStringLiteral("%1 bytes -> %2").arg(dump->bytes.size()).arg(fileNameForLabel(dump->label))
                                         : QStringLiteral("no data received"),
                          secs(bt.ms), status);
        }
        const bool anyFailure = !m_readErrors.isEmpty();
        L << QStringLiteral("Read result: %1").arg(m_readComplete && !anyFailure ? QStringLiteral("complete")
                                                                                  : QStringLiteral("INCOMPLETE"));
    }

    section(QStringLiteral("Errors"));
    if (m_errors.isEmpty() && m_readErrors.isEmpty())
        L << QStringLiteral("(none)");
    L << m_errors;
    for (const QString &e : m_readErrors)
        L << QStringLiteral("bank read: %1").arg(e);

    section(QStringLiteral("Everything sent and received"));
    L << QStringLiteral("OUT = sent to the X50, IN = received. Long dumps are cut here; the whole bytes are in the .syx files.");
    for (const LoggingBackend::Entry &e : m_backend->log()) {
        const QString t = QStringLiteral("+%1").arg(secs(e.ms), 9);
        switch (e.dir) {
        case LoggingBackend::Entry::Dir::Out:
            L << QStringLiteral("%1  OUT %2 bytes: %3").arg(t).arg(e.bytes.size()).arg(hex(e.bytes));
            break;
        case LoggingBackend::Entry::Dir::In:
            if (e.bytes.size() > 48)
                L << QStringLiteral("%1  IN  %2 bytes: %3 ...").arg(t).arg(e.bytes.size()).arg(hex(e.bytes.left(24)));
            else
                L << QStringLiteral("%1  IN  %2 bytes: %3").arg(t).arg(e.bytes.size()).arg(hex(e.bytes));
            break;
        case LoggingBackend::Entry::Dir::Event:
            L << QStringLiteral("%1  %2%3").arg(t, e.text,
                    e.bytes.isEmpty() ? QString() : QStringLiteral(" [%1]").arg(hex(e.bytes)));
            break;
        }
    }

    section(QStringLiteral("Files in this folder"));
    for (const QString &f : m_writtenFiles)
        L << f;
    L << QStringLiteral("report.txt");
    for (const QString &e : m_fileErrors)
        L << QStringLiteral("WRITE ERROR: %1").arg(e);
    L << QString();
    return L.join(QLatin1Char('\n'));
}
