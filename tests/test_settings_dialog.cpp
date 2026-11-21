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

#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "ui/ConnectionSettings.h"
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
        QVERIFY(row.text.startsWith(QStringLiteral("No reply. Is the X50 on")));
        QCOMPARE(row.hint, QStringLiteral("GLOBAL 1: MIDI, tick \"Exclusive\""));
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
};

QTEST_MAIN(TestSettingsDialog)
#include "test_settings_dialog.moc"
