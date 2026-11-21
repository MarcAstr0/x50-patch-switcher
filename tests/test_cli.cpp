// T1.0: the CLI must work with no display (no DISPLAY, WAYLAND_DISPLAY or
// QT_QPA_PLATFORM), by running the built executable.
#include <QProcess>
#include <QProcessEnvironment>
#include <QTest>

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
};

QTEST_GUILESS_MAIN(TestCli)
#include "test_cli.moc"
