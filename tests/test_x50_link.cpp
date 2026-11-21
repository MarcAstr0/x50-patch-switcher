// T1.7: X50Link against FakeX50 (headless, no real MIDI). Every expected byte
// string is typed from docs/MIDI.md; the section is cited above each test.
// Timings are injected so no test waits for a real 10 s or 2 s.
#include <initializer_list>

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

#include "core/Catalogue.h"
#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "midi/X50Protocol.h"

using namespace X50Protocol;
using A = SoundAddress;
using K = SoundAddress::Kind;
using B = SoundAddress::Bank;
using Item = X50Link::CheckItem;

Q_DECLARE_METATYPE(FakeX50::Malformation)

namespace {

QByteArray H(std::initializer_list<int> list) {
    QByteArray b;
    for (int v : list)
        b.append(static_cast<char>(v));
    return b;
}

// MIDI.md section 4: Mode Change requests.
const QByteArray kToProg = H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x02, 0xF7});
const QByteArray kToCombi = H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x00, 0xF7});
// MIDI.md sections 5 and 6.
const QByteArray kInquiry = H({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7});
const QByteArray kModeState = H({0xF0, 0x42, 0x30, 0x7A, 0x12, 0xF7});

bool isChannelVoice(const QByteArray &m) {
    return !m.isEmpty() && (static_cast<unsigned char>(m.at(0)) & 0xF0) >= 0x80 &&
           static_cast<unsigned char>(m.at(0)) < 0xF0;
}

}  // namespace

class TestX50Link : public QObject {
    Q_OBJECT

    FakeX50 *fake = nullptr;
    X50Link *link = nullptr;

    static X50Link::Timing fastTiming() {
        X50Link::Timing t;
        t.pollMs = 30;
        t.requestMs = 300;
        t.modeChangeWaitMs = 100;
        t.bankMs = 3000;
        t.echoMs = 300;
        return t;
    }

    void connectLink() {
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
    }
    // Messages the app sent after the two probe requests.
    QList<QByteArray> afterProbe() const {
        QList<QByteArray> r = fake->received();
        return r.mid(2);
    }
    static QList<QByteArray> channelVoice(const QList<QByteArray> &l) {
        QList<QByteArray> r;
        for (const QByteArray &m : l)
            if (isChannelVoice(m))
                r << m;
        return r;
    }
    static const Item *find(const X50Link::ProbeResult &p, const QString &text) {
        for (const Item &i : p.items)
            if (i.text.contains(text))
                return &i;
        return nullptr;
    }

private slots:
    void initTestCase() {
        qRegisterMetaType<SoundAddress>("SoundAddress");
        qRegisterMetaType<X50Link::State>("X50Link::State");
        qRegisterMetaType<X50Link::ProbeResult>("X50Link::ProbeResult");
        qRegisterMetaType<X50Link::ReadResult>("X50Link::ReadResult");
        qRegisterMetaType<X50Protocol::Mode>("X50Protocol::Mode");
    }
    void init() {
        fake = new FakeX50(this);
        link = new X50Link(fake, this);
        link->setTiming(fastTiming());
        link->setPortNames(QStringLiteral("fake"));
    }
    void cleanup() {
        delete link;
        delete fake;
        link = nullptr;
        fake = nullptr;
    }

    // ------------------------------------------------------------ connection

    // SPEC 4.5: ports are found by substring, case-insensitive.
    void connectsBySubstring() {
        link->setPortNames(QStringLiteral("MIDI 1"));
        QSignalSpy states(link, &X50Link::stateChanged);
        connectLink();
        QCOMPARE(link->connectedPortName(), QStringLiteral("X50 Fake MIDI 1"));
        QCOMPARE(states.at(0).at(0).value<X50Link::State>(), X50Link::State::Probing);
        QCOMPARE(states.last().at(0).value<X50Link::State>(), X50Link::State::Connected);
    }

    void noMatchingPortStaysDisconnected() {
        link->setPortNames(QStringLiteral("nonexistent"));
        link->start();
        QTest::qWait(100);
        QCOMPARE(link->state(), X50Link::State::Disconnected);
        QVERIFY(link->statusText().contains(QStringLiteral("No X50 port found")));
    }

    // SPEC 4.5: while disconnected a poll re-scans; once connected the poll stops.
    void reconnectPollRunsOnlyWhileDisconnected() {
        fake->unplug();
        link->start();
        QCOMPARE(link->state(), X50Link::State::Disconnected);
        QTest::qWait(150);
        QVERIFY(fake->portListCount() >= 3);  // polled several times at 30 ms
        fake->plug();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
        const int calls = fake->portListCount();
        QTest::qWait(200);
        QCOMPARE(fake->portListCount(), calls);  // no timer while connected
    }

    void portDeathDisconnectsAndPollReconnects() {
        connectLink();
        fake->unplug();
        QTRY_COMPARE(link->state(), X50Link::State::Disconnected);
        fake->plug();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
    }

    // SPEC 7.4: open failure on an existing port.
    void portBusy() {
        fake->setPortBusy(true);
        QSignalSpy errors(link, &X50Link::errorOccurred);
        link->start();
        QTest::qWait(200);  // several polls at 30 ms
        QCOMPARE(link->state(), X50Link::State::Disconnected);
        QCOMPARE(errors.count(), 1);  // reported once, not every poll
        QCOMPARE(errors.at(0).at(0).toString(),
                 QStringLiteral("port busy (another program is using the X50)"));
        fake->setPortBusy(false);
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
    }

    // ----------------------------------------------------------------- probe

    // MIDI.md sections 5 and 6: inquiry on 7F, then Mode and State on the learned channel.
    void probeHappyPath() {
        fake->setMode(Mode::CombiPlay);
        fake->setGlobalChannel(5);
        fake->setVersion(1, 5, 0, 0);
        QSignalSpy done(link, &X50Link::probeFinished);
        connectLink();
        QCOMPARE(fake->received().at(0), kInquiry);
        QCOMPARE(fake->received().at(1), H({0xF0, 0x42, 0x34, 0x7A, 0x12, 0xF7}));  // g = 4
        QCOMPARE(link->channel(), 5);
        QCOMPARE(*link->believedMode(), Mode::CombiPlay);
        QVERIFY(link->bankMapKnown());
        QCOMPARE(link->bankMap(), BankMap::Gm);
        QCOMPARE(done.count(), 1);
        const X50Link::ProbeResult &p = link->lastProbe();
        QVERIFY(p.allOk());
        QVERIFY(p.x50Confirmed);
        QVERIFY(find(p, QStringLiteral("X50 found (system 1.05, global channel 5)")));
        QVERIFY(find(p, QStringLiteral("Program Change enabled")));
        QVERIFY(find(p, QStringLiteral("Bank Change enabled")));
        QVERIFY(find(p, QStringLiteral("Combination Change enabled")));
        QVERIFY(find(p, QStringLiteral("Bank Map: GM")));
        QVERIFY(find(p, QStringLiteral("SysEx: replies received")));
        QCOMPARE(*link->currentSound(), A(K::Combi, B::A, 0));
    }

    // Bank Map KORG: the probe learns it and the CC0 bytes change (MIDI.md section 3 table).
    void bankMapKorgChangesCc0() {
        fake->setBankMap(BankMap::Korg);
        connectLink();
        QCOMPARE(link->bankMap(), BankMap::Korg);
        QVERIFY(find(link->lastProbe(), QStringLiteral("Bank Map: KORG")));
        link->send(A(K::Prog, B::B, 5));
        QTRY_COMPARE(fake->currentSound(), A(K::Prog, B::B, 5));
        QCOMPARE(channelVoice(afterProbe()),
                 (QList<QByteArray>{H({0xB0, 0x00, 0x00}), H({0xB0, 0x20, 0x01}), H({0xC0, 0x05})}));
    }

    void bankMapGmCc0() {
        connectLink();
        link->send(A(K::Prog, B::B, 5));
        QTRY_COMPARE(fake->currentSound(), A(K::Prog, B::B, 5));
        QCOMPARE(channelVoice(afterProbe()),
                 (QList<QByteArray>{H({0xB0, 0x00, 0x3F}), H({0xB0, 0x20, 0x01}), H({0xC0, 0x05})}));
    }

    // SPEC 8: member 09 refuses and sends nothing.
    void microXRefusal() {
        fake->setMicroX(true);
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Refused, 3000);
        QVERIFY(link->lastProbe().microX);
        QVERIFY(find(link->lastProbe(), QStringLiteral("This is a microX; X50 Patch Switcher supports the X50 only")));
        QSignalSpy errors(link, &X50Link::errorOccurred);
        QSignalSpy failed(link, &X50Link::sendFailed);
        link->send(A(K::Prog, B::A, 1));
        QCOMPARE(failed.count(), 1);
        QVERIFY(errors.at(0).at(0).toString().contains(QStringLiteral("microX")));
        QTest::qWait(50);
        QCOMPARE(fake->received(), (QList<QByteArray>{kInquiry}));  // nothing else, not even Mode and State
    }

    // SPEC 4.5: no inquiry reply in 1 s (injected: 300 ms) means manual mode.
    void noInquiryReplyIsManualModeAndStillSends() {
        fake->setDropReplies(true);
        QElapsedTimer t;
        t.start();
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        QVERIFY(t.elapsed() >= 250);
        const X50Link::ProbeResult &p = link->lastProbe();
        QVERIFY(!p.x50Confirmed);
        QVERIFY(find(p, QStringLiteral("No reply. Is the X50 on, the USB cable in, and GLOBAL 1: MIDI \"Exclusive\" ticked?")));
        QVERIFY(!link->bankMapKnown());
        fake->clearReceived();
        // Manual defaults: channel 1, GM. Mode unknown: Mode Change goes first.
        link->send(A(K::Combi, B::B, 9));
        QTRY_COMPARE(fake->currentSound(), A(K::Combi, B::B, 9));
        QCOMPARE(fake->received().first(), kToCombi);
        QCOMPARE(channelVoice(fake->received()),
                 (QList<QByteArray>{H({0xB0, 0x00, 0x3F}), H({0xB0, 0x20, 0x01}), H({0xC0, 0x09})}));
    }

    void manualChannelAndBankMapAreUsed() {
        fake->setDropReplies(true);
        fake->setGlobalChannel(4);
        fake->setBankMap(BankMap::Korg);
        link->setManualChannel(4);
        link->setManualBankMap(BankMap::Korg);
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        fake->clearReceived();
        link->send(A(K::Prog, B::C, 2));
        QTRY_COMPARE(fake->currentSound(), A(K::Prog, B::C, 2));
        QVERIFY(fake->received().contains(H({0xF0, 0x42, 0x33, 0x7A, 0x4E, 0x02, 0xF7})));
    }

    void unknownMemberCodeIsManual() {
        fake->setMemberCode(0x02);
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        const Item *i = find(link->lastProbe(), QStringLiteral("Could not confirm an X50"));
        QVERIFY(i);
        QCOMPARE(i->status, Item::Status::Failed);
    }

    // Inquiry fine, Mode and State unanswered: manual, with a checklist line.
    void modeAndStateDropped() {
        fake->setDrop(FakeX50::Reply::ModeAndState, true);
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        QVERIFY(link->lastProbe().x50Confirmed);
        QVERIFY(find(link->lastProbe(), QStringLiteral("No reply to Mode and State")));
    }

    // A reply that fails validation is reported with its raw bytes (MIDI.md section 6: 15 bytes).
    void malformedInquiryReplyCarriesRawBytes() {
        fake->setDropReplies(true);
        QSignalSpy errors(link, &X50Link::errorOccurred);
        link->start();
        QCOMPARE(link->state(), X50Link::State::Probing);
        const QByteArray bad = H({0xF0, 0x7E, 0x00, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x01, 0x00, 0x01, 0x05, 0xF7});
        emit fake->messageReceived(bad);
        QCOMPARE(link->state(), X50Link::State::Manual);
        QCOMPARE(errors.count(), 1);
        QCOMPARE(errors.at(0).at(1).toByteArray(), bad);
    }

    void malformedModeAndStateReplyCarriesRawBytes() {
        fake->setDrop(FakeX50::Reply::ModeAndState, true);
        QSignalSpy errors(link, &X50Link::errorOccurred);
        link->start();
        QTRY_VERIFY(fake->received().size() >= 2);
        QTest::qWait(20);
        // 27 bytes with the byte-6 marker wrong (MIDI.md section 5 says 06).
        const QByteArray bad = H({0xF0, 0x42, 0x30, 0x7A, 0x42, 0x02, 0x07, 0x10, 0x07, 0x00, 0x02, 0x01, 0x05, 0x00,
                                  0x00, 0x02, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0xF7});
        emit fake->messageReceived(bad);
        QCOMPARE(link->state(), X50Link::State::Manual);
        QCOMPARE(errors.last().at(1).toByteArray(), bad);
    }

    void enableFlagOff_data() {
        QTest::addColumn<int>("flag");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("fix");
        QTest::newRow("program change") << 0 << QStringLiteral("Program Change disabled") << QStringLiteral("\"Program Change\"");
        QTest::newRow("bank change") << 1 << QStringLiteral("Bank Change disabled") << QStringLiteral("\"Bank\"");
        QTest::newRow("combination change") << 2 << QStringLiteral("Combination Change disabled") << QStringLiteral("\"Combi\"");
    }
    void enableFlagOff() {
        QFETCH(int, flag);
        QFETCH(QString, text);
        QFETCH(QString, fix);
        fake->setEnables(flag != 0, flag != 1, flag != 2);
        connectLink();
        const X50Link::ProbeResult &p = link->lastProbe();
        const Item *i = find(p, text);
        QVERIFY(i);
        QCOMPARE(i->status, Item::Status::Failed);
        QVERIFY(i->fix.startsWith(QStringLiteral("GLOBAL 1: MIDI")));
        QVERIFY(i->fix.contains(fix));
        QVERIFY(!p.allOk());
        int failed = 0;
        for (const Item &it : p.items)
            failed += it.status == Item::Status::Failed;
        QCOMPARE(failed, 1);  // only the one that is off
    }

    void programBankEIsSuspect() {
        fake->setReportedProgramBank(4);
        connectLink();
        QCOMPARE(link->state(), X50Link::State::Connected);
        const Item *i = find(link->lastProbe(), QStringLiteral("Suspect"));
        QVERIFY(i);
        QCOMPARE(i->status, Item::Status::Warning);
        QVERIFY(!link->lastProbe().allOk());
    }

    void recheckRunsProbeAgain() {
        connectLink();
        QSignalSpy done(link, &X50Link::probeFinished);
        fake->setBankMap(BankMap::Korg);
        link->probe();
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(link->bankMap(), BankMap::Korg);
    }

    // ------------------------------------------------------------------ send

    // MIDI.md section 3 step 1: Mode Change only when the mode differs or is unknown.
    void modeChangeOnlyWhenNeeded() {
        connectLink();  // fake is in PROG PLAY
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Prog, B::A, 1));
        QTRY_COMPARE(sent.count(), 1);
        QVERIFY(!afterProbe().contains(kToProg));
        QVERIFY(!afterProbe().contains(kToCombi));
        fake->clearReceived();
        link->send(A(K::Combi, B::C, 100));
        QTRY_COMPARE(sent.count(), 2);
        QCOMPARE(fake->received().first(), kToCombi);
        QTRY_COMPARE(fake->currentSound(), A(K::Combi, B::C, 100));
        fake->clearReceived();
        link->send(A(K::Combi, B::A, 2));
        QTRY_COMPARE(sent.count(), 3);
        QTRY_COMPARE(fake->currentSound(), A(K::Combi, B::A, 2));
        QVERIFY(!fake->received().contains(kToCombi));
        fake->clearReceived();
        link->send(A(K::Prog, B::D, 127));
        QTRY_COMPARE(sent.count(), 4);
        QCOMPARE(fake->received().first(), kToProg);
        QTRY_COMPARE(fake->currentSound(), A(K::Prog, B::D, 127));
        QCOMPARE(*link->believedMode(), Mode::ProgPlay);
    }

    // The ACK ends the wait early.
    void ackEndsTheWaitEarly() {
        X50Link::Timing t = fastTiming();
        t.modeChangeWaitMs = 2000;
        link->setTiming(t);
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        QElapsedTimer el;
        el.start();
        link->send(A(K::Combi, B::A, 0));
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 1, 1500);
        QVERIFY(el.elapsed() < 1000);
    }

    // No ACK: the program change goes out after the 100 ms wait.
    void noAckWaitsTheHundredMilliseconds() {
        fake->setDrop(FakeX50::Reply::Ack, true);
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        QElapsedTimer el;
        el.start();
        link->send(A(K::Combi, B::A, 0));
        QTest::qWait(40);
        QCOMPARE(sent.count(), 0);
        QCOMPARE(channelVoice(fake->received()).size(), 0);
        QTRY_COMPARE(sent.count(), 1);
        QVERIFY(el.elapsed() >= 90);
        QTRY_COMPARE(fake->currentSound(), A(K::Combi, B::A, 0));
    }

    // A late ACK (after the wait) changes nothing.
    void slowAckDoesNotBlock() {
        connectLink();
        fake->setReplyDelayMs(400);
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Combi, B::B, 1));
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 1, 300);
        QTest::qWait(450);
        QCOMPARE(sent.count(), 1);
    }

    // MIDI.md section 4: NAK 24 cc. The program change must not follow.
    void nakToModeChange() {
        connectLink();
        fake->setNakModeChange(0x02);
        QSignalSpy errors(link, &X50Link::errorOccurred);
        QSignalSpy sent(link, &X50Link::sent);
        QSignalSpy failed(link, &X50Link::sendFailed);
        link->send(A(K::Combi, B::A, 3));
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(errors.at(0).at(0).toString().contains(QStringLiteral("the mode is wrong")));
        QCOMPARE(errors.at(0).at(1).toByteArray(), H({0xF0, 0x42, 0x30, 0x7A, 0x24, 0x02, 0xF7}));
        QTest::qWait(200);  // past the 100 ms wait: still nothing
        QCOMPARE(sent.count(), 0);
        QCOMPARE(channelVoice(afterProbe()).size(), 0);
        QCOMPARE(fake->currentSound(), A(K::Prog, B::A, 0));
        QCOMPARE(*link->believedMode(), Mode::ProgPlay);
    }

    void sendWhenDisconnectedDoesNothing() {
        QSignalSpy failed(link, &X50Link::sendFailed);
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Prog, B::A, 1));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(sent.count(), 0);
        QVERIFY(fake->received().isEmpty());
    }

    void invalidAddressIsRefused() {
        connectLink();
        QSignalSpy failed(link, &X50Link::sendFailed);
        link->send(A(K::Combi, B::D, 0));  // no combination bank D
        QCOMPARE(failed.count(), 1);
        QVERIFY(afterProbe().isEmpty());
    }

    void sendDuringProbeIsRefused() {
        link->start();
        QCOMPARE(link->state(), X50Link::State::Probing);
        QSignalSpy failed(link, &X50Link::sendFailed);
        link->send(A(K::Prog, B::A, 1));
        QCOMPARE(failed.count(), 1);
    }

    // The newest send wins while an earlier one waits for its Mode Change reply.
    void newerSendSupersedesPendingOne() {
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Combi, B::A, 11));
        link->send(A(K::Combi, B::B, 22));
        QTRY_COMPARE(sent.count(), 1);
        QCOMPARE(sent.at(0).at(0).value<SoundAddress>(), A(K::Combi, B::B, 22));
        QTest::qWait(150);
        QCOMPARE(sent.count(), 1);
        QVERIFY(!fake->received().contains(H({0xC0, 0x0B})));
        QCOMPARE(fake->currentSound(), A(K::Combi, B::B, 22));
    }

    // MIDI.md section 3 table: G and g(d) bank selects are the same in both maps.
    void gmBankSelect() {
        connectLink();
        link->send(A(K::Prog, B::G, 1));
        QTRY_COMPARE(fake->currentSound(), A(K::Prog, B::G, 1));
        QCOMPARE(channelVoice(afterProbe()),
                 (QList<QByteArray>{H({0xB0, 0x00, 0x79}), H({0xB0, 0x20, 0x00}), H({0xC0, 0x00})}));
    }

    // ---------------------------------------------------------------- follow

    // SPEC 4.4 / MIDI.md section 8.
    void followsPanelSoundAndMode() {
        connectLink();
        QSignalSpy sound(link, &X50Link::followedSound);
        QSignalSpy mode(link, &X50Link::followedMode);
        fake->panelSelect(A(K::Prog, B::C, 7));
        QTRY_COMPARE(sound.count(), 1);
        QCOMPARE(sound.at(0).at(0).value<SoundAddress>(), A(K::Prog, B::C, 7));
        QCOMPARE(*link->currentSound(), A(K::Prog, B::C, 7));
        fake->panelChangeMode(Mode::CombiPlay);
        QTRY_COMPARE(mode.count(), 1);
        QCOMPARE(mode.at(0).at(0).value<Mode>(), Mode::CombiPlay);
        QCOMPARE(*link->believedMode(), Mode::CombiPlay);
        QVERIFY(!link->currentSound());  // the combination is unknown until the panel sends one
        fake->panelSelect(A(K::Combi, B::B, 100));
        QTRY_COMPARE(sound.count(), 2);
        QCOMPARE(sound.at(1).at(0).value<SoundAddress>(), A(K::Combi, B::B, 100));
        // GM program from the panel (not reportable in Mode and State) is still followed.
        fake->panelChangeMode(Mode::ProgPlay);
        fake->panelSelect(A(K::Prog, B::Gd, 9));
        QTRY_COMPARE(sound.count(), 3);
        QCOMPARE(sound.at(2).at(0).value<SoundAddress>(), A(K::Prog, B::Gd, 9));
    }

    // The mode we then send in must be the followed one: no Mode Change after a panel change.
    void followedModeAvoidsNeedlessModeChange() {
        connectLink();
        fake->panelChangeMode(Mode::CombiPlay);
        QTRY_COMPARE(*link->believedMode(), Mode::CombiPlay);
        fake->clearReceived();
        link->send(A(K::Combi, B::A, 4));
        QTRY_COMPARE(fake->currentSound(), A(K::Combi, B::A, 4));
        QVERIFY(!fake->received().contains(kToCombi));
    }

    void bankMapKorgPanelSelectIsDecodedWithKorgMap() {
        fake->setBankMap(BankMap::Korg);
        connectLink();
        QSignalSpy sound(link, &X50Link::followedSound);
        fake->panelSelect(A(K::Prog, B::D, 3));
        QTRY_COMPARE(sound.count(), 1);
        QCOMPARE(sound.at(0).at(0).value<SoundAddress>(), A(K::Prog, B::D, 3));
    }

    // SPEC 4.4: echoes within 300 ms of our own send are ignored.
    void echoSuppression() {
        X50Link::Timing t = fastTiming();
        t.echoMs = 200;
        link->setTiming(t);
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        QSignalSpy sound(link, &X50Link::followedSound);
        link->send(A(K::Prog, B::A, 1));
        QTRY_COMPARE(sent.count(), 1);
        // The X50 echoing our own selection (MIDI.md section 3, GM): ignored.
        emit fake->messageReceived(H({0xB0, 0x00, 0x3F}));
        emit fake->messageReceived(H({0xB0, 0x20, 0x00}));
        emit fake->messageReceived(H({0xC0, 0x01}));
        QTest::qWait(20);
        QCOMPARE(sound.count(), 0);
        QCOMPARE(*link->currentSound(), A(K::Prog, B::A, 1));
        // After the window the same bytes are a real panel change.
        QTest::qWait(250);
        emit fake->messageReceived(H({0xB0, 0x00, 0x3F}));
        emit fake->messageReceived(H({0xB0, 0x20, 0x00}));
        emit fake->messageReceived(H({0xC0, 0x02}));
        QCOMPARE(sound.count(), 1);
        QCOMPARE(sound.at(0).at(0).value<SoundAddress>(), A(K::Prog, B::A, 2));
    }

    void otherChannelIsNotFollowed() {
        connectLink();
        QSignalSpy sound(link, &X50Link::followedSound);
        emit fake->messageReceived(H({0xB1, 0x00, 0x3F}));
        emit fake->messageReceived(H({0xB1, 0x20, 0x00}));
        emit fake->messageReceived(H({0xC1, 0x02}));
        QCOMPARE(sound.count(), 0);
    }

    void realTimeAndUnknownMessagesAreIgnored() {
        connectLink();
        QSignalSpy sound(link, &X50Link::followedSound);
        QSignalSpy errors(link, &X50Link::errorOccurred);
        emit fake->messageReceived(H({0xF8}));
        emit fake->messageReceived(H({0xFE}));
        emit fake->messageReceived(H({0xF0, 0x42, 0x30, 0x7A, 0x40, 0x00, 0xF7}));  // current-buffer dump: ignored
        QCOMPARE(sound.count(), 0);
        QCOMPARE(errors.count(), 0);
    }

    // ------------------------------------------------------------------ read

    // MIDI.md section 7 requests, in order, then names and categories to the catalogue.
    void readNamesWithProgress() {
        fake->setProgramName(1, 5, QStringLiteral("My Lead"));
        fake->setProgramCategory(1, 5, 3);
        fake->setCombinationName(2, 127, QStringLiteral("Last Combi"));
        fake->setCategoryNames({QStringLiteral("Keyboard"), QStringLiteral("Organ"), QStringLiteral("Bass"),
                                QStringLiteral("Lead")},
                               {QStringLiteral("CKeys")});
        connectLink();
        QSignalSpy progress(link, &X50Link::readProgress);
        QSignalSpy finished(link, &X50Link::readFinished);
        QSignalSpy failed(link, &X50Link::readFailed);
        link->readNames();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(afterProbe(),
                 (QList<QByteArray>{H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x10, 0x00, 0x00, 0xF7}),
                                    H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x11, 0x00, 0x00, 0xF7}),
                                    H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x12, 0x00, 0x00, 0xF7}),
                                    H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x13, 0x00, 0x00, 0xF7}),
                                    H({0xF0, 0x42, 0x30, 0x7A, 0x1D, 0x10, 0x00, 0x00, 0xF7}),
                                    H({0xF0, 0x42, 0x30, 0x7A, 0x1D, 0x11, 0x00, 0x00, 0xF7}),
                                    H({0xF0, 0x42, 0x30, 0x7A, 0x1D, 0x12, 0x00, 0x00, 0xF7}),
                                    H({0xF0, 0x42, 0x30, 0x7A, 0x0E, 0x00, 0xF7})}));
        QCOMPARE(progress.count(), 9);  // 0/8 at the start, then one per bank
        QCOMPARE(progress.at(0).at(0).toInt(), 0);
        QCOMPARE(progress.at(1).at(0).toInt(), 1);
        QCOMPARE(progress.at(1).at(2).toString(), QStringLiteral("Program A"));
        QCOMPARE(progress.at(8).at(0).toInt(), 8);
        QCOMPARE(progress.at(8).at(1).toInt(), 8);
        QCOMPARE(progress.at(8).at(2).toString(), QStringLiteral("Global"));

        const X50Link::ReadResult res = finished.at(0).at(0).value<X50Link::ReadResult>();
        QVERIFY(res.complete);
        QCOMPARE(res.names.size(), 7 * 128);
        QCOMPARE(res.names.value(A(K::Prog, B::B, 5)).name, QStringLiteral("My Lead"));
        QCOMPARE(res.names.value(A(K::Prog, B::B, 5)).category, QStringLiteral("Lead"));
        QCOMPARE(res.names.value(A(K::Combi, B::C, 127)).name, QStringLiteral("Last Combi"));
        QVERIFY(res.haveCategories);
        QCOMPARE(res.programCategories.size(), 16);
        QCOMPARE(res.programCategories.at(1), QStringLiteral("Organ"));
        QCOMPARE(res.combinationCategories.at(0), QStringLiteral("CKeys"));

        QCOMPARE(link->rawDumps().size(), 8);
        QCOMPARE(link->rawDumps().at(0).label, QStringLiteral("Program A"));
        QVERIFY(link->rawDumps().at(0).bytes.startsWith(H({0xF0, 0x42, 0x30, 0x7A, 0x4C})));
        QVERIFY(link->rawDumps().at(7).bytes.startsWith(H({0xF0, 0x42, 0x30, 0x7A, 0x51})));
        QVERIFY(!link->isReading());

        Catalogue cat;
        QVERIFY(cat.loadFromResource());
        res.applyTo(cat);
        QCOMPARE(cat.effectiveName(A(K::Prog, B::B, 5)), QStringLiteral("My Lead"));
        QVERIFY(cat.hasUnitCategories());
    }

    // Cancel at any time: here from inside the progress signal, mid-read.
    void cancelMidRead() {
        connectLink();
        QSignalSpy finished(link, &X50Link::readFinished);
        QSignalSpy cancelled(link, &X50Link::readCancelled);
        QSignalSpy failed(link, &X50Link::readFailed);
        connect(link, &X50Link::readProgress, link, [this](int done, int, const QString &) {
            if (done == 2)
                link->cancelRead();
        });
        link->readNames();
        QTRY_COMPARE(cancelled.count(), 1);
        QVERIFY(!link->isReading());
        QTest::qWait(100);  // anything still in flight is ignored
        QCOMPARE(finished.count(), 0);
        QCOMPARE(failed.count(), 0);
        // The link is usable again.
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Prog, B::A, 9));
        QTRY_COMPARE(sent.count(), 1);
    }

    void cancelBeforeAnyReply() {
        fake->setDrop(FakeX50::Reply::Dump, true);
        connectLink();
        QSignalSpy cancelled(link, &X50Link::readCancelled);
        link->readNames();
        QVERIFY(link->isReading());
        link->cancelRead();
        QCOMPARE(cancelled.count(), 1);
        link->cancelRead();  // idempotent
        QCOMPARE(cancelled.count(), 1);
    }

    // SPEC 4.6: 10 s per bank, injected here as 150 ms.
    void bankTimeout() {
        X50Link::Timing t = fastTiming();
        t.bankMs = 150;
        link->setTiming(t);
        fake->setDrop(FakeX50::Reply::Dump, true);
        connectLink();
        QSignalSpy failed(link, &X50Link::readFailed);
        QElapsedTimer el;
        el.start();
        link->readNames();
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(el.elapsed() >= 130);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("Program A"));
        QVERIFY(failed.at(0).at(1).toString().contains(QStringLiteral("no reply")));
        QVERIFY(!link->isReading());
    }

    void timeoutDefaultsAreTheSpecifiedOnes() {
        X50Link::Timing t;
        QCOMPARE(t.pollMs, 2000);
        QCOMPARE(t.requestMs, 1000);
        QCOMPARE(t.modeChangeWaitMs, 100);
        QCOMPARE(t.bankMs, 10000);
        QCOMPARE(t.echoMs, 300);
    }

    // MIDI.md section 4 / 7: the X50 answers a dump request with NAK 24 cc.
    void nakToDumpRequest() {
        connectLink();
        fake->setNakDumps(0x01);
        QSignalSpy failed(link, &X50Link::readFailed);
        link->readNames();
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("Program A"));
        QVERIFY(failed.at(0).at(1).toString().contains(QStringLiteral("does not exist")));
        QCOMPARE(failed.at(0).at(2).toByteArray(), H({0xF0, 0x42, 0x30, 0x7A, 0x24, 0x01, 0xF7}));
    }

    void malformedDump_data() {
        QTest::addColumn<FakeX50::Malformation>("m");
        QTest::newRow("127 records") << FakeX50::Malformation::FewerRecords;
        QTest::newRow("odd length") << FakeX50::Malformation::OddLength;
        QTest::newRow("status byte in data") << FakeX50::Malformation::StatusByteInData;
        QTest::newRow("wrong bank") << FakeX50::Malformation::WrongBank;
        QTest::newRow("no F7") << FakeX50::Malformation::NoEnd;
    }
    void malformedDump() {
        QFETCH(FakeX50::Malformation, m);
        fake->setMalformedDump(m);
        connectLink();
        QSignalSpy failed(link, &X50Link::readFailed);
        QSignalSpy finished(link, &X50Link::readFinished);
        link->readNames();
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(finished.count(), 0);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("Program A"));
        QVERIFY(!failed.at(0).at(1).toString().isEmpty());
        const QByteArray raw = failed.at(0).at(2).toByteArray();
        QVERIFY(raw.size() > 100);
        QVERIFY(raw.startsWith(H({0xF0, 0x42, 0x30, 0x7A, 0x4C})));
        QCOMPARE(link->rawDumps().size(), 1);
        QCOMPARE(link->rawDumps().at(0).bytes, raw);
        QCOMPARE(afterProbe().size(), 1);  // stopped: no second request
    }

    // --diagnose mode: keep going and collect every error and every raw dump.
    void continueOnErrorKeepsReading() {
        fake->setMalformedDump(FakeX50::Malformation::FewerRecords);
        connectLink();
        QSignalSpy finished(link, &X50Link::readFinished);
        link->readNames(true);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        const X50Link::ReadResult res = finished.at(0).at(0).value<X50Link::ReadResult>();
        QVERIFY(!res.complete);
        QCOMPARE(res.errors.size(), 8);
        QVERIFY(res.errors.at(0).startsWith(QStringLiteral("Program A: ")));
        QCOMPARE(link->rawDumps().size(), 8);
        QCOMPARE(res.names.size(), 0);
    }

    void sendDuringReadIsRefused() {
        fake->setDrop(FakeX50::Reply::Dump, true);
        connectLink();
        link->readNames();
        QSignalSpy failed(link, &X50Link::sendFailed);
        link->send(A(K::Prog, B::A, 1));
        QCOMPARE(failed.count(), 1);
        QVERIFY(link->isReading());
    }

    void secondReadWhileReadingIsRefused() {
        fake->setDrop(FakeX50::Reply::Dump, true);
        connectLink();
        link->readNames();
        QSignalSpy errors(link, &X50Link::errorOccurred);
        link->readNames();
        QCOMPARE(errors.count(), 1);
    }

    void readRefusedWhenDisconnectedOrMicroX() {
        QSignalSpy errors(link, &X50Link::errorOccurred);
        link->readNames();
        QCOMPARE(errors.count(), 1);
        QVERIFY(!link->isReading());
    }

    void connectionLostDuringRead() {
        fake->setDrop(FakeX50::Reply::Dump, true);
        connectLink();
        QSignalSpy failed(link, &X50Link::readFailed);
        link->readNames();
        fake->unplug();
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(link->state(), X50Link::State::Disconnected);
    }
};

QTEST_MAIN(TestX50Link)
#include "test_x50_link.moc"
