// T1.0: the CLI must work with no display (no DISPLAY, WAYLAND_DISPLAY or
// QT_QPA_PLATFORM), by running the built executable.
#include <QDir>
#include <QFileDevice>
#include <QFileInfo>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

#include "midi/RtMidiBackend.h"
#include "midi/X50Protocol.h"

class TestCli : public QObject {
    Q_OBJECT

    static QProcessEnvironment headlessEnv() {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove(QStringLiteral("DISPLAY"));
        env.remove(QStringLiteral("WAYLAND_DISPLAY"));
        env.remove(QStringLiteral("QT_QPA_PLATFORM"));
        return env;
    }
    static int run(const QStringList &args, QString *out, bool microX = false,
                   const QMap<QString, QString> &extraEnv = QMap<QString, QString>()) {
        QProcess p;
        QProcessEnvironment env = headlessEnv();
        if (microX)
            env.insert(QStringLiteral("X50_FAKE_MICROX"), QStringLiteral("1"));  // hidden hook in CliCommon::makeBackend
        for (auto it = extraEnv.cbegin(); it != extraEnv.cend(); ++it)
            env.insert(it.key(), it.value());  // hidden hooks in CliCommon::makeBackend
        p.setProcessEnvironment(env);
        p.start(QStringLiteral(X50_EXE), args);
        if (!p.waitForFinished(20000)) {
            p.kill();
            return -100;
        }
        if (p.exitStatus() != QProcess::NormalExit)
            return -101;
        *out = QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError());
        return p.exitCode();
    }

private slots:
    void listPorts() {
        QString out;
        const int rc = run({QStringLiteral("--list-ports")}, &out);
        if (rc == 1) {
            // Documented: exit 1 when enumeration itself fails (e.g. no ALSA sequencer on
            // a hosted CI runner). The error must be reported, not swallowed.
            QVERIFY2(out.contains(QStringLiteral("MIDI error")), qPrintable(out));
            return;
        }
        QCOMPARE(rc, 0);
        QVERIFY2(out.contains(QStringLiteral("Outputs:")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("Inputs:")), qPrintable(out));
    }
    void help() {
        QString out;
        QCOMPARE(run({QStringLiteral("--help")}, &out), 0);
        QVERIFY(out.contains(QStringLiteral("--list-ports")));
        QVERIFY2(out.contains(QStringLiteral("Usage: ")) &&
                     out.contains(QStringLiteral("x50-patch-switcher")) &&
                     !out.contains(QStringLiteral("executable_name")),
                 qPrintable(out));
    }
    void helpAll() {
        QString out;
        QCOMPARE(run({QStringLiteral("--help-all")}, &out), 0);
        QVERIFY(out.contains(QStringLiteral("--list-ports")));
    }
    void configDirInHelp() {
        QString out;
        QCOMPARE(run({QStringLiteral("--help")}, &out), 0);
        QVERIFY(out.contains(QStringLiteral("--config-dir")));
    }
    void configDirNeedsValue() {
        QString out;
        QCOMPARE(run({QStringLiteral("--config-dir")}, &out), 2);
    }
    void unknownOption() {
        QString out;
        QCOMPARE(run({QStringLiteral("--bogus")}, &out), 2);
        QVERIFY(!out.isEmpty());
    }

    // T1.8 exit codes. SPEC 6 and --help: 0 ok, 1 probe/diagnose failed, 2 bad usage,
    // 3 no port, 4 send failed or refused, 5 microX.
    void helpListsExitCodes() {
        QString out;
        QCOMPARE(run({QStringLiteral("--help")}, &out), 0);
        for (const char *flag : {"--send", "--probe", "--diagnose", "--fake-midi", "--port", "--in-port", "--channel"})
            QVERIFY2(out.contains(QString::fromLatin1(flag)), flag);
        QVERIFY(out.contains(QStringLiteral("Exit codes:")));
    }
    void sendFake() {
        QString out;
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("PROG A000"), QStringLiteral("--fake-midi")}, &out), 0);
        QVERIFY2(out.contains(QStringLiteral("Sent PROG A000")), qPrintable(out));
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("combi c127"), QStringLiteral("--fake-midi")}, &out), 0);
        QVERIFY2(out.contains(QStringLiteral("Sent COMBI C127")), qPrintable(out));
    }
    void sendBadAddress() {
        QString out;
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("PROG Z9"), QStringLiteral("--fake-midi")}, &out), 2);
        QVERIFY(!out.isEmpty());
        // A bare bank and number is both a program and a combination: ambiguous.
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("A000"), QStringLiteral("--fake-midi")}, &out), 2);
        QVERIFY2(out.contains(QStringLiteral("ambiguous")), qPrintable(out));
    }
    void badChannel() {
        QString out;
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("PROG A000"), QStringLiteral("--channel"),
                      QStringLiteral("17"), QStringLiteral("--fake-midi")}, &out), 2);
        QCOMPARE(run({QStringLiteral("--probe"), QStringLiteral("--channel"), QStringLiteral("x"),
                      QStringLiteral("--fake-midi")}, &out), 2);
    }
    void oneJobOnly() {
        QString out;
        QCOMPARE(run({QStringLiteral("--probe"), QStringLiteral("--send"), QStringLiteral("PROG A000"),
                      QStringLiteral("--fake-midi")}, &out), 2);
    }
    void sendNoPort() {
        QString out;
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("PROG A000"), QStringLiteral("--port"),
                      QStringLiteral("no such port"), QStringLiteral("--fake-midi")}, &out), 3);
        QVERIFY2(out.contains(QStringLiteral("No MIDI port matches")), qPrintable(out));
    }
    void probeFake() {
        QString out;
        QCOMPARE(run({QStringLiteral("--probe"), QStringLiteral("--fake-midi")}, &out), 0);
        QVERIFY2(out.contains(QStringLiteral("X50 found (system 1.05, global channel 1)")), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("Bank Map: GM")), qPrintable(out));
        QVERIFY2(!out.contains(QChar(0x2717)), qPrintable(out));
    }
    void probeNoPort() {
        QString out;
        QCOMPARE(run({QStringLiteral("--probe"), QStringLiteral("--port"), QStringLiteral("no such port"),
                      QStringLiteral("--fake-midi")}, &out), 3);
    }
    // The exit-1 path: a real port that no X50 answers (ALSA "Midi Through"). Skipped
    // where that port does not exist, so it never depends on the machine.
    void probeNoReplyExitsOne() {
        const MidiBackend::Ports ports = RtMidiBackend::listPorts();
        if (!ports.error.isEmpty())
            QSKIP("MIDI enumeration fails on this machine (no ALSA sequencer?)");
        bool haveThrough = false;
        for (const auto &n : ports.outputs)
            haveThrough = haveThrough || n.contains(QStringLiteral("Midi Through"));
        if (!haveThrough)
            QSKIP("no ALSA Midi Through port on this machine");
        QString out;
        const int rc = run({QStringLiteral("--probe"), QStringLiteral("--port"), QStringLiteral("Midi Through")}, &out);
        if (rc == 3 || rc == 4)
            QSKIP("no ALSA Midi Through port on this machine");
        QCOMPARE(rc, 1);
        QVERIFY2(out.contains(QChar(0x2717)) && out.contains(QStringLiteral("No reply")), qPrintable(out));
    }

    // MIDI.md sections 6 and 7: the diagnose sends a Device Inquiry, Mode and State and
    // bank dump requests only; raw dumps are saved as received.
    void diagnoseFake() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString dir = tmp.filePath(QStringLiteral("nested/diag"));  // created by the run
        QString out;
        QCOMPARE(run({QStringLiteral("--diagnose"), dir, QStringLiteral("--fake-midi")}, &out), 0);
        QFile report(dir + QStringLiteral("/report.txt"));
        QVERIFY(report.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(report.readAll());
        QVERIFY2(text.contains(QStringLiteral("[ OK ] X50 found (system 1.05, global channel 1)")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("Bank Map: GM")));
        QVERIFY(text.contains(QStringLiteral("F0 7E 7F 06 01 F7")));           // the Device Inquiry request
        QVERIFY(text.contains(QStringLiteral("F0 7E 00 06 02 42 7A 00 01")));  // its reply, MIDI.md section 6
        QVERIFY(text.contains(QStringLiteral("Program A: 64960 bytes -> prog-A.syx")));
        QVERIFY(text.contains(QStringLiteral("Global:")));
        QVERIFY(text.contains(QStringLiteral("Read result: complete")));
        QVERIFY(text.contains(QStringLiteral("RtMidi: ")) && text.contains(QStringLiteral("Qt: running ")));
        // Nothing that selects or writes: no Mode Change (4E), no CC/PC.
        QVERIFY(!text.contains(QStringLiteral("7A 4E")));
        QVERIFY(!text.contains(QStringLiteral("OUT 3 bytes")) && !text.contains(QStringLiteral("OUT 2 bytes")));
        QVERIFY(!text.contains(QStringLiteral("REFUSED")));

        const QStringList files = {QStringLiteral("prog-A.syx"), QStringLiteral("prog-B.syx"),
                                   QStringLiteral("prog-C.syx"), QStringLiteral("prog-D.syx"),
                                   QStringLiteral("combi-A.syx"), QStringLiteral("combi-B.syx"),
                                   QStringLiteral("combi-C.syx"), QStringLiteral("global.syx")};
        for (const QString &name : files) {
            QFile f(dir + QLatin1Char('/') + name);
            QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(name));
            const QByteArray bytes = f.readAll();
            QVERIFY2(bytes.size() > 2, qPrintable(name));
            QCOMPARE(static_cast<unsigned char>(bytes.front()), 0xF0);
            QCOMPARE(static_cast<unsigned char>(bytes.back()), 0xF7);
            if (name == QStringLiteral("prog-A.syx")) {
                const auto d = X50Protocol::parseProgramDump(bytes);
                QVERIFY2(d.ok, qPrintable(d.error));
                const auto recs = X50Protocol::sliceProgramRecords(d.value.data);
                QVERIFY2(recs.ok, qPrintable(recs.error));
                QCOMPARE(recs.value.size(), 128);
            }
            if (name == QStringLiteral("combi-C.syx")) {
                const auto d = X50Protocol::parseCombinationDump(bytes);
                QVERIFY2(d.ok, qPrintable(d.error));
                QCOMPARE(X50Protocol::sliceCombinationRecords(d.value.data).value.size(), 128);
            }
        }
    }
    void diagnoseNoPort() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString out;
        QCOMPARE(run({QStringLiteral("--diagnose"), tmp.path(), QStringLiteral("--port"),
                      QStringLiteral("no such port"), QStringLiteral("--fake-midi")}, &out), 3);
        QFile report(tmp.filePath(QStringLiteral("report.txt")));
        QVERIFY(report.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(report.readAll());
        QVERIFY2(text.contains(QStringLiteral("Outputs: 0: \"X50 Fake MIDI 1\"")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("NO PORT")));
        QVERIFY(QDir(tmp.path()).entryList({QStringLiteral("*.syx")}).isEmpty());
    }
    // --in-port alone: the omitted side defaults to "X50", which the fake matches.
    void inPortAlone() {
        QString out;
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("PROG A000"), QStringLiteral("--in-port"),
                      QStringLiteral("Fake"), QStringLiteral("--fake-midi")}, &out), 0);
        // ... and the input substring really is honoured: a wrong one finds no port.
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("PROG A000"), QStringLiteral("--in-port"),
                      QStringLiteral("zzz"), QStringLiteral("--fake-midi")}, &out), 3);
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QCOMPARE(run({QStringLiteral("--diagnose"), tmp.path(), QStringLiteral("--in-port"),
                      QStringLiteral("zzz"), QStringLiteral("--fake-midi")}, &out), 3);
        QCOMPARE(run({QStringLiteral("--diagnose"), tmp.path(), QStringLiteral("--in-port"),
                      QStringLiteral("Fake"), QStringLiteral("--fake-midi")}, &out), 0);
        QFile report(tmp.filePath(QStringLiteral("report.txt")));
        QVERIFY(report.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(report.readAll()).contains(QStringLiteral("output \"X50\", input \"Fake\"")));
    }
    // A microX answer exits 5 for --probe, --diagnose and --send (hidden X50_FAKE_MICROX hook).
    void microXExitsFive() {
        QString out;
        QCOMPARE(run({QStringLiteral("--probe"), QStringLiteral("--fake-midi")}, &out, true), 5);
        QCOMPARE(run({QStringLiteral("--send"), QStringLiteral("PROG A000"), QStringLiteral("--fake-midi")}, &out, true), 5);
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QCOMPARE(run({QStringLiteral("--diagnose"), tmp.path(), QStringLiteral("--fake-midi")}, &out, true), 5);
        QVERIFY(QFile::exists(tmp.filePath(QStringLiteral("report.txt"))));
        QVERIFY(QDir(tmp.path()).entryList({QStringLiteral("*.syx")}).isEmpty());
    }
    // R1: the Device Inquiry reply arrives but is refused (MIDI.md section 6 with the family byte
    // 7A changed to 7B). Byte 2 is 0x02 = channel 3 and the fake sits on channel 3, so the
    // read-only requests that follow (Mode and State, every bank, Global) are answered: the
    // attempt says "reply refused", and the banks are saved.
    void diagnoseRefusedInquiryStillReadsBanks() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString out;
        const int rc = run({QStringLiteral("--diagnose"), tmp.path(), QStringLiteral("--fake-midi")}, &out, false,
                           {{QStringLiteral("X50_FAKE_INQUIRY_REPLY"), QStringLiteral("F07E0206 02427B00 01000105 0000F7")},
                            {QStringLiteral("X50_FAKE_CHANNEL"), QStringLiteral("3")}});
        QCOMPARE(rc, 1);  // an X50 was never confirmed, so not a clean run
        QFile report(tmp.filePath(QStringLiteral("report.txt")));
        QVERIFY(report.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(report.readAll());
        QVERIFY2(text.contains(QStringLiteral("reply refused, raw bytes in report")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("Result: INCOMPLETE: the device answered the Device Inquiry in an unexpected form "
                                              "(raw bytes in Probe); banks were read anyway: 8 of 8 saved.")), qPrintable(text));
        QVERIFY2(!text.contains(QStringLiteral("no reply to the Device Inquiry")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("F0 7E 02 06 02 42 7B 00 01 00 01 05 00 00 F7")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("OUT 6 bytes: F0 42 32 7A 12 F7")), qPrintable(text));  // MIDI.md section 5, channel 3
        QVERIFY2(text.contains(QStringLiteral("Mode and State answered")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("Read result: complete")), qPrintable(text));
        for (const char *f : {"prog-A.syx", "prog-D.syx", "combi-A.syx", "combi-C.syx", "global.syx",
                              "device-inquiry-reply-refused-1.syx"})
            QVERIFY2(QFile::exists(tmp.filePath(QString::fromLatin1(f))), f);
    }
    // A refused reply without the 0x0g form in byte 2 (here 7F): the requests go on channel 1.
    void diagnoseRefusedInquiryWithoutNibbleUsesChannelOne() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString out;
        QCOMPARE(run({QStringLiteral("--diagnose"), tmp.path(), QStringLiteral("--fake-midi")}, &out, false,
                     {{QStringLiteral("X50_FAKE_INQUIRY_REPLY"), QStringLiteral("F07E7F06 02427B00 01000105 0000F7")}}), 1);
        QFile report(tmp.filePath(QStringLiteral("report.txt")));
        QVERIFY(report.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(report.readAll());
        QVERIFY2(text.contains(QStringLiteral("reply refused, raw bytes in report")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("OUT 6 bytes: F0 42 30 7A 12 F7")), qPrintable(text));  // channel 1
        QVERIFY2(text.contains(QStringLiteral("Read result: complete")), qPrintable(text));
    }
    // A silent unit: the Device Inquiry is asked twice before "no reply" is declared.
    void diagnoseAsksTheInquiryTwice() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString out;
        QCOMPARE(run({QStringLiteral("--diagnose"), tmp.path(), QStringLiteral("--fake-midi")}, &out, false,
                     {{QStringLiteral("X50_FAKE_DROP_REPLIES"), QStringLiteral("1")}}), 1);
        QFile report(tmp.filePath(QStringLiteral("report.txt")));
        QVERIFY(report.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(report.readAll());
        QVERIFY2(text.contains(QStringLiteral("no reply to the Device Inquiry (asked twice)")), qPrintable(text));
        QCOMPARE(text.count(QStringLiteral("OUT 6 bytes: F0 7E 7F 06 01 F7")), 2);  // MIDI.md section 6 request, twice
    }
    // Exit 4: the diagnose folder cannot be created.
    void diagnoseUnwritableExitsFour() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QFile::setPermissions(tmp.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner);
        if (QFileInfo(tmp.path()).isWritable()) {
            QFile::setPermissions(tmp.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            QSKIP("directory permissions are not enforced (running as root?)");
        }
        QString out;
        const int rc = run({QStringLiteral("--diagnose"), tmp.filePath(QStringLiteral("sub")),
                            QStringLiteral("--fake-midi")}, &out);
        QFile::setPermissions(tmp.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        QCOMPARE(rc, 4);
    }
    // One .syx cannot be written (a directory has its name): report.txt is still written,
    // names the failure, the other files exist, and the exit code is 4.
    void diagnoseSyxFailureKeepsReport() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(QDir(tmp.path()).mkdir(QStringLiteral("prog-A.syx")));
        QString out;
        QCOMPARE(run({QStringLiteral("--diagnose"), tmp.path(), QStringLiteral("--fake-midi")}, &out), 4);
        QFile report(tmp.filePath(QStringLiteral("report.txt")));
        QVERIFY(report.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(report.readAll());
        QVERIFY2(text.contains(QStringLiteral("WRITE ERROR: could not write")) && text.contains(QStringLiteral("prog-A.syx")),
                 qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("Exit code: 4")));
        QVERIFY(QFile::exists(tmp.filePath(QStringLiteral("prog-B.syx"))));
        QVERIFY(QFile::exists(tmp.filePath(QStringLiteral("global.syx"))));
    }
};

QTEST_GUILESS_MAIN(TestCli)
#include "test_cli.moc"
