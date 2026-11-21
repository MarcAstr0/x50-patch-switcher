// T1.7: FakeX50 on its own. Expected bytes come from docs/MIDI.md (section
// cited above each test), typed by hand.
#include <initializer_list>

#include <QSignalSpy>
#include <QTest>

#include "midi/FakeX50.h"
#include "midi/X50Protocol.h"

using namespace X50Protocol;
using A = SoundAddress;

namespace {
QByteArray H(std::initializer_list<int> list) {
    QByteArray b;
    for (int v : list)
        b.append(static_cast<char>(v));
    return b;
}
void openBoth(FakeX50 &f) {
    QVERIFY(f.openOutput(0).ok);
    QVERIFY(f.openInput(0).ok);
}
}  // namespace

class TestFakeX50 : public QObject {
    Q_OBJECT
private slots:
    // MIDI.md section 6: request F0 7E 7F 06 01 F7, reply F0 7E 0g 06 02 42 7A 00 mm 00 vv nn rr bb F7.
    // Replies are never synchronous with send().
    void inquiryIsAsynchronous() {
        FakeX50 f;
        f.setGlobalChannel(3);
        f.setVersion(1, 5, 0, 0);
        openBoth(f);
        QSignalSpy spy(&f, &MidiBackend::messageReceived);
        QVERIFY(f.send(H({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7})).ok);
        QCOMPARE(spy.count(), 0);
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toByteArray(),
                 H({0xF0, 0x7E, 0x02, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x01, 0x00, 0x01, 0x05, 0x00, 0x00, 0xF7}));
    }

    // MIDI.md section 6: member 09 = microX.
    void microXMemberCode() {
        FakeX50 f;
        f.setMicroX(true);
        openBoth(f);
        QSignalSpy spy(&f, &MidiBackend::messageReceived);
        f.send(deviceInquiryRequest());
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toByteArray().at(8), char(0x09));
    }

    // MIDI.md section 5: Mode and State on its own channel only; bytes 7, 8 carry Bank Map and enables.
    void modeAndStateReflectsConfig() {
        FakeX50 f;
        f.setGlobalChannel(2);
        f.setBankMap(BankMap::Korg);
        f.setEnables(true, false, true);
        f.setMode(Mode::CombiPlay);
        openBoth(f);
        QSignalSpy spy(&f, &MidiBackend::messageReceived);
        f.send(H({0xF0, 0x42, 0x30, 0x7A, 0x12, 0xF7}));  // channel 1: not ours
        f.send(H({0xF0, 0x42, 0x31, 0x7A, 0x12, 0xF7}));
        QTRY_COMPARE(spy.count(), 1);
        const QByteArray r = spy.at(0).at(0).toByteArray();
        QCOMPARE(r.size(), 27);
        QCOMPARE(int(r.at(5)), 0);               // COMBI PLAY
        QCOMPARE(int(r.at(7) & 0x10), 0);        // Bank Map KORG
        QCOMPARE(int(r.at(8) & 0x07), 0x05);     // PC and Combi on, Bank off
    }

    // MIDI.md section 3: CC0, CC32, PC select the sound; MIDI.md section 4: Mode Change then ACK.
    void selectAndModeChange() {
        FakeX50 f;
        f.setBankMap(BankMap::Gm);
        f.setMode(Mode::ProgPlay);
        openBoth(f);
        QSignalSpy spy(&f, &MidiBackend::messageReceived);
        f.send(H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x00, 0xF7}));
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toByteArray(), H({0xF0, 0x42, 0x30, 0x7A, 0x23, 0xF7}));
        QCOMPARE(f.mode(), Mode::CombiPlay);
        f.send(H({0xB0, 0x00, 0x3F}));
        f.send(H({0xB0, 0x20, 0x02}));
        f.send(H({0xC0, 0x09}));
        QTRY_COMPARE(f.currentSound(), A(A::Kind::Combi, A::Bank::C, 9));
    }

    // MIDI.md section 3: with MIDI Filter Program Change off nothing switches;
    // with Bank off only the number changes within the current bank.
    void enableFlagsGate() {
        FakeX50 f;
        f.setBankMap(BankMap::Korg);
        openBoth(f);
        f.setEnables(false, true, true);
        f.send(H({0xC0, 0x05}));
        QTest::qWait(30);
        QCOMPARE(f.currentSound(), A(A::Kind::Prog, A::Bank::A, 0));
        f.setEnables(true, false, true);
        f.send(H({0xB0, 0x00, 0x00}));
        f.send(H({0xB0, 0x20, 0x02}));
        f.send(H({0xC0, 0x07}));
        QTRY_COMPARE(f.currentSound(), A(A::Kind::Prog, A::Bank::A, 7));
    }

    // MIDI.md section 3: Combi flag off, in Combination mode a PC does not change the combination.
    void combiFlagOff() {
        FakeX50 f;
        f.setMode(Mode::CombiPlay);
        f.setEnables(true, true, false);
        openBoth(f);
        f.send(H({0xC0, 0x05}));
        QTest::qWait(30);
        QCOMPARE(f.currentSound(), A(A::Kind::Combi, A::Bank::A, 0));
    }

    // MIDI.md section 7: 1C 1b 00 00 returns the 4C dump of that bank; names via the record layout.
    void dumpRequestAnswered() {
        FakeX50 f;
        f.setProgramName(1, 5, QStringLiteral("My Lead"));
        f.setProgramCategory(1, 5, 9);
        openBoth(f);
        QSignalSpy spy(&f, &MidiBackend::messageReceived);
        f.send(H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x11, 0x00, 0x00, 0xF7}));
        QTRY_COMPARE(spy.count(), 1);
        const auto d = parseProgramDump(spy.at(0).at(0).toByteArray(), 1);
        QVERIFY2(d.ok, qPrintable(d.error));
        QCOMPARE(d.value.bank, 1);
        const auto recs = sliceProgramRecords(d.value.data);
        QVERIFY(recs.ok);
        QCOMPARE(recs.value.size(), 128);
        QCOMPARE(recs.value.at(5).name.text, QStringLiteral("My Lead"));
        QCOMPARE(recs.value.at(5).category, 9);
    }

    // MIDI.md section 8: the panel transmits 4E and CC0/CC32/PC.
    void panelTransmits() {
        FakeX50 f;
        f.setBankMap(BankMap::Korg);
        openBoth(f);
        QSignalSpy spy(&f, &MidiBackend::messageReceived);
        f.panelChangeMode(Mode::CombiPlay);
        f.panelSelect(A(A::Kind::Combi, A::Bank::B, 3));
        QTRY_COMPARE(spy.count(), 4);
        QCOMPARE(spy.at(0).at(0).toByteArray(), H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x00, 0xF7}));
        QCOMPARE(spy.at(1).at(0).toByteArray(), H({0xB0, 0x00, 0x00}));
        QCOMPARE(spy.at(2).at(0).toByteArray(), H({0xB0, 0x20, 0x01}));
        QCOMPARE(spy.at(3).at(0).toByteArray(), H({0xC0, 0x03}));
    }

    // R-T1.7 item 1. MIDI.md section 7: request F0 42 3g 7A 1C 1b 00 00 F7 (b=0 is bank A),
    // answered by a 4C dump. Fragmented delivery (WinMM style) is joined before the listener.
    void fragmentedDumpArrivesWhole() {
        FakeX50 whole;
        openBoth(whole);
        QSignalSpy w(&whole, &MidiBackend::messageReceived);
        whole.send(H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x10, 0x00, 0x00, 0xF7}));
        QTRY_COMPARE(w.count(), 1);

        FakeX50 f;
        f.setSysExFragmentSize(1024);
        openBoth(f);
        QSignalSpy spy(&f, &MidiBackend::messageReceived);
        f.send(H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x10, 0x00, 0x00, 0xF7}));
        QTRY_COMPARE(spy.count(), 1);
        const QByteArray dump = spy.at(0).at(0).toByteArray();
        QVERIFY(dump.size() > 60000);
        QCOMPARE(dump, w.at(0).at(0).toByteArray());
    }

    // R-T1.7 item 8: messages queued before a close/reopen are dropped.
    // MIDI.md section 3: panel selection is CC0, CC32, PC; section 6: inquiry request.
    void staleQueuedMessagesAreDroppedAfterReopen() {
        FakeX50 f;
        openBoth(f);
        QSignalSpy spy(&f, &MidiBackend::messageReceived);
        const unsigned int g = f.generation();
        f.panelSelect(A(A::Kind::Prog, A::Bank::B, 2));
        f.send(H({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}));
        f.closePorts();
        openBoth(f);
        QVERIFY(f.generation() > g);
        QTest::qWait(50);
        QCOMPARE(spy.count(), 0);
        // New traffic on the new connection still works.
        f.send(H({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}));
        QTRY_COMPARE(spy.count(), 1);
    }

    void portsAndBusy() {
        FakeX50 f;
        QCOMPARE(f.ports().outputs.size(), 1);
        f.setPortBusy(true);
        const auto st = f.openOutput(0);
        QVERIFY(!st.ok);
        QCOMPARE(st.error, QStringLiteral("port busy (another program is using the X50)"));
        f.unplug();
        QVERIFY(f.ports().outputs.isEmpty());
        QVERIFY(!f.send(H({0xF8})).ok);
    }
};

QTEST_MAIN(TestFakeX50)
#include "test_fake_x50.moc"
