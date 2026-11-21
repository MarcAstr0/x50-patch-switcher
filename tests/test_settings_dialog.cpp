// T3.4: SettingsDialog and ProbeChecklist against FakeX50 and X50Link (headless).
// Bytes are typed from docs/MIDI.md: section 3 (bank select values per Bank Map, and the gating
// settings on GLOBAL 1: MIDI), section 5 (Mode and State request), section 6 (Device Inquiry).
// SPEC 4.5 (checklist, manual mode), 7.4 (port busy wording), 8 (microX text).
// Registered twice in CMake (1x and QT_SCALE_FACTOR=2) for the screenshot smoke.
#include <memory>

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>

#include <QTimer>

#include "core/Library.h"
#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "ui/ConnectionSettings.h"
#include "ui/MainWindow.h"
#include "ui/ProbeChecklist.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

namespace {
using S = ProbeChecklist::State;

QByteArray H(std::initializer_list<int> list)
{
    QByteArray b;
    for (int v : list)
        b.append(static_cast<char>(v));
    return b;
}

// MIDI.md section 6 and 5.
const QByteArray kInquiry = H({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7});
const QByteArray kModeState = H({0xF0, 0x42, 0x30, 0x7A, 0x12, 0xF7});

X50Link::Timing fastTiming()
{
    X50Link::Timing t;
    t.pollMs = 30;
    t.requestMs = 150;
    t.modeChangeWaitMs = 20;
    t.bankMs = 3000;
    t.echoMs = 50;
    return t;
}

SoundAddress combi(int n) { return SoundAddress(SoundAddress::Kind::Combi, SoundAddress::Bank::A, n); }

bool isChannelVoice(const QByteArray &m)
{
    const unsigned char s = m.isEmpty() ? 0 : static_cast<unsigned char>(m.at(0));
    return s >= 0x80 && s < 0xF0;
}
} // namespace

class TestSettingsDialog : public QObject {
    Q_OBJECT
    FakeX50 *fake = nullptr;
    X50Link *link = nullptr;
    QTemporaryDir *dir = nullptr;

    void connectLink(X50Link::State want = X50Link::State::Connected)
    {
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), want, 3000);
    }
    static const ProbeChecklist::Row *find(const ProbeChecklist *l, const QString &text)
    {
        for (const ProbeChecklist::Row &r : l->rows())
            if (r.text.contains(text))
                return &r;
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        Theme::applyStyleSheet(*static_cast<QApplication *>(QCoreApplication::instance()));
        Theme::loadFonts();
        qRegisterMetaType<SoundAddress>("SoundAddress");
        qRegisterMetaType<X50Link::State>("X50Link::State");
        qRegisterMetaType<X50Link::ProbeResult>("X50Link::ProbeResult");
    }
    void init()
    {
        fake = new FakeX50(this);
        link = new X50Link(fake, this);
        link->setTiming(fastTiming());
        link->setPortNames(QStringLiteral("fake"));
        dir = new QTemporaryDir;
    }
    void cleanup()
    {
        delete link;
        delete fake;
        delete dir;
    }

    // Opening the dialog sends nothing, even on a connected link (SPEC 4.5, CLAUDE.md).
    void openingSendsNothing()
    {
        connectLink();
        fake->clearReceived();
        SettingsDialog d(link, fake, dir->path());
        d.show();
        QTest::qWait(50);
        QVERIFY(fake->received().isEmpty());
        // Changing the manual controls (disabled here) or the combos must not send either.
        d.refreshPorts();
        QTest::qWait(20);
        QVERIFY(fake->received().isEmpty());
    }

    // All lines pass on a healthy X50 (SPEC 4.5 order), and the manual controls stay disabled.
    void healthyX50AllPass()
    {
        connectLink();
        SettingsDialog d(link, fake, dir->path());
        const auto &rows = d.checklist()->rows();
        QCOMPARE(rows.size(), 6);
        for (const auto &r : rows) {
            QCOMPARE(r.state, S::Pass);
            QVERIFY(r.hint.isEmpty());
        }
        QCOMPARE(rows.at(0).text, QStringLiteral("X50 found (system 1.05, global channel 1)"));
        QVERIFY(!d.manualEnabled());
        QCOMPARE(d.outputCombo()->currentText(), fake->ports().outputs.first());
    }

    // MIDI.md section 3, "Gating settings": Program Change, Bank, Combi each get a Failed line
    // with the GLOBAL 1: MIDI hint naming the checkbox.
    void eachEnableFlagOffShowsFailedLineWithFixHint_data()
    {
        QTest::addColumn<bool>("pc");
        QTest::addColumn<bool>("bank");
        QTest::addColumn<bool>("combi");
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("hint");
        QTest::newRow("program change") << false << true << true << QStringLiteral("Program Change disabled")
                                        << QStringLiteral("GLOBAL 1: MIDI, tick \"Program Change\"");
        QTest::newRow("bank") << true << false << true << QStringLiteral("Bank Change disabled")
                              << QStringLiteral("GLOBAL 1: MIDI, tick \"Bank\"");
        QTest::newRow("combi") << true << true << false << QStringLiteral("Combination Change disabled")
                               << QStringLiteral("GLOBAL 1: MIDI, tick \"Combi\"");
    }
    void eachEnableFlagOffShowsFailedLineWithFixHint()
    {
        QFETCH(bool, pc);
        QFETCH(bool, bank);
        QFETCH(bool, combi);
        QFETCH(QString, line);
        QFETCH(QString, hint);
        fake->setEnables(pc, bank, combi);
        connectLink();
        SettingsDialog d(link, fake, dir->path());
        const auto *r = find(d.checklist(), line);
        QVERIFY2(r, qPrintable(line));
        QCOMPARE(r->state, S::Fail);
        QCOMPARE(r->hint, hint);
        // The other lines still pass.
        int fails = 0;
        for (const auto &row : d.checklist()->rows())
            fails += row.state == S::Fail;
        QCOMPARE(fails, 1);
    }

    // SPEC 8: a microX gets the exact text, the app refuses, and nothing is sent to it.
    void microXShowsTextAndRefuses()
    {
        fake->setMicroX(true);
        connectLink(X50Link::State::Refused);
        SettingsDialog d(link, fake, dir->path());
        const auto *r = find(d.checklist(), QStringLiteral("This is a microX"));
        QVERIFY(r);
        QCOMPARE(r->text, QStringLiteral("This is a microX; X50 Patch Switcher supports the X50 only"));
        QCOMPARE(r->state, S::Fail);
        QVERIFY(!d.manualEnabled());
        fake->clearReceived();
        QSignalSpy failed(link, &X50Link::sendFailed);
        link->send(combi(0));
        QCOMPARE(failed.count(), 1);
        for (const QByteArray &m : fake->received())
            QVERIFY(!isChannelVoice(m));
    }

    // No Device Inquiry reply: one Failed line with the "No reply" text (SPEC 4.5, DESIGN 8.1),
    // the Exclusive hint (MIDI.md section 3), manual controls enabled; applying them changes
    // the next send's bytes: CC0 is 00 for KORG and 3F for GM (MIDI.md section 3, combination
    // bank A, program 0 and 1), on channel 3 (status B2 / C2).
    void noReplyEnablesManualAndAppliesToNextSend()
    {
        fake->setDropReplies(true);
        connectLink(X50Link::State::Manual);
        SettingsDialog d(link, fake, dir->path());
        QCOMPARE(d.checklist()->rows().size(), 1);
        const auto &row = d.checklist()->rows().first();
        QCOMPARE(row.state, S::Fail);
        QCOMPARE(row.text, QStringLiteral("No reply from the X50"));
        // MIDI.md section 3: "no SysEx reply: check Exclusive", MIDI Filter Exclusive checked.
        QVERIFY(row.hint.contains(QStringLiteral("check Exclusive")));
        QVERIFY(row.hint.contains(QStringLiteral("MIDI Filter \"Exclusive\" checked")));
        QVERIFY(d.manualEnabled());

        d.channelSpin()->setValue(3);
        d.bankMapCombo()->setCurrentIndex(1);  // KORG
        fake->clearReceived();
        QSignalSpy sent(link, &X50Link::sent);
        link->send(combi(0));
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 1, 3000);
        QList<QByteArray> cv;
        for (const QByteArray &m : fake->received())
            if (isChannelVoice(m))
                cv << m;
        QCOMPARE(cv, (QList<QByteArray>{H({0xB2, 0x00, 0x00}), H({0xB2, 0x20, 0x00}), H({0xC2, 0x00})}));

        d.bankMapCombo()->setCurrentIndex(0);  // GM
        fake->clearReceived();
        link->send(combi(1));
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 2, 3000);
        cv.clear();
        for (const QByteArray &m : fake->received())
            if (isChannelVoice(m))
                cv << m;
        QCOMPARE(cv, (QList<QByteArray>{H({0xB2, 0x00, 0x3F}), H({0xB2, 0x20, 0x00}), H({0xC2, 0x01})}));
    }

    // Device Inquiry answered but Mode and State not: Bank Map is "not reported" (unknown line,
    // with the GM or KORG sending note) and the manual controls are enabled.
    void modeAndStateMissingShowsBankMapUnknown()
    {
        fake->setDrop(FakeX50::Reply::ModeAndState, true);
        connectLink(X50Link::State::Manual);
        SettingsDialog d(link, fake, dir->path());
        const auto *bm = find(d.checklist(), QStringLiteral("Bank Map: not reported"));
        QVERIFY(bm);
        QCOMPARE(bm->state, S::Unknown);
        QVERIFY(bm->hint.contains(QStringLiteral("Sending as GM")));
        QVERIFY(find(d.checklist(), QStringLiteral("No reply to Mode and State")));
        QVERIFY(d.manualEnabled());
    }

    // SPEC 7.4: the busy wording comes through when Test cannot open the port.
    void portBusyText()
    {
        fake->setPortBusy(true);
        link->start();
        SettingsDialog d(link, fake, dir->path());
        d.testButton()->click();
        QVERIFY2(d.note().contains(QStringLiteral("port busy (another program is using the X50)")), qPrintable(d.note()));
        QCOMPARE(link->state(), X50Link::State::Disconnected);
    }

    // Test applies the chosen ports and probes; it sends only the two read-only probe requests
    // (MIDI.md sections 5 and 6), never a sound change. Check X50 re-probes the open port.
    void testAndCheckSendOnlyProbeRequests()
    {
        connectLink();
        SettingsDialog d(link, fake, dir->path());
        fake->clearReceived();
        d.testButton()->click();
        QTRY_VERIFY_WITH_TIMEOUT(link->state() == X50Link::State::Connected && fake->received().size() >= 2, 3000);
        QCOMPARE(fake->received(), (QList<QByteArray>{kInquiry, kModeState}));
        QVERIFY(d.statusWord()->text().startsWith(QStringLiteral("Last check: ")));

        fake->clearReceived();
        d.checkButton()->click();
        QTRY_VERIFY_WITH_TIMEOUT(fake->received().size() >= 2, 3000);
        QCOMPARE(fake->received(), (QList<QByteArray>{kInquiry, kModeState}));
    }

    // While the probe runs every line is "running" (static glyph) with the check's name; the
    // Check button is disabled until it finishes.
    void runningStateWhileProbing()
    {
        fake->setReplyDelayMs(80);
        connectLink(X50Link::State::Probing);
        SettingsDialog d(link, fake, dir->path());
        QCOMPARE(d.checklist()->rows().size(), 6);
        for (const auto &r : d.checklist()->rows())
            QCOMPARE(r.state, S::Running);
        QVERIFY(!d.checkButton()->isEnabled());
        QCOMPARE(d.statusWord()->text(), QStringLiteral("Checking..."));
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
        QVERIFY(d.checkButton()->isEnabled());
        QCOMPARE(d.checklist()->rows().size(), 6);
    }

    // Port choices and manual values survive closing and reopening, in <configDir>/settings.ini.
    void choicesPersistAcrossReopen()
    {
        fake->setDropReplies(true);
        connectLink(X50Link::State::Manual);
        QString portName;
        {
            SettingsDialog d(link, fake, dir->path());
            QVERIFY(d.manualEnabled());
            d.channelSpin()->setValue(7);
            d.bankMapCombo()->setCurrentIndex(1);
            portName = d.outputCombo()->currentText();
            d.testButton()->click();  // applies and saves the port choice
        }
        QVERIFY(QFile::exists(dir->filePath(ConnectionSettings::fileName())));
        const ConnectionSettings s = ConnectionSettings::load(dir->path());
        QCOMPARE(s.outputPort, portName);
        QCOMPARE(s.inputPort, portName);
        QCOMPARE(s.channel, 7);
        QCOMPARE(s.bankMap, BankMap::Korg);

        SettingsDialog again(link, fake, dir->path());
        QCOMPARE(again.channelSpin()->value(), 7);
        QCOMPARE(again.bankMapCombo()->currentText(), QStringLiteral("KORG"));
        QCOMPARE(again.outputCombo()->currentText(), portName);

        // applyTo pushes them into a fresh link (what main() does at start).
        FakeX50 other;
        X50Link fresh(&other);
        s.applyTo(fresh);
        QCOMPARE(fresh.channel(), 7);
        QCOMPARE(fresh.bankMap(), BankMap::Korg);
    }

    void loadClampsBadValues()
    {
        QFile f(dir->filePath(ConnectionSettings::fileName()));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[manual]\nchannel=99\nbankMap=weird\n");
        f.close();
        const ConnectionSettings s = ConnectionSettings::load(dir->path());
        QCOMPARE(s.channel, 1);
        QCOMPARE(s.bankMap, BankMap::Gm);
    }

    // Checklist hints: text between quotes is drawn strong; rows repaint only on a change.
    void checklistRowsAndHeight()
    {
        ProbeChecklist l;
        l.resize(456, 100);
        QVERIFY(!l.rows().size());
        QCOMPARE(l.heightForWidth(456), 96);  // the placeholder box
        l.setRows({{S::Pass, QStringLiteral("a"), QString()},
                   {S::Fail, QStringLiteral("b"), QStringLiteral("GLOBAL 1: MIDI, tick \"Bank\"")}});
        QVERIFY(l.heightForWidth(456) > 2 * 32 + 4);
        QVERIFY(l.rowGeometry(1).height > 32);
        QCOMPARE(l.rowGeometry(0).height, 32);
    }

    // Test and Check are off while the link waits on a Mode Change, with the reason in the note,
    // and come back when it ends.
    void testAndCheckDisabledWhileBusy()
    {
        connectLink();
        SettingsDialog d(link, fake, dir->path());
        QVERIFY(d.testButton()->isEnabled() && d.checkButton()->isEnabled());
        const auto kind = link->believedMode() == X50Protocol::Mode::CombiPlay ? SoundAddress::Kind::Prog
                                                                                : SoundAddress::Kind::Combi;
        link->send(SoundAddress(kind, SoundAddress::Bank::A, 0));
        QVERIFY(link->isBusy());
        QVERIFY(!d.testButton()->isEnabled());
        QVERIFY(!d.checkButton()->isEnabled());
        QVERIFY2(d.note().contains(QStringLiteral("busy")), qPrintable(d.note()));
        d.checkButton()->click();  // disabled: nothing happens
        QVERIFY(link->isBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!link->isBusy(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(d.checkButton()->isEnabled() && d.testButton()->isEnabled(), 3000);
        QVERIFY(d.note().isEmpty());
    }

    // MIDI.md section 6: a device with another member code answers: a Failed line with the raw
    // reply, and the manual controls enable.
    void nonX50DeviceShowsLineAndEnablesManual()
    {
        fake->setMemberCode(0x02);
        connectLink(X50Link::State::Manual);
        SettingsDialog d(link, fake, dir->path());
        const auto *r = find(d.checklist(), QStringLiteral("A device answered but it is not an X50 (member code 02)"));
        QVERIFY(r);
        QCOMPARE(r->state, S::Fail);
        QVERIFY(r->hint.contains(QStringLiteral("Raw bytes: F0 7E")));
        QVERIFY(d.manualEnabled());
        QVERIFY(d.channelSpin()->isEnabled());
    }

    // A channel learned from the inquiry reply is shown, and a manual change does not replace it.
    void learnedChannelShownAndKept()
    {
        fake->setGlobalChannel(5);
        fake->setDrop(FakeX50::Reply::ModeAndState, true);
        connectLink(X50Link::State::Manual);
        SettingsDialog d(link, fake, dir->path());
        QCOMPARE(d.channelSpin()->value(), 5);
        QVERIFY(!d.channelSpin()->isEnabled());
        QVERIFY(d.bankMapCombo()->isEnabled());
        d.channelSpin()->setValue(9);  // programmatic, as a test would
        QCOMPARE(link->channel(), 5);
        d.bankMapCombo()->setCurrentIndex(1);
        QCOMPARE(link->channel(), 5);
        d.closeButton()->click();
        QCOMPARE(ConnectionSettings::load(dir->path()).channel, 1);  // the learned one is never stored as manual
    }

    // Settings are saved on Test, Check and close, not on every change; a failed save is told.
    void savesOnCloseAndTellsFailure()
    {
        fake->setDropReplies(true);
        connectLink(X50Link::State::Manual);
        {
            SettingsDialog d(link, fake, dir->path());
            d.channelSpin()->setValue(4);
            d.channelSpin()->setValue(6);
            QVERIFY(!QFile::exists(dir->filePath(ConnectionSettings::fileName())));
            QCOMPARE(link->channel(), 6);  // applied to the link at once
            d.closeButton()->click();
            QVERIFY(!d.saveFailed());
        }
        QCOMPARE(ConnectionSettings::load(dir->path()).channel, 6);

        QFile blocker(dir->filePath(QStringLiteral("blocker")));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        SettingsDialog bad(link, fake, dir->filePath(QStringLiteral("blocker/sub")));
        bad.checkButton()->click();
        QVERIFY(bad.saveFailed());
        QCOMPARE(bad.note(), QStringLiteral("Settings could not be saved (folder is read-only?)"));
    }

    // "Last check" is the library's unit.lastSeen when a probe confirmed the X50 (MainWindow
    // records it on each completed probe).
    void lastCheckComesFromLibraryUnit()
    {
        Library lib;
        MainWindow w(nullptr, &lib, link);
        connectLink();
        QVERIFY(lib.unit().has_value());
        QCOMPARE(lib.unit()->memberCode, 1);
        QCOMPARE(lib.unit()->version, QStringLiteral("1.05"));
        const QDateTime seen = QDateTime(QDate(2026, 1, 2), QTime(3, 4), Qt::UTC);
        Library::UnitInfo u = *lib.unit();
        u.lastSeen = seen;
        lib.setUnit(u);
        SettingsDialog d(link, fake, dir->path());
        d.setLibrary(&lib);
        QCOMPARE(d.statusWord()->text(), QStringLiteral("Last check: %1").arg(seen.toLocalTime().toString(QStringLiteral("HH:mm"))));
    }

    // The guard keeps Settings from opening while the live view is active (M4 sets it).
    void settingsBlockedWhileGuarded()
    {
        MainWindow w(nullptr, nullptr, link);
        w.setSettingsAllowed(false);
        QVERIFY(!w.openSettings());
        w.setSettingsAllowed(true);
        QTimer::singleShot(100, [] {
            if (QWidget *m = QApplication::activeModalWidget())
                m->close();
        });
        QVERIFY(w.openSettings());
    }

    // The shared "Bank Map: not reported" line.
    void bankMapNotReportedRowText()
    {
        const auto r = SettingsDialog::bankMapNotReportedRow(BankMap::Korg);
        QCOMPARE(r.text, QStringLiteral("Bank Map: not reported"));
        QVERIFY(r.hint.contains(QStringLiteral("Sending as KORG")));
    }

    // DESIGN 8 (D3): Close sits below the card, its right edge on the card's right edge; the
    // controls are 40 high (QSS content box plus border) and the focus border does not grow them.
    void closeBelowCardAndControlHeights()
    {
        connectLink();
        SettingsDialog d(link, fake, dir->path());
        d.show();
        QApplication::processEvents();
        QFrame *card = d.findChild<QFrame *>(QStringLiteral("settingsCard"));
        QVERIFY(card);
        const QRect cardRect(card->mapTo(&d, QPoint(0, 0)), card->size());
        const QRect close(d.closeButton()->mapTo(&d, QPoint(0, 0)), d.closeButton()->size());
        QVERIFY(close.top() > cardRect.bottom());
        QCOMPARE(close.top() - (cardRect.bottom() + 1), 16);
        QCOMPARE(close.right(), cardRect.right());
        QCOMPARE(d.closeButton()->height(), 40);
        QCOMPARE(d.checkButton()->height(), 40);
        QCOMPARE(d.testButton()->height(), 40);
        QCOMPARE(d.outputCombo()->height(), 40);
    }

    // Screenshot smoke: the dialog renders at the DESIGN width; the four glyph shapes are
    // painted in the DESIGN colours (sampled off-centre, where no mark is drawn on top).
    void screenshotSmoke()
    {
        connectLink();
        SettingsDialog d(link, fake, dir->path());
        d.checklist()->setRows({{S::Pass, QStringLiteral("p"), QString()},
                                {S::Fail, QStringLiteral("f"), QStringLiteral("hint")},
                                {S::Unknown, QStringLiteral("u"), QString()},
                                {S::Running, QStringLiteral("r"), QString()}});
        d.fitToContent();
        d.show();
        QApplication::processEvents();
        QCOMPARE(d.width(), 520);
        QVERIFY(d.height() >= 560);
        const QPixmap shot = d.grab();
        QVERIFY(!shot.isNull());
        const qreal dpr = shot.devicePixelRatio();
        QCOMPARE(shot.width(), qRound(520 * dpr));
        const QImage list = d.checklist()->grab().toImage();
        const qreal ld = list.devicePixelRatio();
        auto at = [&](int x, int y) { return QColor(list.pixel(qRound(x * ld), qRound(y * ld))); };
        auto near = [](const QColor &a, QRgb b) {
            return qAbs(a.red() - qRed(b)) < 6 && qAbs(a.green() - qGreen(b)) < 6 && qAbs(a.blue() - qBlue(b)) < 6;
        };
        QVERIFY(near(at(4, d.checklist()->rowGeometry(0).top + 16), Theme::green));  // disc
        QVERIFY(near(at(4, d.checklist()->rowGeometry(1).top + 16), Theme::red));    // square (corner area)
        // The diamond is outlined, not filled: just inside its top vertex is background.
        QVERIFY(!near(at(12, d.checklist()->rowGeometry(2).top + 16 - 5), Theme::amber));
    }

    // R3 D: with no port, never probed, the manual controls are enabled.
    void manualEnabledBeforeAnyProbe()
    {
        SettingsDialog d(link, fake, dir->path());
        QVERIFY(d.manualEnabled());
        QVERIFY(d.channelSpin()->isEnabled());
        fake->setPortBusy(true);
        link->start();
        SettingsDialog d2(link, fake, dir->path());
        QCOMPARE(link->state(), X50Link::State::Disconnected);
        QVERIFY(d2.manualEnabled());
    }

    // R3 E: the dialog shows, and the link uses, the same manual channel (here --channel 5 over a saved 1).
    void displayedManualChannelIsTheOneOnTheWire()
    {
        fake->setDropReplies(true);
        link->setManualChannel(5);
        connectLink(X50Link::State::Manual);
        SettingsDialog d(link, fake, dir->path());
        QCOMPARE(d.channelSpin()->value(), 5);  // the link's effective manual value, not the saved 1
        fake->clearReceived();
        QSignalSpy sent(link, &X50Link::sent);
        link->send(combi(0));
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 1, 3000);
        QList<QByteArray> cv;
        for (const QByteArray &m : fake->received())
            if (isChannelVoice(m))
                cv << m;
        QCOMPARE(cv.first().at(0) & 0x0F, d.channelSpin()->value() - 1);
    }

    // Item 1: opening and closing Settings untouched leaves a --channel 5 equivalent at 5.
    void openAndCloseKeepsCommandLineChannel()
    {
        fake->setDropReplies(true);
        link->setManualChannel(5);
        link->setManualBankMap(BankMap::Korg);
        connectLink(X50Link::State::Manual);
        {
            SettingsDialog d(link, fake, dir->path());
            QCOMPARE(d.channelSpin()->value(), 5);
            d.reject();
        }
        QCOMPARE(link->manualChannel(), 5);
        QCOMPARE(link->manualBankMap(), BankMap::Korg);
        QCOMPARE(link->channel(), 5);
    }

    // R4 P2: saved channel 1, launched with --channel 5, only the Bank Map is changed: the
    // channel 5 from the command line must not be persisted.
    void onlyTheEditedManualFieldIsSaved()
    {
        {
            ConnectionSettings saved;
            saved.channel = 1;
            saved.bankMap = BankMap::Gm;
            QVERIFY(saved.save(dir->path()));
        }
        fake->setDropReplies(true);
        link->setManualChannel(5);
        connectLink(X50Link::State::Manual);
        {
            SettingsDialog d(link, fake, dir->path());
            QCOMPARE(d.channelSpin()->value(), 5);
            d.bankMapCombo()->setCurrentIndex(1);  // KORG
            d.reject();
        }
        const ConnectionSettings after = ConnectionSettings::load(dir->path());
        QCOMPARE(after.channel, 1);
        QCOMPARE(after.bankMap, BankMap::Korg);
        // And the other way round: only the channel edited leaves the saved Bank Map alone.
        {
            SettingsDialog d(link, fake, dir->path());
            d.channelSpin()->setValue(8);
            d.reject();
        }
        const ConnectionSettings again = ConnectionSettings::load(dir->path());
        QCOMPARE(again.channel, 8);
        QCOMPARE(again.bankMap, BankMap::Korg);
    }
};

QTEST_MAIN(TestSettingsDialog)
#include "test_settings_dialog.moc"
