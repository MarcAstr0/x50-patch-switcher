#include "midi/RtMidiBackend.h"
#include "ui/MainWindow.h"

#include <QtTest>

class TestScaffold : public QObject
{
    Q_OBJECT
private slots:
    void mainWindowTitle()
    {
        MainWindow w;
        QCOMPARE(w.windowTitle(), QStringLiteral("X50 Patch Switcher"));
    }

    void listPortsDoesNotThrow()
    {
        // Lists may be empty on CI; only enumeration must not throw.
        QVERIFY_THROWS_NO_EXCEPTION(RtMidiBackend::listPorts());
    }
};

QTEST_MAIN(TestScaffold)
#include "test_scaffold.moc"
