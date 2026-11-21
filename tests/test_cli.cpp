// T1.0: the CLI must work with no display (no DISPLAY, WAYLAND_DISPLAY or
// QT_QPA_PLATFORM), by running the built executable.
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

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
    static int run(const QStringList &args, QString *out) {
        QProcess p;
        p.setProcessEnvironment(headlessEnv());
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
        QCOMPARE(run({QStringLiteral("--list-ports")}, &out), 0);
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
};

QTEST_GUILESS_MAIN(TestCli)
#include "test_cli.moc"
