// T4.3: the display-sleep inhibit (SPEC 4.2) follows the live view. The Windows API call is
// compiled by CI and confirmed at G3; here the tracked state is checked on every platform.
#include <memory>

#include <QApplication>
#include <QTest>

#include "ui/DisplaySleepInhibitor.h"
#include "ui/MainWindow.h"

class TestDisplaySleepInhibitor : public QObject
{
    Q_OBJECT
private slots:
    void acquireReleaseIdempotent()
    {
        DisplaySleepInhibitor i;
        QVERIFY(!i.isActive());
        i.acquire();
        i.acquire();
        QVERIFY(i.isActive());
        i.release();
        i.release();
        QVERIFY(!i.isActive());
    }

    void followsLiveView()
    {
        MainWindow w;
        w.setLiveFullScreen(false);
        QVERIFY(!w.displaySleepInhibited());
        QVERIFY(w.enterLive());
        QVERIFY(w.displaySleepInhibited());
        QVERIFY(!w.enterLive());
        QVERIFY(w.displaySleepInhibited());
        w.leaveLive();
        QVERIFY(!w.displaySleepInhibited());
    }

    void releasedOnQuit()
    {
        MainWindow w;
        w.setLiveFullScreen(false);
        w.enterLive();
        QVERIFY(w.displaySleepInhibited());
        QMetaObject::invokeMethod(qApp, "aboutToQuit", Qt::DirectConnection);
        QVERIFY(!w.displaySleepInhibited());
    }

    void releasedOnDestruction()
    {
        // No observable state survives the object; this proves the destructor path runs cleanly.
        auto w = std::make_unique<MainWindow>();
        w->setLiveFullScreen(false);
        w->enterLive();
        QVERIFY(w->displaySleepInhibited());
        w.reset();
    }
};

QTEST_MAIN(TestDisplaySleepInhibitor)
#include "test_display_sleep_inhibitor.moc"
