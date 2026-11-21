// T1.5: config directory resolution (SPEC 6 --config-dir, SPEC 7 item 2).
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "core/ConfigLocation.h"

class TestConfigLocation : public QObject {
    Q_OBJECT
private slots:
    void cliWins() {
        QTemporaryDir exe;
        QVERIFY(exe.isValid());
        QFile f(exe.filePath(QStringLiteral("portable")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QTemporaryDir cli;
        QCOMPARE(ConfigLocation::resolve(cli.path(), exe.path(), QStringLiteral("/appcfg")),
                 QDir(cli.path()).absolutePath());
    }
    void portableMarker() {
        QTemporaryDir exe;
        QFile f(exe.filePath(QStringLiteral("portable")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QCOMPARE(ConfigLocation::resolve(QString(), exe.path(), QStringLiteral("/appcfg")),
                 QDir(exe.path()).absolutePath());
    }
    void markerMustBeAFile() {
        QTemporaryDir exe;
        QVERIFY(QDir(exe.path()).mkdir(QStringLiteral("portable")));
        QCOMPARE(ConfigLocation::resolve(QString(), exe.path(), QStringLiteral("/appcfg")),
                 QStringLiteral("/appcfg"));
    }
    void fallsBackToAppConfig() {
        QTemporaryDir exe;
        QCOMPARE(ConfigLocation::resolve(QString(), exe.path(), QStringLiteral("/appcfg")),
                 QStringLiteral("/appcfg"));
        QCOMPARE(ConfigLocation::resolve(QString(), QString(), QStringLiteral("/appcfg")),
                 QStringLiteral("/appcfg"));
    }
    void relativeCliBecomesAbsolute() {
        const QString r = ConfigLocation::resolve(QStringLiteral("some/where"), QString(), QString());
        QVERIFY(QDir::isAbsolutePath(r));
    }
};

QTEST_GUILESS_MAIN(TestConfigLocation)
#include "test_config_location.moc"
