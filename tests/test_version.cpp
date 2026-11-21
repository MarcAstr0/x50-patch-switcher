#include <QTest>

#include "core/Version.h"

class TestVersion : public QObject {
    Q_OBJECT
private slots:
    // appVersion() must equal the CMake project VERSION (X50_VERSION define).
    void matchesCMakeVersion() {
        QCOMPARE(Version::appVersion(), QStringLiteral(X50_VERSION));
        QCOMPARE(Version::appVersion(), QStringLiteral("0.9.0"));
    }
};

QTEST_APPLESS_MAIN(TestVersion)
#include "test_version.moc"
