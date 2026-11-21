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
        QVERIFY(link->statusIsFailure());
    }

    // statusIsFailure() is set for a failed port search and cleared once connected.
    void statusIsFailureClearsOnConnect() {
        fake->unplug();
        link->start();
        QVERIFY(link->statusIsFailure());
        fake->plug();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
        QVERIFY(!link->statusIsFailure());
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
        QVERIFY(find(p, QStringLiteral("No reply from the X50")));
        QVERIFY(p.items.first().fix.contains(QStringLiteral("check Exclusive")));  // MIDI.md section 3
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
        const Item *i = find(link->lastProbe(), QStringLiteral("A device answered but it is not an X50 (member code 02)"));
        QVERIFY(i);
        QCOMPARE(i->status, Item::Status::Failed);
        QVERIFY(!i->raw.isEmpty());  // the reply's bytes (MIDI.md section 6)
    }

    // A channel learned from the Device Inquiry reply (MIDI.md section 6) survives a manual change
    // when Mode and State then fails; the manual one applies again after the next disconnect.
    void learnedChannelIsNotOverwrittenByManual() {
        fake->setGlobalChannel(5);
        fake->setDrop(FakeX50::Reply::ModeAndState, true);
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        QCOMPARE(link->channel(), 5);
        link->setManualChannel(9);
        QCOMPARE(link->channel(), 5);
        link->stop();
        QCOMPARE(link->channel(), 9);
    }

    // stop() ends a running read and a pending send with their failure signals.
    void stopFailsRunningReadAndPendingSend() {
        connectLink();
        fake->setDropReplies(true);
        QSignalSpy readFailed(link, &X50Link::readFailed);
        link->readNames();
        QVERIFY(link->isBusy());
        link->stop();
        QCOMPARE(readFailed.count(), 1);
        QVERIFY(!link->isBusy());

        fake->setDropReplies(false);
        connectLink();
        const auto kind = link->believedMode() == X50Protocol::Mode::CombiPlay ? K::Prog : K::Combi;
        QSignalSpy sendFailed(link, &X50Link::sendFailed);
        link->send(A(kind, B::A, 0));
        QVERIFY(link->isBusy());  // waiting on the Mode Change
        link->stop();
        QCOMPARE(sendFailed.count(), 1);
    }

    // A probe started while a Mode Change wait is pending fails that send.
    void probeDuringModeWaitFailsPendingSend() {
        connectLink();
        const auto kind = link->believedMode() == X50Protocol::Mode::CombiPlay ? K::Prog : K::Combi;
        QSignalSpy sendFailed(link, &X50Link::sendFailed);
        link->send(A(kind, B::A, 0));
        QVERIFY(link->isBusy());
        link->probe();
        QCOMPARE(sendFailed.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
    }

    // The "reported once" memory of the connect poll is forgotten by stop(): a busy port that
    // is reported, stopped and started again reports again.
    void stopForgetsLastRaisedFailure() {
        fake->setPortBusy(true);
        QSignalSpy errors(link, &X50Link::errorOccurred);
        link->start();
        QTest::qWait(100);
        QCOMPARE(errors.count(), 1);
        link->stop();
        link->start();
        QCOMPARE(errors.count(), 2);
        link->stop();
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

    // ============================================================ R-T1.7 fix round

    // Item 1. MIDI.md section 7: bank dumps; delivered in 1024-byte WinMM-style fragments
    // and reassembled before X50Link sees them: a full read still completes.
    void fullReadThroughFragments() {
        fake->setSysExFragmentSize(1024);
        fake->setProgramName(1, 5, QStringLiteral("My Lead"));
        connectLink();
        QSignalSpy finished(link, &X50Link::readFinished);
        link->readNames();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        const X50Link::ReadResult res = finished.at(0).at(0).value<X50Link::ReadResult>();
        QVERIFY(res.complete);
        QCOMPARE(res.names.size(), 7 * 128);
        QCOMPARE(res.names.value(A(K::Prog, B::B, 5)).name, QStringLiteral("My Lead"));
        QCOMPARE(link->rawDumps().size(), 8);
    }

    // Item 2. Learned mode is forgotten on disconnect: with the replies dropped on
    // reconnect (manual mode) a COMBI send must start with a Mode Change again.
    // MIDI.md section 4: Mode Change F0 42 3g 7A 4E 0m F7, m = 0 COMBI PLAY.
    void learnedStateIsClearedOnDisconnect() {
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Combi, B::A, 1));
        QTRY_COMPARE(sent.count(), 1);
        QVERIFY(fake->received().contains(kToCombi));
        QCOMPARE(*link->believedMode(), Mode::CombiPlay);
        link->stop();
        QVERIFY(!link->believedMode());
        QVERIFY(!link->currentSound());
        QVERIFY(!link->bankMapKnown());
        fake->setDropReplies(true);
        link->setManualChannel(1);
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        QVERIFY(!link->believedMode());
        fake->clearReceived();
        link->send(A(K::Combi, B::A, 2));
        QTRY_COMPARE(sent.count(), 2);
        QCOMPARE(fake->received().first(), kToCombi);
    }

    // Item 2: manual values are never overwritten by learned ones.
    void manualFallbacksSurviveLearning() {
        fake->setGlobalChannel(5);
        fake->setBankMap(BankMap::Korg);
        link->setManualChannel(2);
        link->setManualBankMap(BankMap::Gm);
        connectLink();
        QCOMPARE(link->channel(), 5);
        QCOMPARE(link->bankMap(), BankMap::Korg);
        link->stop();
        QCOMPARE(link->channel(), 2);
        QCOMPARE(link->bankMap(), BankMap::Gm);
    }

    // Item 3. MIDI.md section 3/4: PROG believed, send COMBI A011, then PROG B022 before
    // the ACK: the wire ends with a PROG Mode Change then the B022 select; `sent` is B022.
    void sendDuringModeChangeKeepsTheInflightEffect() {
        connectLink();  // PROG PLAY
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Combi, B::A, 11));
        link->send(A(K::Prog, B::B, 22));
        QTRY_COMPARE(sent.count(), 1);
        QCOMPARE(sent.at(0).at(0).value<SoundAddress>(), A(K::Prog, B::B, 22));
        const QList<QByteArray> wire = afterProbe();
        QCOMPARE(wire, (QList<QByteArray>{kToCombi, kToProg, H({0xB0, 0x00, 0x3F}),
                                          H({0xB0, 0x20, 0x01}), H({0xC0, 0x16})}));
        QTRY_COMPARE(fake->currentSound(), A(K::Prog, B::B, 22));
        QCOMPARE(*link->believedMode(), Mode::ProgPlay);
        QTest::qWait(150);
        QCOMPARE(sent.count(), 1);
    }

    // Item 3, same-kind variant: the second send needs no further Mode Change.
    void sendDuringModeChangeSameKindNeedsOneChange() {
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Combi, B::A, 11));
        link->send(A(K::Combi, B::B, 22));
        QTRY_COMPARE(sent.count(), 1);
        QCOMPARE(afterProbe().first(), kToCombi);
        QCOMPARE(afterProbe().count(kToCombi), 1);
        QVERIFY(!afterProbe().contains(kToProg));
    }

    // Item 4. MIDI.md section 3 (GM): A001 is B0 00 3F, B0 20 00, C0 01; B002 is
    // B0 00 3F, B0 20 01, C0 02. A panel change within 300 ms of our send is not swallowed.
    void echoSuppressionConsumesOnlyTheExpectedEcho() {
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        QSignalSpy sound(link, &X50Link::followedSound);
        link->send(A(K::Prog, B::A, 1));
        QTRY_COMPARE(sent.count(), 1);
        fake->panelSelect(A(K::Prog, B::B, 2));
        QTRY_COMPARE(sound.count(), 1);
        QCOMPARE(sound.at(0).at(0).value<SoundAddress>(), A(K::Prog, B::B, 2));
        QCOMPARE(*link->currentSound(), A(K::Prog, B::B, 2));
    }

    // Item 4: the same bytes after the echo was consumed are a real panel change.
    void echoIsConsumedOnce() {
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        QSignalSpy sound(link, &X50Link::followedSound);
        link->send(A(K::Prog, B::A, 1));
        QTRY_COMPARE(sent.count(), 1);
        emit fake->messageReceived(H({0xB0, 0x00, 0x3F}));
        emit fake->messageReceived(H({0xB0, 0x20, 0x00}));
        emit fake->messageReceived(H({0xC0, 0x01}));
        QCOMPARE(sound.count(), 0);
        emit fake->messageReceived(H({0xB0, 0x00, 0x3F}));
        emit fake->messageReceived(H({0xB0, 0x20, 0x00}));
        emit fake->messageReceived(H({0xC0, 0x03}));
        QCOMPARE(sound.count(), 1);
        QCOMPARE(sound.at(0).at(0).value<SoundAddress>(), A(K::Prog, B::A, 3));
    }

    // Item 5. The port vanishes without a driver error; the read times out and the link
    // notices the ports are gone, goes Disconnected, and polling resumes.
    void timeoutWithVanishedPortDisconnects() {
        X50Link::Timing t = fastTiming();
        t.bankMs = 150;
        link->setTiming(t);
        connectLink();
        fake->setDrop(FakeX50::Reply::Dump, true);
        QSignalSpy failed(link, &X50Link::readFailed);
        link->readNames();
        fake->unplug(false);
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(failed.at(0).at(1).toString().contains(QStringLiteral("gone")));
        QCOMPARE(link->state(), X50Link::State::Disconnected);
        const int calls = fake->portListCount();
        QTest::qWait(100);
        QVERIFY(fake->portListCount() > calls);  // polling again
        fake->plug();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
    }

    // Item 5, probe variant: ports gone while the Device Inquiry is unanswered.
    void probeTimeoutWithVanishedPortDisconnects() {
        fake->setDropReplies(true);
        link->start();
        QCOMPARE(link->state(), X50Link::State::Probing);
        fake->unplug(false);
        QTRY_COMPARE(link->state(), X50Link::State::Disconnected);
    }

    // Item 6. MIDI.md section 7. A well-formed dump for another bank is ignored; the
    // step's timeout decides (here: "no reply"), not a parse failure.
    void wrongBankDumpIsIgnoredUntilTimeout() {
        X50Link::Timing t = fastTiming();
        t.bankMs = 150;
        link->setTiming(t);
        fake->setMalformedDump(FakeX50::Malformation::WrongBank);
        connectLink();
        QSignalSpy failed(link, &X50Link::readFailed);
        link->readNames();
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("Program A"));
        QVERIFY(failed.at(0).at(1).toString().contains(QStringLiteral("no reply")));
        QCOMPARE(link->rawDumps().size(), 0);
    }

    // Item 6: cancel mid-bank, read again at once with delayed dumps: the late dump of the
    // first request is harmless and the new read completes.
    void lateDumpAfterCancelDoesNotBreakTheNextRead() {
        connectLink();
        fake->setReplyDelayMs(40);
        QSignalSpy cancelled(link, &X50Link::readCancelled);
        QSignalSpy failed(link, &X50Link::readFailed);
        QSignalSpy finished(link, &X50Link::readFinished);
        link->readNames();
        link->cancelRead();
        QCOMPARE(cancelled.count(), 1);
        link->readNames();
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        QCOMPARE(failed.count(), 0);
        const X50Link::ReadResult res = finished.at(0).at(0).value<X50Link::ReadResult>();
        QVERIFY(res.complete);
        QCOMPARE(res.names.size(), 7 * 128);
    }

    // Item 7: the Mode Change write fails.
    void modeChangeWriteFailureEmitsSendFailed() {
        connectLink();
        fake->setFailSend(true);
        QSignalSpy failed(link, &X50Link::sendFailed);
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Combi, B::A, 1));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).value<SoundAddress>(), A(K::Combi, B::A, 1));
        QCOMPARE(sent.count(), 0);
        QCOMPARE(link->state(), X50Link::State::Disconnected);
        link->stop();
    }

    // Item 7: the port dies while the Mode Change wait is pending.
    void portDeathDuringModeWaitEmitsSendFailed() {
        X50Link::Timing t = fastTiming();
        t.modeChangeWaitMs = 2000;
        link->setTiming(t);
        connectLink();
        fake->setDrop(FakeX50::Reply::Ack, true);
        QSignalSpy failed(link, &X50Link::sendFailed);
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Combi, B::A, 1));
        fake->unplug();
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).value<SoundAddress>(), A(K::Combi, B::A, 1));
        QCOMPARE(sent.count(), 0);
        QCOMPARE(link->state(), X50Link::State::Disconnected);
    }

    // Item 8, at link level: after stop() and a restart, anything the fake had queued for
    // the old connection never reaches the link.
    void staleMessagesFromTheOldConnectionAreDropped() {
        connectLink();
        QSignalSpy sound(link, &X50Link::followedSound);
        fake->panelSelect(A(K::Prog, B::C, 7));  // queued, not yet delivered
        link->stop();
        link->start();                           // reopen before the event loop runs
        QTest::qWait(100);
        QCOMPARE(sound.count(), 0);
    }

    // ---------------------------------------------------------------- R1 fixes

    // RtMidi 6.0.0 WinMM appends " <portIndex>" to every port name: output and input names of
    // one device differ only by that number and must still match.
    void portNameIndexSuffixIsIgnored() {
        QCOMPARE(X50Link::stripPortIndex(QStringLiteral("USB MIDI Device 1")), QStringLiteral("USB MIDI Device"));
        QCOMPARE(X50Link::stripPortIndex(QStringLiteral("X50 12")), QStringLiteral("X50"));
        QCOMPARE(X50Link::stripPortIndex(QStringLiteral("X50")), QStringLiteral("X50"));
        QCOMPARE(X50Link::stripPortIndex(QStringLiteral("USB MIDI Device")), QStringLiteral("USB MIDI Device"));
        QCOMPARE(X50Link::stripPortIndex(QStringLiteral("MIDI2")), QStringLiteral("MIDI2"));
        QCOMPARE(X50Link::stripPortIndex(QStringLiteral(" 7")), QStringLiteral(" 7"));
        QVERIFY(X50Link::portNameMatches(QStringLiteral("USB MIDI Device 1"), QStringLiteral("usb midi device 0")));
        QVERIFY(X50Link::portNameMatches(QStringLiteral("USB MIDI Device 1"), QStringLiteral("Device 1")));
        QVERIFY(X50Link::portNameMatches(QStringLiteral("KORG X50 3"), QStringLiteral("X50")));
        QVERIFY(!X50Link::portNameMatches(QStringLiteral("KORG X50 3"), QStringLiteral("microX")));
        QVERIFY(!X50Link::portNameMatches(QStringLiteral("KORG X50 3"), QString()));
    }

    // Three passes: exact, raw substring over all names, then index-stripped. With two identical
    // devices "Device 1" must pick "USB MIDI Device 1", not the first stripped match.
    void findPortIndexPrefersExactThenRawSubstring() {
        const QStringList two{QStringLiteral("USB MIDI Device 0"), QStringLiteral("USB MIDI Device 1")};
        QCOMPARE(X50Link::findPortIndex(two, QStringLiteral("Device 1")), 1);
        QCOMPARE(X50Link::findPortIndex(two, QStringLiteral("usb midi device 1")), 1);  // exact, any case
        QCOMPARE(X50Link::findPortIndex(two, QStringLiteral("Device 0")), 0);
        const QStringList other{QStringLiteral("USB MIDI Device 4"), QStringLiteral("Other 5")};
        QCOMPARE(X50Link::findPortIndex(other, QStringLiteral("usb midi device 0")), 0);  // pass 3
        QCOMPARE(X50Link::findPortIndex(two, QStringLiteral("microX")), -1);
        QCOMPARE(X50Link::findPortIndex(two, QString()), -1);
    }

    void connectsWhenTheSubstringCarriesAnotherIndex() {
        fake->setPortName(QStringLiteral("USB MIDI Device 1"));
        link->setPortNames(QStringLiteral("usb midi device 0"));  // the input list's spelling
        connectLink();
        QCOMPARE(link->connectedPortName(), QStringLiteral("USB MIDI Device 1"));
    }

    // A refused Device Inquiry reply is flagged on the probe result with its bytes.
    void refusedInquiryIsFlagged() {
        fake->setGlobalChannel(3);
        // MIDI.md section 6 with the family byte 7A altered to 7B: refused, channel nibble 2.
        const QByteArray bad = H({0xF0, 0x7E, 0x02, 0x06, 0x02, 0x42, 0x7B, 0x00, 0x01, 0x00, 0x01, 0x05, 0x00, 0x00, 0xF7});
        fake->setInquiryReplyOverride(bad);
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        QVERIFY(link->lastProbe().inquiryRefused);
        QCOMPARE(link->lastProbe().inquiryRaw, bad);
        QVERIFY(!link->lastProbe().inquiry.has_value());
    }

    // A reply shaped like an inquiry reply but with byte 2 not of the 0g form is refused (with its
    // bytes), not ignored as silence.
    void inquiryReplyWithoutNibbleFormIsRefusedNotIgnored() {
        const QByteArray bad = H({0xF0, 0x7E, 0x7F, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x01, 0x00, 0x01, 0x05, 0x00, 0x00, 0xF7});
        fake->setInquiryReplyOverride(bad);
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        QVERIFY(link->lastProbe().inquiryRefused);
        QCOMPARE(link->lastProbe().inquiryRaw, bad);
    }

    // The read-only follow-up after a refused reply: Mode and State on the given channel,
    // then the banks, all while the link stays in manual mode.
    void modeAndStateOnlyThenReadInManualState() {
        fake->setGlobalChannel(3);
        fake->setInquiryReplyOverride(H({0xF0, 0x7E, 0x02, 0x06, 0x02, 0x42, 0x7B, 0x00, 0x01, 0x00, 0x01, 0x05, 0x00, 0x00, 0xF7}));
        link->start();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Manual, 3000);
        link->setManualChannel(3);
        QSignalSpy probed(link, &X50Link::probeFinished);
        fake->clearReceived();
        link->probeModeAndStateOnly();
        QTRY_COMPARE_WITH_TIMEOUT(probed.count(), 1, 3000);
        QCOMPARE(fake->received(), (QList<QByteArray>{H({0xF0, 0x42, 0x32, 0x7A, 0x12, 0xF7})}));  // MIDI.md section 5, channel 3
        QCOMPARE(link->state(), X50Link::State::Manual);  // an X50 is still not confirmed
        QVERIFY(link->lastProbe().state.has_value());
        QVERIFY(!link->lastProbe().x50Confirmed);
        QSignalSpy finished(link, &X50Link::readFinished);
        link->readNames(true);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        QVERIFY(finished.at(0).at(0).value<X50Link::ReadResult>().complete);
    }

    // Codex R1: a NAK of a Mode Change that a newer send superseded does not fail the newest
    // send; it is re-evaluated against the unchanged mode (here PROG PLAY: no Mode Change).
    void nakOfSupersededModeChangeReevaluatesTheNewestSend() {
        connectLink();  // PROG PLAY
        fake->setNakModeChange(0x02);
        QSignalSpy sent(link, &X50Link::sent);
        QSignalSpy failed(link, &X50Link::sendFailed);
        link->send(A(K::Combi, B::A, 11));  // Mode Change to COMBI, which the X50 refuses
        link->send(A(K::Prog, B::B, 22));   // supersedes it; PROG PLAY is where we already are
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 1, 3000);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(sent.at(0).at(0).value<SoundAddress>(), A(K::Prog, B::B, 22));
        QCOMPARE(*link->believedMode(), Mode::ProgPlay);
        QCOMPARE(afterProbe(), (QList<QByteArray>{kToCombi, H({0xB0, 0x00, 0x3F}), H({0xB0, 0x20, 0x01}), H({0xC0, 0x16})}));
    }

    // ... and when the superseding send needs yet another mode change, that one is made.
    void nakOfSupersededModeChangeStartsTheNeededOne() {
        connectLink();
        fake->setMode(Mode::CombiPlay);
        link->probe();
        QTRY_COMPARE_WITH_TIMEOUT(link->state(), X50Link::State::Connected, 3000);
        QCOMPARE(*link->believedMode(), Mode::CombiPlay);
        fake->setNakModeChange(0x02);
        fake->clearReceived();
        QSignalSpy failed(link, &X50Link::sendFailed);
        QSignalSpy sent(link, &X50Link::sent);
        link->send(A(K::Prog, B::A, 1));    // Mode Change to PROG: refused
        link->send(A(K::Combi, B::A, 2));   // already in COMBI PLAY
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 1, 3000);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(*link->currentSound(), A(K::Combi, B::A, 2));
    }

    // Codex R1: two sends close together: every echo tuple that has not expired is kept, so
    // neither one's echo is taken for the player.
    void overlappingSendsKeepAllEchoTuples() {
        connectLink();
        QSignalSpy sent(link, &X50Link::sent);
        QSignalSpy sound(link, &X50Link::followedSound);
        link->send(A(K::Prog, B::A, 1));  // MIDI.md section 3 (GM): B0 00 3F, B0 20 00, C0 01
        link->send(A(K::Prog, B::A, 2));  // ... C0 02
        QCOMPARE(sent.count(), 2);
        for (const QByteArray &m : {H({0xB0, 0x00, 0x3F}), H({0xB0, 0x20, 0x00}), H({0xC0, 0x01}),
                                    H({0xB0, 0x00, 0x3F}), H({0xB0, 0x20, 0x00}), H({0xC0, 0x02})})
            emit fake->messageReceived(m);
        QCOMPARE(sound.count(), 0);
        // After both echoes the same bytes are the player again.
        emit fake->messageReceived(H({0xB0, 0x00, 0x3F}));
        emit fake->messageReceived(H({0xB0, 0x20, 0x00}));
        emit fake->messageReceived(H({0xC0, 0x05}));
        QCOMPARE(sound.count(), 1);
    }

    // Codex R1: a dump of an unexpected kind is parsed. Malformed: the step fails with its bytes.
    void malformedDumpOfAnotherKindFailsTheStep() {
        connectLink();
        fake->setDrop(FakeX50::Reply::Dump, true);
        QSignalSpy failed(link, &X50Link::readFailed);
        link->readNames();  // waiting for Program A
        QByteArray bad = buildCombinationDump(1, 0, QByteArray(kCombinationRecordSize * kRecordsPerBank, '\0')).value;
        QVERIFY(bad.size() > 30);
        bad[20] = static_cast<char>(0x90);  // a status byte inside the data
        emit fake->messageReceived(bad);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("Program A"));
        QCOMPARE(failed.at(0).at(2).toByteArray(), bad);
    }

    // ... and a well formed late dump of another kind is ignored: the step times out as before.
    void wellFormedDumpOfAnotherKindIsIgnored() {
        X50Link::Timing t = fastTiming();
        t.bankMs = 150;
        link->setTiming(t);
        connectLink();
        fake->setDrop(FakeX50::Reply::Dump, true);
        QSignalSpy failed(link, &X50Link::readFailed);
        link->readNames();
        const QByteArray good = buildCombinationDump(1, 0, QByteArray(kCombinationRecordSize * kRecordsPerBank, '\0')).value;
        emit fake->messageReceived(good);
        QCOMPARE(failed.count(), 0);
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(failed.at(0).at(1).toString().contains(QStringLiteral("no reply")));
    }

    // Codex R1: the timeout check looks for the exact ports that were opened. Another port
    // that also contains the substring must not hide that the X50 is gone.
    void timeoutChecksTheExactOpenedPortNames() {
        X50Link::Timing t = fastTiming();
        t.bankMs = 150;
        link->setTiming(t);
        connectLink();  // opened "X50 Fake MIDI 1", found by "fake"
        fake->setDrop(FakeX50::Reply::Dump, true);
        QSignalSpy failed(link, &X50Link::readFailed);
        link->readNames();
        fake->setPortName(QStringLiteral("Other Fake Thing"));  // still matches "fake", is not the opened port
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(failed.at(0).at(1).toString().contains(QStringLiteral("gone")));
    }
};

QTEST_MAIN(TestX50Link)
#include "test_x50_link.moc"
