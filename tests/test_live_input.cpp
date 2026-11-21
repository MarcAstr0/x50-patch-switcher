// T4.2: live input (SPEC 4.2): click/touch, keys 1-8, next/previous with empty-slot skipping and no
// wrap, re-send on the current slot, set switching, Esc hold / double Esc, no dialogs, and the
// window restore on leaving. Everything runs through a MainWindow, a Library, an X50Link and the
// FakeX50, driven with QTest key and mouse events.
#include <memory>

#include <QApplication>
#include <QKeyEvent>
#include <QPointingDevice>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>
#include <QWindow>

#include "core/Catalogue.h"
#include "core/Library.h"
#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "ui/BrowseView.h"
#include "ui/LiveView.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

namespace {
using A = SoundAddress;
using B = SoundAddress::Bank;
SoundAddress combi(B b, int n) { return A(A::Kind::Combi, b, n); }
SoundAddress prog(B b, int n) { return A(A::Kind::Prog, b, n); }
} // namespace

// Slots: 0 C A002, 1 P D104, 2 empty, 3 C A000, 4 empty, 5 empty, 6 P A028, 7 empty.
struct Rig
{
    QTemporaryDir dir;
    Library lib;
    FakeX50 fake;
    X50Link link;
    std::unique_ptr<MainWindow> w;
    QString setA, setB;
    SoundAddress slotAddr[8];

    explicit Rig(Catalogue *cat, bool fullScreen = false) : link(&fake)
    {
        lib.load(dir.path());
        lib.createSet(QStringLiteral("Set A"), 8, &setA);
        slotAddr[0] = combi(B::A, 2);
        slotAddr[1] = prog(B::D, 104);
        slotAddr[3] = combi(B::A, 0);
        slotAddr[6] = prog(B::A, 28);
        for (int i : {0, 1, 3, 6})
            lib.setSlot(setA, i, slotAddr[i]);
        lib.createSet(QStringLiteral("Set B"), 6, &setB);
        lib.setSlot(setB, 0, combi(B::C, 5));
        lib.setCurrentSet(setA);
        fake.setMode(X50Protocol::Mode::CombiPlay);
        link.setPortNames(QStringLiteral("X50"));
        w = std::make_unique<MainWindow>(cat, &lib, &link);
        w->setLiveFullScreen(fullScreen);
        link.start();
        const auto deadline = QDeadlineTimer(3000);
        while (link.state() != X50Link::State::Connected && !deadline.hasExpired())
            QTest::qWait(10);
        w->show();
        (void)QTest::qWaitForWindowExposed(w.get());
        fake.clearReceived();
    }
    ~Rig() { link.stop(); }
    LiveView *live() const { return w->liveView(); }
    // Keys go to the focused widget, as a real keyboard's would.
    void key(Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) { QTest::keyClick(QApplication::focusWidget() ? QApplication::focusWidget() : static_cast<QWidget *>(live()), k, m); }
    // Waits for the link to report sound `a` as sent.
    bool sentIs(QSignalSpy &spy, int count, const SoundAddress &a)
    {
        const auto deadline = QDeadlineTimer(3000);
        while (spy.count() < count && !deadline.hasExpired())
            QTest::qWait(10);
        return spy.count() == count && spy.last().at(0).value<SoundAddress>() == a;
    }
};

class TestLiveInput : public QObject
{
    Q_OBJECT
    Catalogue m_cat;

private slots:
    void initTestCase()
    {
        qRegisterMetaType<SoundAddress>();
        Theme::loadFonts();
        Theme::applyStyleSheet(*static_cast<QApplication *>(QCoreApplication::instance()));
        QVERIFY(m_cat.loadFromResource());
    }

    // SPEC 4.2 "Click or touch a button: send". Exact bytes: MIDI.md 3 (Bank Map GM, global channel 1,
    // mode already COMBI PLAY so no Mode Change): combination A 002 = B0 00 3F, B0 20 00, C0 02.
    void clickSendsExactBytes()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QSignalSpy sent(&r.link, &X50Link::sent);
        const QPoint c = r.live()->buttonRect(0).center();
        QTest::mouseClick(r.live(), Qt::LeftButton, Qt::NoModifier, c);
        QVERIFY(r.sentIs(sent, 1, combi(B::A, 2)));
        QCOMPARE(r.fake.received(), (QList<QByteArray>{QByteArray::fromHex("B0003F"), QByteArray::fromHex("B02000"),
                                                       QByteArray::fromHex("C002")}));
        QCOMPARE(r.live()->litSlot(), 0);
    }

    // SPEC 4.2 / DESIGN 5.4: send on release inside; an empty slot, or a slide out, sends nothing.
    void clickRules()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QSignalSpy sent(&r.link, &X50Link::sent);
        QTest::mouseClick(r.live(), Qt::LeftButton, Qt::NoModifier, r.live()->buttonRect(2).center());  // empty
        QTest::mousePress(r.live(), Qt::LeftButton, Qt::NoModifier, r.live()->buttonRect(1).center());
        QCOMPARE(r.live()->buttonState(1), LiveView::ButtonState::Pressed);
        QTest::qWait(50);
        QVERIFY2(r.fake.received().isEmpty(), "press alone sent MIDI");
        QTest::mouseMove(r.live(), r.live()->buttonRect(3).center());
        QTest::mouseRelease(r.live(), Qt::LeftButton, Qt::NoModifier, r.live()->buttonRect(3).center());
        QTest::qWait(150);
        QVERIFY2(r.fake.received().isEmpty(), "empty slot or slide-out sent MIDI");
        QCOMPARE(sent.count(), 0);
        QTest::mouseClick(r.live(), Qt::LeftButton, Qt::NoModifier, r.live()->buttonRect(6).center());
        QVERIFY(r.sentIs(sent, 1, prog(B::A, 28)));
    }

    // Touch: a tap is one send (Qt synthesises the mouse events when the widget does not take touch).
    void touchTapSendsOnce()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QSignalSpy sent(&r.link, &X50Link::sent);
        QPointingDevice *dev = QTest::createTouchDevice();
        QWindow *win = r.w->windowHandle();
        const QPoint p = r.live()->mapTo(r.w.get(), r.live()->buttonRect(3).center());
        QTest::touchEvent(win, dev).press(0, p, win);
        QTest::touchEvent(win, dev).release(0, p, win);
        QVERIFY(r.sentIs(sent, 1, combi(B::A, 0)));
        QTest::qWait(300);
        QCOMPARE(sent.count(), 1);
    }

    // SPEC 4.2 "Keys 1..8: send that slot" (empty slot and a slot beyond the set: nothing).
    void digitKeys()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QSignalSpy sent(&r.link, &X50Link::sent);
        r.key(Qt::Key_2);
        QVERIFY(r.sentIs(sent, 1, prog(B::D, 104)));
        r.key(Qt::Key_3);  // empty
        r.key(Qt::Key_5);  // empty
        r.key(Qt::Key_8);  // empty
        r.key(Qt::Key_9);  // not a slot
        r.key(Qt::Key_0);
        QTest::qWait(200);
        QCOMPARE(sent.count(), 1);
        r.key(Qt::Key_7);
        QVERIFY(r.sentIs(sent, 2, prog(B::A, 28)));
        r.key(Qt::Key_1, Qt::KeypadModifier);  // keypad digit
        QVERIFY(r.sentIs(sent, 3, combi(B::A, 2)));
        r.key(Qt::Key_1, Qt::ControlModifier);  // Ctrl+1 is not a live key
        QTest::qWait(150);
        QCOMPARE(sent.count(), 3);
    }

    // SPEC 4.2 "Right, Down, Space, PgDn: next (skips empty slots; stops at the last, no wrap). Left, Up,
    // PgUp: previous", and every key of the group.
    void nextPreviousSkipAndNoWrap()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QSignalSpy sent(&r.link, &X50Link::sent);
        r.key(Qt::Key_1);
        QVERIFY(r.sentIs(sent, 1, combi(B::A, 2)));
        const Qt::Key nexts[] = {Qt::Key_Right, Qt::Key_Down, Qt::Key_Space, Qt::Key_PageDown};
        const Qt::Key prevs[] = {Qt::Key_Left, Qt::Key_Up, Qt::Key_PageUp};
        // forward: slot 0 -> 1 -> 3 (skips 2) -> 6 (skips 4, 5) -> stays (7 empty, no wrap)
        r.key(nexts[0]);
        QVERIFY(r.sentIs(sent, 2, prog(B::D, 104)));
        QTRY_COMPARE(r.live()->litSlot(), 1);
        r.key(nexts[1]);
        QVERIFY(r.sentIs(sent, 3, combi(B::A, 0)));
        QTRY_COMPARE(r.live()->litSlot(), 3);
        r.key(nexts[2]);
        QVERIFY(r.sentIs(sent, 4, prog(B::A, 28)));
        QTRY_COMPARE(r.live()->litSlot(), 6);
        for (Qt::Key k : nexts)
            r.key(k);
        QTest::qWait(300);
        QCOMPARE(sent.count(), 4);  // nothing past the last non-empty slot, no wrap to the first
        // backward: 6 -> 3 -> 1 -> 0 -> stays
        r.key(prevs[0]);
        QVERIFY(r.sentIs(sent, 5, combi(B::A, 0)));
        QTRY_COMPARE(r.live()->litSlot(), 3);
        r.key(prevs[1]);
        QVERIFY(r.sentIs(sent, 6, prog(B::D, 104)));
        QTRY_COMPARE(r.live()->litSlot(), 1);
        r.key(prevs[2]);
        QVERIFY(r.sentIs(sent, 7, combi(B::A, 2)));
        QTRY_COMPARE(r.live()->litSlot(), 0);
        for (Qt::Key k : prevs)
            r.key(k);
        QTest::qWait(300);
        QCOMPARE(sent.count(), 7);
    }

    // The current sound is not in the set (the player changed it on the panel): Next goes to the first
    // non-empty slot, Previous to the last (builder's definition; SPEC is silent). Nothing sent by the panel change.
    void currentNotInSet()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        r.fake.panelSelect(combi(B::B, 7));
        QTRY_VERIFY(r.live()->currentNotInSet());
        QTest::qWait(350);
        QVERIFY(r.fake.received().isEmpty());
        QSignalSpy sent(&r.link, &X50Link::sent);
        r.key(Qt::Key_Right);
        QVERIFY(r.sentIs(sent, 1, combi(B::A, 2)));
        r.fake.panelSelect(combi(B::B, 7));
        QTRY_VERIFY(r.live()->currentNotInSet());
        r.key(Qt::Key_Left);
        QVERIFY(r.sentIs(sent, 2, prog(B::A, 28)));
    }

    // SPEC 4.2 "Pressing the current slot again re-sends it", by key and by button.
    void resendOnCurrent()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QSignalSpy sent(&r.link, &X50Link::sent);
        r.key(Qt::Key_4);
        QVERIFY(r.sentIs(sent, 1, combi(B::A, 0)));
        QTRY_COMPARE(r.live()->litSlot(), 3);
        r.fake.clearReceived();
        r.key(Qt::Key_4);
        QVERIFY(r.sentIs(sent, 2, combi(B::A, 0)));
        QCOMPARE(r.fake.received().last(), QByteArray::fromHex("C000"));
        QTest::mouseClick(r.live(), Qt::LeftButton, Qt::NoModifier, r.live()->buttonRect(3).center());
        QVERIFY(r.sentIs(sent, 3, combi(B::A, 0)));
    }

    // Auto-repeat never sends (a held key or a stuck pedal), for every key group, and not for Esc either.
    void autoRepeatSendsNothing()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        auto repeat = [&](Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) {
            QKeyEvent e(QEvent::KeyPress, k, m, QString(), true);
            QApplication::sendEvent(r.live(), &e);
        };
        for (Qt::Key k : {Qt::Key_1, Qt::Key_2, Qt::Key_Right, Qt::Key_Down, Qt::Key_Space, Qt::Key_PageDown, Qt::Key_Left,
                          Qt::Key_Up, Qt::Key_PageUp, Qt::Key_BracketRight, Qt::Key_BracketLeft})
            for (int n = 0; n < 5; ++n)
                repeat(k);
        repeat(Qt::Key_Right, Qt::ControlModifier);
        repeat(Qt::Key_Escape);
        repeat(Qt::Key_Escape);
        QTest::qWait(250);
        QVERIFY2(r.fake.received().isEmpty(), "auto-repeat sent MIDI");
        QCOMPARE(r.lib.currentSetId(), r.setA);
        QVERIFY(!r.live()->escArmed());
        QVERIFY(r.w->isLive());
    }

    // SPEC 4.2 "Ctrl+Left/Right (or [ / ]): previous / next set"; no wrap; sends nothing; the grid follows.
    void setSwitching()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QCOMPARE(r.live()->slotCount(), 8);
        r.key(Qt::Key_Right, Qt::ControlModifier);
        QCOMPARE(r.lib.currentSetId(), r.setB);
        QCOMPARE(r.live()->set().name(), QStringLiteral("Set B"));
        QCOMPARE(r.live()->slotCount(), 6);
        QCOMPARE(r.live()->setNameShown(), QStringLiteral("Set B"));
        r.key(Qt::Key_Right, Qt::ControlModifier);  // last set: no wrap
        r.key(Qt::Key_BracketRight);
        QCOMPARE(r.lib.currentSetId(), r.setB);
        r.key(Qt::Key_Left, Qt::ControlModifier);
        QCOMPARE(r.lib.currentSetId(), r.setA);
        QCOMPARE(r.live()->slotCount(), 8);
        r.key(Qt::Key_BracketLeft);  // first set: no wrap
        QCOMPARE(r.lib.currentSetId(), r.setA);
        r.key(Qt::Key_BracketRight);
        QCOMPARE(r.lib.currentSetId(), r.setB);
        r.key(Qt::Key_BracketLeft);
        QCOMPARE(r.lib.currentSetId(), r.setA);
        QTest::qWait(300);
        QVERIFY2(r.fake.received().isEmpty(), "switching a set sent MIDI");
        QVERIFY(r.w->isLive());
    }

    // SPEC 4.2: a single Esc does not leave; it arms the hint, and the window passing disarms it.
    void escSinglePressDoesNotLeave()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        r.key(Qt::Key_Escape);
        QVERIFY(r.w->isLive());
        QVERIFY(r.live()->escArmed());
        QVERIFY(r.live()->hintText().startsWith(QLatin1Char('|')));
        QTest::qWait(1200);
        QVERIFY(r.w->isLive());
        QVERIFY(!r.live()->escArmed());
        QVERIFY(r.live()->hintText().startsWith(QStringLiteral("Hold")));
    }

    // SPEC 4.2: Esc held 1 s leaves (auto-repeat of the held key changes nothing).
    void escHeldLeaves()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QTest::keyPress(r.live(), Qt::Key_Escape);
        QVERIFY(r.live()->escArmed());
        for (int i = 0; i < 4; ++i) {
            QTest::qWait(200);
            QKeyEvent e(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier, QString(), true);
            QApplication::sendEvent(r.live(), &e);
            QVERIFY(r.w->isLive());
        }
        QTRY_VERIFY_WITH_TIMEOUT(!r.w->isLive(), 1000);
        QTest::keyRelease(r.live(), Qt::Key_Escape);
        QVERIFY(r.w->browseView()->isVisible());
        QVERIFY(r.w->settingsAllowed());
    }

    // SPEC 4.2: Esc released early and not repeated: stays; the hint disarms with the window.
    void escReleasedEarlyStays()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QTest::keyPress(r.live(), Qt::Key_Escape);
        QTest::qWait(400);
        QTest::keyRelease(r.live(), Qt::Key_Escape);
        QVERIFY(r.live()->escArmed());  // still waiting for a possible second press
        QTest::qWait(1200);
        QVERIFY(r.w->isLive());
        QVERIFY(!r.live()->escArmed());
    }

    // SPEC 4.2: Esc twice within 1 s leaves.
    void escTwiceLeaves()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        r.key(Qt::Key_Escape);
        QTest::qWait(300);
        QVERIFY(r.w->isLive());
        r.key(Qt::Key_Escape);
        QVERIFY(!r.w->isLive());
        QVERIFY(!r.live()->escArmed());
    }

    // Two presses 1.2 s apart do not leave; the second one arms again.
    void escTwiceTooSlowStays()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        r.key(Qt::Key_Escape);
        QTest::qWait(1200);
        r.key(Qt::Key_Escape);
        QVERIFY(r.w->isLive());
        QVERIFY(r.live()->escArmed());
        QTest::qWait(1200);
        QVERIFY(r.w->isLive());
    }

    // SPEC 4.2: no other key leaves, F5 does nothing, Tab keeps the focus, and no dialog can appear.
    void otherKeysAndNoDialogs()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QVERIFY(r.live()->hasFocus());
        for (Qt::Key k : {Qt::Key_F5, Qt::Key_Return, Qt::Key_Enter, Qt::Key_Backspace, Qt::Key_Delete, Qt::Key_Tab, Qt::Key_Backtab,
                          Qt::Key_F1, Qt::Key_F11, Qt::Key_Home, Qt::Key_End, Qt::Key_Q, Qt::Key_L, Qt::Key_Menu})
            r.key(k);
        r.key(Qt::Key_Q, Qt::ControlModifier);
        r.key(Qt::Key_W, Qt::ControlModifier);
        r.key(Qt::Key_Comma, Qt::ControlModifier);
        r.key(Qt::Key_F, Qt::ControlModifier);
        QTest::qWait(200);
        QVERIFY(r.w->isLive());
        QVERIFY(!r.live()->escArmed());
        QVERIFY(r.live()->hasFocus());
        QVERIFY(r.fake.received().isEmpty());
        // Settings cannot open, no modal or other top-level widget exists.
        QVERIFY(!r.w->openSettings());
        QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(!QApplication::activePopupWidget());
        for (QWidget *top : QApplication::topLevelWidgets())
            if (top != r.w.get() && top->isVisible())
                QFAIL(qPrintable(QStringLiteral("a visible top-level widget in live view: %1").arg(QString::fromLatin1(top->metaObject()->className()))));
    }

    // Leaving restores browse exactly as it was (normal window): state and geometry.
    void leaveRestoresNormalWindow()
    {
        Rig r(&m_cat, true);
        r.w->resize(1100, 740);
        r.w->move(80, 60);
        QTest::qWait(50);
        const QRect before(r.w->pos(), r.w->size());
        QVERIFY(r.w->enterLive());
        QTest::qWait(100);
        QVERIFY2(r.w->isFullScreen(), "offscreen platform did not give real full screen");
        r.key(Qt::Key_Escape);
        r.key(Qt::Key_Escape);
        QVERIFY(!r.w->isLive());
        QTRY_VERIFY(!r.w->isFullScreen());
        QVERIFY(!r.w->isMaximized());
        QTRY_COMPARE(r.w->size(), before.size());
        QCOMPARE(r.w->pos(), before.topLeft());
        QVERIFY(r.w->settingsAllowed());
        QVERIFY(r.w->browseView()->isVisible());
    }

    // ... and maximised stays maximised.
    void leaveRestoresMaximisedWindow()
    {
        Rig r(&m_cat, true);
        r.w->showMaximized();
        QTRY_VERIFY(r.w->isMaximized());
        QVERIFY(r.w->enterLive());
        QTRY_VERIFY(r.w->isFullScreen());
        r.live()->leaveRequested();
        QVERIFY(!r.w->isLive());
        QTRY_VERIFY(r.w->isMaximized());
        QVERIFY(!r.w->isFullScreen());
    }

    // T4.1 review fix 1: saveLayout while full screen stores the pre-live normal geometry.
    void saveLayoutWhileLiveStoresNormalGeometry()
    {
        Rig r(&m_cat, true);
        QTemporaryDir cfg;
        r.w->restoreLayout(cfg.path());
        r.w->resize(1100, 740);
        r.w->move(80, 60);
        QTest::qWait(50);
        const QRect before(r.w->pos(), r.w->size());
        QVERIFY(r.w->enterLive());
        QTest::qWait(100);
        QVERIFY(r.w->isFullScreen());
        r.w->saveLayout();
        const QSettings s(QDir(cfg.path()).filePath(MainWindow::layoutFileName()), QSettings::IniFormat);
        QCOMPARE(s.value(QStringLiteral("window/width")).toInt(), before.width());
        QCOMPARE(s.value(QStringLiteral("window/height")).toInt(), before.height());
        QCOMPARE(s.value(QStringLiteral("window/x")).toInt(), before.x());
        QCOMPARE(s.value(QStringLiteral("window/y")).toInt(), before.y());
        QVERIFY(!s.value(QStringLiteral("window/maximized")).toBool());
    }

    // T4.1 review fix 3: a failed library save while live goes to the live top bar.
    void saveFailureGoesToTheLiveBar()
    {
        Rig r(&m_cat);
        QVERIFY(r.w->enterLive());
        QVERIFY(!r.live()->sendFailureShown());
        emit r.lib.saveFailed(QStringLiteral("disk full"));
        QVERIFY(r.live()->sendFailureShown());
        QVERIFY2(r.live()->connectionText().contains(QStringLiteral("DISK FULL"), Qt::CaseInsensitive), qPrintable(r.live()->connectionText()));
    }

    // T4.1 review fix 4: unit names read while live reach the live view (the readout shows the new name).
    void unitNamesReachLiveView()
    {
        Catalogue cat;  // its own: this test edits the names
        QVERIFY(cat.loadFromResource());
        Rig r(&cat);
        QVERIFY(r.w->enterLive());
        QSignalSpy sent(&r.link, &X50Link::sent);
        r.key(Qt::Key_1);
        QVERIFY(r.sentIs(sent, 1, combi(B::A, 2)));
        QTRY_COMPARE(r.live()->litSlot(), 0);
        cat.setUnitName(combi(B::A, 2), QStringLiteral("Zebra Zither"), QString(), QDateTime::currentDateTimeUtc());
        QMap<SoundAddress, Catalogue::UnitName> names;
        names.insert(combi(B::A, 2), {QStringLiteral("Zebra Zither"), QString(), QDateTime::currentDateTimeUtc()});
        r.lib.setUnitNames(names);
        QTRY_VERIFY2(r.live()->readoutBar().contains(QStringLiteral("Zebra")), qPrintable(r.live()->readoutBar()));
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    TestLiveInput t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_live_input.moc"
