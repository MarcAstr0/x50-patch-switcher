// T1.6: X50Protocol. Every expected byte below is typed by hand from
// docs/MIDI.md; the section is cited above each test. IMP = Korg's MIDI
// Implementation text (line numbers are MIDI.md's, after tr -d '\r').
#include <initializer_list>
#include <optional>
#include <random>

#include <QTest>

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

// A rejected parse must carry the exact input and a message.
template <typename R>
void verifyRejected(const R &r, const QByteArray &input, const char *what) {
    QVERIFY2(!r.ok, what);
    QVERIFY2(!r.error.isEmpty(), what);
    QCOMPARE(r.raw, input);
}

// The six standard negatives of the brief. Indices say where the manufacturer,
// function and channel bytes sit; `chanHigh` is the correct high nibble of the
// channel byte (3 for Korg frames, 0 for Device Inquiry).
template <typename Parse>
void standardNegatives(const QByteArray &good, int mfrIdx, int funcIdx, int chanIdx, int chanHigh,
                       bool fixedLength, Parse parse) {
    QVERIFY(parse(good, std::nullopt).ok);

    QByteArray b = good;
    b[mfrIdx] = static_cast<char>(0x41);  // Roland instead of KORG
    verifyRejected(parse(b, std::nullopt), b, "wrong manufacturer");

    // Wrong channel: high nibble wrong, and expected channel mismatch.
    b = good;
    b[chanIdx] = static_cast<char>(((chanHigh + 1) << 4) | (good.at(chanIdx) & 0x0F));
    verifyRejected(parse(b, std::nullopt), b, "wrong channel nibble");
    const int g = (good.at(chanIdx) & 0x0F) + 1;
    const int other = g == 16 ? 1 : g + 1;
    verifyRejected(parse(good, other), good, "expected channel mismatch");
    QVERIFY(parse(good, g).ok);

    b = good;
    b[funcIdx] = static_cast<char>(good.at(funcIdx) ^ 0x01);
    verifyRejected(parse(b, std::nullopt), b, "wrong function");

    b = good.left(good.size() - 1);
    verifyRejected(parse(b, std::nullopt), b, "truncated by one byte");

    b = good;
    b.append('\0');  // a byte after F7
    verifyRejected(parse(b, std::nullopt), b, "extra byte after F7");

    b = good;
    b.chop(1);
    b.append('\0');  // F7 replaced by 00
    verifyRejected(parse(b, std::nullopt), b, "missing F7");

    if (fixedLength) {
        b = good;
        b.insert(good.size() - 1, '\0');  // extra byte before F7
        verifyRejected(parse(b, std::nullopt), b, "extra byte before F7");
        b = good;
        b.remove(good.size() - 2, 1);  // truncated, F7 kept
        verifyRejected(parse(b, std::nullopt), b, "truncated, F7 kept");
    }
}

QByteArray syntheticProgramData() {
    QByteArray d;
    for (int i = 0; i < kRecordsPerBank; ++i) {
        QByteArray rec(kProgramRecordSize, '\0');
        const QByteArray name = QStringLiteral("Prog %1").arg(i, 3, 10, QLatin1Char('0')).toLatin1();
        for (int j = 0; j < kNameLength; ++j)
            rec[j] = j < name.size() ? name.at(j) : ' ';
        rec[110] = static_cast<char>(i % 16);
        d.append(rec);
    }
    return d;
}

QByteArray syntheticCombiData() {
    QByteArray d;
    for (int i = 0; i < kRecordsPerBank; ++i) {
        QByteArray rec(kCombinationRecordSize, '\0');
        const QByteArray name = QStringLiteral("Combi %1").arg(i, 3, 10, QLatin1Char('0')).toLatin1();
        for (int j = 0; j < kNameLength; ++j)
            rec[j] = j < name.size() ? name.at(j) : ' ';
        rec[118] = static_cast<char>(0xF0 | (i % 16));  // high nibble reserved, must be masked
        d.append(rec);
    }
    return d;
}

QByteArray syntheticGlobalData() {
    QByteArray d(850, '\0');  // IMP TABLE 4 goes to offset 849
    for (int i = 0; i < 16; ++i) {
        const QByteArray p = QStringLiteral("PCat%1").arg(i).toLatin1();
        const QByteArray c = QStringLiteral("CCat%1").arg(i).toLatin1();
        for (int j = 0; j < 16; ++j) {
            d[328 + i * 16 + j] = j < p.size() ? p.at(j) : ' ';
            d[584 + i * 16 + j] = j < c.size() ? c.at(j) : ' ';
        }
    }
    return d;
}

}  // namespace

class TestX50Protocol : public QObject {
    Q_OBJECT

private slots:
    // ---------------------------------------------------------- builders

    // MIDI.md section 3 (steps 2-4) and the bank table; IMP *2 (line 152).
    void programSelectBytes_data() {
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("bank");
        QTest::addColumn<int>("number");
        QTest::addColumn<int>("map");
        QTest::addColumn<int>("channel");
        QTest::addColumn<QByteArray>("cc0");
        QTest::addColumn<QByteArray>("cc32");
        QTest::addColumn<QByteArray>("pc");
        const int Co = static_cast<int>(A::Kind::Combi), Pr = static_cast<int>(A::Kind::Prog);
        const int K = static_cast<int>(BankMap::Korg), G = static_cast<int>(BankMap::Gm);
        // COMBI A000, KORG map, channel 1: B0 00 00 / B0 20 00 / C0 00
        QTest::newRow("combi A0 korg ch1") << Co << int(A::Bank::A) << 0 << K << 1
            << H({0xB0, 0x00, 0x00}) << H({0xB0, 0x20, 0x00}) << H({0xC0, 0x00});
        // PROG D127, GM map, channel 16: BF 00 3F / BF 20 03 / CF 7F
        QTest::newRow("prog D127 gm ch16") << Pr << int(A::Bank::D) << 127 << G << 16
            << H({0xBF, 0x00, 0x3F}) << H({0xBF, 0x20, 0x03}) << H({0xCF, 0x7F});
        // PROG B 10, KORG, ch 3: B2 00 00 / B2 20 01 / C2 0A
        QTest::newRow("prog B10 korg ch3") << Pr << int(A::Bank::B) << 10 << K << 3
            << H({0xB2, 0x00, 0x00}) << H({0xB2, 0x20, 0x01}) << H({0xC2, 0x0A});
        // PROG G001, either map, ch 10: B9 00 79 / B9 20 00 / C9 00
        QTest::newRow("prog G1 korg ch10") << Pr << int(A::Bank::G) << 1 << K << 10
            << H({0xB9, 0x00, 0x79}) << H({0xB9, 0x20, 0x00}) << H({0xC9, 0x00});
        QTest::newRow("prog G128 gm ch1") << Pr << int(A::Bank::G) << 128 << G << 1
            << H({0xB0, 0x00, 0x79}) << H({0xB0, 0x20, 0x00}) << H({0xC0, 0x7F});
        // PROG g(d)128, KORG, ch 2: B1 00 78 / B1 20 00 / C1 7F
        QTest::newRow("prog gd128 korg ch2") << Pr << int(A::Bank::Gd) << 128 << K << 2
            << H({0xB1, 0x00, 0x78}) << H({0xB1, 0x20, 0x00}) << H({0xC1, 0x7F});
        QTest::newRow("prog gd1 gm ch2") << Pr << int(A::Bank::Gd) << 1 << G << 2
            << H({0xB1, 0x00, 0x78}) << H({0xB1, 0x20, 0x00}) << H({0xC1, 0x00});
        // COMBI C005, GM map, ch 1: B0 00 3F / B0 20 02 / C0 05
        QTest::newRow("combi C5 gm ch1") << Co << int(A::Bank::C) << 5 << G << 1
            << H({0xB0, 0x00, 0x3F}) << H({0xB0, 0x20, 0x02}) << H({0xC0, 0x05});
    }
    void programSelectBytes() {
        QFETCH(int, kind);
        QFETCH(int, bank);
        QFETCH(int, number);
        QFETCH(int, map);
        QFETCH(int, channel);
        QFETCH(QByteArray, cc0);
        QFETCH(QByteArray, cc32);
        QFETCH(QByteArray, pc);
        const auto r = programSelect(A(static_cast<A::Kind>(kind), static_cast<A::Bank>(bank), number),
                                     static_cast<BankMap>(map), channel);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.size(), 3);
        QCOMPARE(r.value.at(0), cc0);
        QCOMPARE(r.value.at(1), cc32);
        QCOMPARE(r.value.at(2), pc);
    }
    void programSelectRejects() {
        QVERIFY(!programSelect(A(A::Kind::Prog, A::Bank::G, 0), BankMap::Korg, 1).ok);   // G is 1-128
        QVERIFY(!programSelect(A(A::Kind::Prog, A::Bank::A, 128), BankMap::Korg, 1).ok);
        QVERIFY(!programSelect(A(A::Kind::Combi, A::Bank::D, 0), BankMap::Korg, 1).ok);  // no combi D
        QVERIFY(!programSelect(A(), BankMap::Korg, 1).ok);
        QVERIFY(!programSelect(A(A::Kind::Prog, A::Bank::A, 0), BankMap::Korg, 0).ok);
        QVERIFY(!programSelect(A(A::Kind::Prog, A::Bank::A, 0), BankMap::Korg, 17).ok);
        QVERIFY(!programSelect(A(A::Kind::Prog, A::Bank::A, 0), BankMap::Gm, -1).ok);
    }
    void modeForKind_() {
        QCOMPARE(modeForKind(A::Kind::Combi), Mode::CombiPlay);  // MIDI.md section 4: m = 0
        QCOMPARE(modeForKind(A::Kind::Prog), Mode::ProgPlay);    // m = 2
    }

    // MIDI.md section 4: F0 42 3g 7A 4E 0m F7, m = 0 COMBI PLAY, 2 PROG PLAY.
    void modeChangeBytes() {
        QCOMPARE(modeChange(1, Mode::CombiPlay).value, H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x00, 0xF7}));
        QCOMPARE(modeChange(16, Mode::ProgPlay).value, H({0xF0, 0x42, 0x3F, 0x7A, 0x4E, 0x02, 0xF7}));
        QCOMPARE(modeChange(1, Mode::ProgPlay).value, H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x02, 0xF7}));
        QCOMPARE(modeChange(16, Mode::CombiPlay).value, H({0xF0, 0x42, 0x3F, 0x7A, 0x4E, 0x00, 0xF7}));
        QVERIFY(modeChange(1, Mode::Global).ok == false);  // MIDI.md section 9
        QVERIFY(modeChange(1, Mode::Multi).ok == false);
        QVERIFY(modeChange(1, Mode::Demo).ok == false);
        QVERIFY(modeChange(1, Mode::CombiEdit).ok == false);
        QVERIFY(!modeChange(0, Mode::CombiPlay).ok);
        QVERIFY(!modeChange(17, Mode::CombiPlay).ok);
    }

    // MIDI.md section 6: F0 7E 7F 06 01 F7.
    void deviceInquiryRequestBytes() {
        QCOMPARE(deviceInquiryRequest(), H({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}));
    }

    // MIDI.md section 5: F0 42 3g 7A 12 F7.
    void modeAndStateRequestBytes() {
        QCOMPARE(modeAndStateRequest(1).value, H({0xF0, 0x42, 0x30, 0x7A, 0x12, 0xF7}));
        QCOMPARE(modeAndStateRequest(16).value, H({0xF0, 0x42, 0x3F, 0x7A, 0x12, 0xF7}));
        QVERIFY(!modeAndStateRequest(0).ok);
        QVERIFY(!modeAndStateRequest(17).ok);
    }

    // MIDI.md section 7, Requests: F0 42 3g 7A 1C 1b 00 00 F7 (byte after the
    // function is 00kk 00bb with kk = 1, so 10 + b); IMP (3), *1.
    void programBankDumpRequestBytes() {
        QCOMPARE(programBankDumpRequest(1, 0).value, H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x10, 0x00, 0x00, 0xF7}));
        QCOMPARE(programBankDumpRequest(1, 1).value, H({0xF0, 0x42, 0x30, 0x7A, 0x1C, 0x11, 0x00, 0x00, 0xF7}));
        QCOMPARE(programBankDumpRequest(16, 3).value, H({0xF0, 0x42, 0x3F, 0x7A, 0x1C, 0x13, 0x00, 0x00, 0xF7}));
        QVERIFY(!programBankDumpRequest(1, 4).ok);  // E is microX only
        QVERIFY(!programBankDumpRequest(1, -1).ok);
        QVERIFY(!programBankDumpRequest(0, 0).ok);
        QVERIFY(!programBankDumpRequest(17, 0).ok);
    }
    // MIDI.md section 7: F0 42 3g 7A 1D 1b 00 00 F7, b = 0-2; IMP (5), *2.
    void combinationBankDumpRequestBytes() {
        QCOMPARE(combinationBankDumpRequest(1, 0).value, H({0xF0, 0x42, 0x30, 0x7A, 0x1D, 0x10, 0x00, 0x00, 0xF7}));
        QCOMPARE(combinationBankDumpRequest(3, 2).value, H({0xF0, 0x42, 0x32, 0x7A, 0x1D, 0x12, 0x00, 0x00, 0xF7}));
        QCOMPARE(combinationBankDumpRequest(16, 1).value, H({0xF0, 0x42, 0x3F, 0x7A, 0x1D, 0x11, 0x00, 0x00, 0xF7}));
        QVERIFY(!combinationBankDumpRequest(1, 3).ok);
        QVERIFY(!combinationBankDumpRequest(17, 0).ok);
    }
    // MIDI.md section 7: F0 42 3g 7A 0E 00 F7; IMP (8).
    void globalDumpRequestBytes() {
        QCOMPARE(globalDumpRequest(1).value, H({0xF0, 0x42, 0x30, 0x7A, 0x0E, 0x00, 0xF7}));
        QCOMPARE(globalDumpRequest(16).value, H({0xF0, 0x42, 0x3F, 0x7A, 0x0E, 0x00, 0xF7}));
        QVERIFY(!globalDumpRequest(0).ok);
        QVERIFY(!globalDumpRequest(17).ok);
    }

    // ----------------------------------------------------------- parsers

    // MIDI.md section 6: F0 7E 0g 06 02 42 7A 00 mm 00 vv nn rr bb F7; SPEC section 8.
    void deviceInquiryReplyParse() {
        const QByteArray x50 = H({0xF0, 0x7E, 0x00, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x01, 0x00, 0x01, 0x05, 0x00, 0x01, 0xF7});
        auto r = parseDeviceInquiryReply(x50);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.channel, 1);
        QCOMPARE(r.value.memberCode, 1);
        QVERIFY(r.value.family == DeviceInquiryReply::Family::X50);
        QCOMPARE(r.value.major, 1);
        QCOMPARE(r.value.minor, 5);
        QCOMPARE(r.value.release, 0);
        QCOMPARE(r.value.build, 1);

        const QByteArray micro = H({0xF0, 0x7E, 0x0F, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x09, 0x00, 0x01, 0x00, 0x01, 0x01, 0xF7});
        r = parseDeviceInquiryReply(micro, 16);
        QVERIFY(r.ok);
        QCOMPARE(r.value.channel, 16);
        QVERIFY(r.value.family == DeviceInquiryReply::Family::MicroX);

        const QByteArray unknown = H({0xF0, 0x7E, 0x00, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x05, 0x00, 0x01, 0x00, 0x01, 0x01, 0xF7});
        r = parseDeviceInquiryReply(unknown);
        QVERIFY(r.ok);
        QVERIFY(r.value.family == DeviceInquiryReply::Family::Unknown);
    }
    void deviceInquiryReplyNegatives() {
        const QByteArray good = H({0xF0, 0x7E, 0x00, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x01, 0x00, 0x01, 0x05, 0x00, 0x01, 0xF7});
        standardNegatives(good, 5, 4, 2, 0, true,
                          [](const QByteArray &m, std::optional<int> c) { return parseDeviceInquiryReply(m, c); });
        // Fixed family bytes.
        for (int idx : {1, 3, 6, 7, 9}) {
            QByteArray b = good;
            b[idx] = static_cast<char>(good.at(idx) ^ 0x02);
            verifyRejected(parseDeviceInquiryReply(b), b, "fixed byte");
        }
    }

    // MIDI.md section 5, 27-byte reply. Hand-built: channel 1, mode 0 (COMBI
    // PLAY), data1 = 11 (Local Control + Bank Map GM), data2 = 07 (PC, bank,
    // combi enabled), protect 00, program bank C no. 5, combi bank B no. 127.
    static QByteArray modeStateGood() {
        return H({0xF0, 0x42, 0x30, 0x7A, 0x42,  // 0-4
                  0x00,                          // 5 mode
                  0x06,                          // 6
                  0x11, 0x07, 0x00,              // 7-9
                  0x02, 0x02, 0x05, 0x00,        // 10-13
                  0x00, 0x01, 0x7F, 0x00,        // 14-17
                  0x04, 0x00, 0x00, 0x00,        // 18-21
                  0x21, 0x00, 0x00, 0x00,        // 22-25
                  0xF7});                        // 26
    }
    void modeAndStateParse() {
        const auto r = parseModeAndState(modeStateGood(), 1);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.channel, 1);
        QCOMPARE(r.value.mode, Mode::CombiPlay);
        QVERIFY(r.value.bankMap == BankMap::Gm);
        QVERIFY(r.value.localControl);
        QCOMPARE(r.value.midiClock, 0);
        QVERIFY(!r.value.powerOnMemorize);
        QVERIFY(!r.value.extRealtime);
        QVERIFY(r.value.enableProgramChange);
        QVERIFY(r.value.enableBankChange);
        QVERIFY(r.value.enableCombinationChange);
        QVERIFY(!r.value.enableAftertouch);
        QVERIFY(!r.value.enableControlChange);
        QCOMPARE(r.value.memoryProtectBits, 0);
        QCOMPARE(r.value.programBank, 2);
        QCOMPARE(r.value.programNumber, 5);
        QCOMPARE(r.value.combinationBank, 1);
        QCOMPARE(r.value.combinationNumber, 127);
    }
    void modeAndStateFieldBits() {
        QByteArray b = modeStateGood();
        b[2] = 0x3F;  // channel 16
        b[5] = 0x02;  // PROG PLAY
        b[7] = 0x6E;  // KORG map (bit 4 clear), clock 3, bit 5 and 6 set
        b[8] = 0x18;  // aftertouch + control change only
        b[9] = 0x3F;
        const auto r = parseModeAndState(b);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.channel, 16);
        QCOMPARE(r.value.mode, Mode::ProgPlay);
        QVERIFY(r.value.bankMap == BankMap::Korg);
        QVERIFY(!r.value.localControl);
        QCOMPARE(r.value.midiClock, 3);
        QVERIFY(r.value.powerOnMemorize);
        QVERIFY(r.value.extRealtime);
        QVERIFY(!r.value.enableProgramChange);
        QVERIFY(!r.value.enableBankChange);
        QVERIFY(!r.value.enableCombinationChange);
        QVERIFY(r.value.enableAftertouch);
        QVERIFY(r.value.enableControlChange);
        QCOMPARE(r.value.memoryProtectBits, 0x3F);
        // Demo is reported only here (MIDI.md section 4).
        b[5] = 0x7F;
        QVERIFY(parseModeAndState(b).ok);
        b[5] = 0x05;  // not in the table
        verifyRejected(parseModeAndState(b), b, "unknown mode");
    }
    void modeAndStateNegatives() {
        const QByteArray good = modeStateGood();
        standardNegatives(good, 1, 4, 2, 3, true,
                          [](const QByteArray &m, std::optional<int> c) { return parseModeAndState(m, c); });
        for (int idx : {6, 10, 13, 14, 17, 18, 22}) {  // markers and reserved bytes
            QByteArray b = good;
            b[idx] = static_cast<char>(good.at(idx) ^ 0x01);
            verifyRejected(parseModeAndState(b), b, "marker");
        }
        QByteArray b = good;
        b[11] = 5;
        verifyRejected(parseModeAndState(b), b, "program bank out of range");
        b = good;
        b[15] = 3;
        verifyRejected(parseModeAndState(b), b, "combination bank out of range");
        b = good;
        b[12] = static_cast<char>(0x80);
        verifyRejected(parseModeAndState(b), b, "bit 7 in data");
    }

    // MIDI.md section 3 step 1 / section 4: ACK F0 42 3g 7A 23 F7.
    void ackParse() {
        const QByteArray good = H({0xF0, 0x42, 0x30, 0x7A, 0x23, 0xF7});
        const auto r = parseAckNak(good);
        QVERIFY(r.ok);
        QVERIFY(r.value.ack);
        QCOMPARE(r.value.channel, 1);
        standardNegatives(good, 1, 4, 2, 3, true,
                          [](const QByteArray &m, std::optional<int> c) { return parseAckNak(m, c); });
        const auto r16 = parseAckNak(H({0xF0, 0x42, 0x3F, 0x7A, 0x23, 0xF7}), 16);
        QVERIFY(r16.ok);
        QCOMPARE(r16.value.channel, 16);
    }
    // MIDI.md section 4: NAK F0 42 3g 7A 24 cc F7, cc per IMP *22.
    void nakParse() {
        const QByteArray good = H({0xF0, 0x42, 0x30, 0x7A, 0x24, 0x02, 0xF7});
        const auto r = parseAckNak(good);
        QVERIFY(r.ok);
        QVERIFY(!r.value.ack);
        QCOMPARE(r.value.errorCode, 2);
        QVERIFY(!nakCodeText(2).isEmpty());
        QCOMPARE(parseAckNak(H({0xF0, 0x42, 0x30, 0x7A, 0x24, 0x40, 0xF7})).value.errorCode, 0x40);
        standardNegatives(good, 1, 4, 2, 3, true,
                          [](const QByteArray &m, std::optional<int> c) { return parseAckNak(m, c); });
    }

    // MIDI.md section 4 and 8: incoming F0 42 3g 7A 4E 0m F7.
    void modeChangeParse() {
        const QByteArray good = H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x02, 0xF7});
        const auto r = parseModeChange(good);
        QVERIFY(r.ok);
        QCOMPARE(r.value.mode, Mode::ProgPlay);
        QCOMPARE(r.value.channel, 1);
        QCOMPARE(parseModeChange(H({0xF0, 0x42, 0x3F, 0x7A, 0x4E, 0x00, 0xF7})).value.mode, Mode::CombiPlay);
        QCOMPARE(parseModeChange(H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x06, 0xF7})).value.mode, Mode::Global);
        standardNegatives(good, 1, 4, 2, 3, true,
                          [](const QByteArray &m, std::optional<int> c) { return parseModeChange(m, c); });
        const QByteArray demo = H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x7F, 0xF7});
        verifyRejected(parseModeChange(demo), demo, "demo is reply-only");
        const QByteArray bad = H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x05, 0xF7});
        verifyRejected(parseModeChange(bad), bad, "mode 5 not in table");
    }

    // MIDI.md section 7, Replies: 4C 0X kind/bank program-no data F7. Kind 0
    // (all; the kind-1 and kind-2 record counts are checked in dumpKindImpliesRecordCount),
    // bank B, 3 internal bytes 80 01 FF encode to 05 00 01 7F
    // (top bits: byte 1 -> bit 0, byte 3 -> bit 2).
    void programDumpParse() {
        const QByteArray good = H({0xF0, 0x42, 0x30, 0x7A, 0x4C, 0x00, 0x01, 0x00, 0x05, 0x00, 0x01, 0x7F, 0xF7});
        const auto r = parseProgramDump(good, 1);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.channel, 1);
        QCOMPARE(r.value.kind, 0);
        QCOMPARE(r.value.bank, 1);
        QCOMPARE(r.value.number, 0);
        QCOMPARE(r.value.data, H({0x80, 0x01, 0xFF}));
        standardNegatives(good, 1, 4, 2, 3, false,
                          [](const QByteArray &m, std::optional<int> c) { return parseProgramDump(m, c); });
        QByteArray b = good;
        b[5] = 0x01;  // X25, not X50
        verifyRejected(parseProgramDump(b), b, "X25 flag");
        b = good;
        b[6] = 0x31;  // kind 3
        verifyRejected(parseProgramDump(b), b, "kind 3");
        b = good;
        b[6] = 0x15;  // bank 5
        verifyRejected(parseProgramDump(b), b, "bank 5");
        b = good;
        b[6] = 0x41;  // reserved bit 6
        verifyRejected(parseProgramDump(b), b, "reserved bits in kind/bank");
        // Data of 9 MIDI bytes: one full group plus a lone header byte.
        b = H({0xF0, 0x42, 0x30, 0x7A, 0x4C, 0x00, 0x11, 0x00, 0x00, 1, 2, 3, 4, 5, 6, 7, 0x03, 0xF7});
        verifyRejected(parseProgramDump(b), b, "malformed final group");
        // A combination dump is not a program dump.
        const QByteArray combi = H({0xF0, 0x42, 0x30, 0x7A, 0x4D, 0x00, 0x01, 0x00, 0x05, 0x00, 0x01, 0x7F, 0xF7});
        verifyRejected(parseProgramDump(combi), combi, "4D given to program parser");
        // Short: no data at all is still a well-formed (empty) dump; truncated header is not.
        const QByteArray tiny = H({0xF0, 0x42, 0x30, 0x7A, 0x4C, 0x00, 0x11, 0xF7});
        verifyRejected(parseProgramDump(tiny), tiny, "truncated header");
    }
    // MIDI.md section 7: 4D 00 kind/bank combination-no data F7. Kind 0, bank C, no. 7.
    void combinationDumpParse() {
        const QByteArray good = H({0xF0, 0x42, 0x3F, 0x7A, 0x4D, 0x00, 0x02, 0x07, 0x00, 0x41, 0xF7});
        const auto r = parseCombinationDump(good, 16);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.kind, 0);
        QCOMPARE(r.value.bank, 2);
        QCOMPARE(r.value.number, 7);
        QCOMPARE(r.value.data, H({0x41}));
        standardNegatives(good, 1, 4, 2, 3, false,
                          [](const QByteArray &m, std::optional<int> c) { return parseCombinationDump(m, c); });
        QByteArray b = good;
        b[5] = 0x01;
        verifyRejected(parseCombinationDump(b), b, "reserved byte");
        b = good;
        b[6] = 0x03;  // bank D does not exist for combinations
        verifyRejected(parseCombinationDump(b), b, "combination bank 3");
        // Truncating the data by one byte leaves a header-only group.
        b = H({0xF0, 0x42, 0x3F, 0x7A, 0x4D, 0x00, 0x02, 0x07, 0x00, 0xF7});
        verifyRejected(parseCombinationDump(b), b, "header-only group");
    }
    // MIDI.md section 7 (IMP *6, *9): kind 1 = one bank = exactly 128 records,
    // kind 2 = exactly one record. Bank E (4) is microX only: rejected in an X50 program dump.
    void dumpKindImpliesRecordCount() {
        auto frame = [](int function, int kb, const QByteArray &data) {
            QByteArray m = H({0xF0, 0x42, 0x30, 0x7A, function, 0x00, kb, 0x00});
            m.append(encode78(data));
            m.append(static_cast<char>(0xF7));
            return m;
        };
        const QByteArray bank = syntheticProgramData();  // 128 x 444
        QVERIFY(parseProgramDump(frame(0x4C, 0x10, bank), 1).ok);
        const QByteArray shortBank = bank.left(444 * 127);  // record aligned, one short
        const QByteArray m1 = frame(0x4C, 0x10, shortBank);
        verifyRejected(parseProgramDump(m1), m1, "127 records in a bank dump");
        QVERIFY(parseProgramDump(frame(0x4C, 0x20, bank.left(444)), 1).ok);
        const QByteArray m2 = frame(0x4C, 0x20, bank.left(444 * 2));
        verifyRejected(parseProgramDump(m2), m2, "2 records in a single-record dump");
        const QByteArray cbank = syntheticCombiData();  // 128 x 544
        QVERIFY(parseCombinationDump(frame(0x4D, 0x10, cbank), 1).ok);
        const QByteArray m3 = frame(0x4D, 0x10, cbank.left(544 * 127));
        verifyRejected(parseCombinationDump(m3), m3, "127 combinations in a bank dump");
        QVERIFY(parseCombinationDump(frame(0x4D, 0x20, cbank.left(544)), 1).ok);
        const QByteArray m4 = frame(0x4D, 0x20, cbank.left(543));
        verifyRejected(parseCombinationDump(m4), m4, "partial single combination");
        // Bank D accepted, bank E rejected, for programs.
        QVERIFY(parseProgramDump(frame(0x4C, 0x13, bank), 1).ok);
        const QByteArray e = frame(0x4C, 0x14, bank);
        verifyRejected(parseProgramDump(e), e, "program bank E on an X50");
    }
    // MIDI.md section 7: 51 00 data F7; data >= 840 bytes (TABLE 4).
    void globalDumpParse() {
        const QByteArray data = syntheticGlobalData();
        QByteArray good = H({0xF0, 0x42, 0x30, 0x7A, 0x51, 0x00});
        good.append(encode78(data));
        good.append(static_cast<char>(0xF7));
        const auto r = parseGlobalDump(good, 1);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.data, data);
        standardNegatives(good, 1, 4, 2, 3, false,
                          [](const QByteArray &m, std::optional<int> c) { return parseGlobalDump(m, c); });
        QByteArray b = H({0xF0, 0x42, 0x30, 0x7A, 0x51, 0x00});
        b.append(encode78(data.left(839)));
        b.append(static_cast<char>(0xF7));
        verifyRejected(parseGlobalDump(b), b, "too short");
        b = good;
        b[5] = 0x01;
        verifyRejected(parseGlobalDump(b), b, "reserved byte");

        const auto cats = parseGlobalCategories(r.value.data);
        QVERIFY(cats.ok);
        QCOMPARE(cats.value.programCategories.size(), 16);
        QCOMPARE(cats.value.combinationCategories.size(), 16);
        QCOMPARE(cats.value.programCategories.at(0).text, QStringLiteral("PCat0"));
        QCOMPARE(cats.value.programCategories.at(15).text, QStringLiteral("PCat15"));
        QCOMPARE(cats.value.combinationCategories.at(0).text, QStringLiteral("CCat0"));
        QCOMPARE(cats.value.combinationCategories.at(15).text, QStringLiteral("CCat15"));
        QVERIFY(!parseGlobalCategories(data.left(839)).ok);
    }

    // ------------------------------------------------------------- codec

    // MIDI.md section 7 "Data encoding" (IMP *5): 7 bytes -> top-bit byte + 7.
    void encodeKnownVectors() {
        // 7 bytes, bytes 1 and 7 have bit 7 set: top = bit0 + bit6 = 41.
        QCOMPARE(encode78(H({0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF})),
                 H({0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F}));
        // 3 internal bytes -> 4 MIDI bytes; byte 3 high bit -> bit 2.
        QCOMPARE(encode78(H({0x01, 0x02, 0x83})), H({0x04, 0x01, 0x02, 0x03}));
        // 8 internal bytes -> 8 + 2; second group has top bits 01.
        QCOMPARE(encode78(H({1, 2, 3, 4, 5, 6, 7, 0x88})), H({0x00, 1, 2, 3, 4, 5, 6, 7, 0x01, 0x08}));
        QCOMPARE(encode78(QByteArray()), QByteArray());
        QCOMPARE(decode78(H({0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F})).value,
                 H({0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}));
        QCOMPARE(decode78(H({0x00, 1, 2, 3, 4, 5, 6, 7, 0x01, 0x08})).value, H({1, 2, 3, 4, 5, 6, 7, 0x88}));
    }
    void roundTripEveryLength() {
        for (int len = 0; len <= 7 * 20; ++len) {
            QByteArray in;
            for (int i = 0; i < len; ++i)
                in.append(static_cast<char>((i * 37 + 0x85) & 0xFF));  // mixes high and low bits
            const QByteArray enc = encode78(in);
            const int tail = len % 7;
            QCOMPARE(enc.size(), len / 7 * 8 + (tail ? tail + 1 : 0));
            for (char c : enc)
                QVERIFY((c & 0x80) == 0);
            const auto dec = decode78(enc);
            QVERIFY2(dec.ok, qPrintable(dec.error));
            QCOMPARE(dec.value, in);
        }
        // All-high-bit and all-zero patterns at every length 1-7.
        for (int len = 1; len <= 7; ++len) {
            QCOMPARE(decode78(encode78(QByteArray(len, '\xFF'))).value, QByteArray(len, '\xFF'));
            QCOMPARE(decode78(encode78(QByteArray(len, '\0'))).value, QByteArray(len, '\0'));
        }
    }
    void roundTripRandom() {
        std::mt19937 rng(0x58503530u);  // fixed seed
        for (int n = 0; n < 200; ++n) {
            const int len = static_cast<int>(rng() % 2000);
            QByteArray in;
            for (int i = 0; i < len; ++i)
                in.append(static_cast<char>(rng() & 0xFF));
            const auto dec = decode78(encode78(in));
            QVERIFY(dec.ok);
            QCOMPARE(dec.value, in);
        }
    }
    void decodeRejects() {
        const QByteArray bad = H({0x00, 0x01, 0x80, 0x03});
        verifyRejected(decode78(bad), bad, "bit 7 set");
        const QByteArray lone = H({0x00, 1, 2, 3, 4, 5, 6, 7, 0x01});  // final group: header only
        verifyRejected(decode78(lone), lone, "lone header byte");
        const QByteArray lone2 = H({0x00});
        verifyRejected(decode78(lone2), lone2, "lone header byte alone");
        // Final group of 2 data bytes cannot have header bit 2 set.
        const QByteArray stray = H({0x04, 0x01, 0x02});
        verifyRejected(decode78(stray), stray, "header bit for a missing byte");
        QVERIFY(decode78(QByteArray()).ok);
    }

    // ----------------------------------------------------------- records

    // MIDI.md section 7 table: program 444 bytes, name 0-15, category byte 110.
    void programRecords() {
        const QByteArray d = syntheticProgramData();
        QCOMPARE(d.size(), 444 * 128);
        const auto r = sliceProgramRecords(d);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.size(), 128);
        QCOMPARE(r.value.at(0).name.text, QStringLiteral("Prog 000"));  // trailing spaces trimmed
        QCOMPARE(r.value.at(0).name.raw.size(), 16);
        QCOMPARE(r.value.at(127).name.text, QStringLiteral("Prog 127"));
        QCOMPARE(r.value.at(17).category, 1);
        QCOMPARE(r.value.at(15).category, 15);
        QVERIFY(r.value.at(15).categoryValid);
        QVERIFY(!r.value.at(5).name.nonPrintable);

        verifyRejected(sliceProgramRecords(d.left(444 * 128 - 1)), d.left(64), "short by one");
        QByteArray plus = d;
        plus.append('\0');
        verifyRejected(sliceProgramRecords(plus), plus.left(64), "long by one");
        QVERIFY(!sliceProgramRecords(QByteArray()).ok);
        QVERIFY(sliceProgramRecords(d.left(444)).ok);  // a single-program dump
    }
    // MIDI.md section 7 table: combination 544 bytes, name 0-15, category byte 118 bits 0-3.
    void combinationRecords() {
        const QByteArray d = syntheticCombiData();
        QCOMPARE(d.size(), 544 * 128);
        const auto r = sliceCombinationRecords(d);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.value.size(), 128);
        QCOMPARE(r.value.at(3).name.text, QStringLiteral("Combi 003"));
        QCOMPARE(r.value.at(127).name.text, QStringLiteral("Combi 127"));
        QCOMPARE(r.value.at(19).category, 3);  // 0xF3 & 0x0F
        QCOMPARE(r.value.at(127).category, 15);
        verifyRejected(sliceCombinationRecords(d.left(544 * 128 - 1)), d.left(64), "short by one");
        QByteArray plus = d;
        plus.append('\0');
        verifyRejected(sliceCombinationRecords(plus), plus.left(64), "long by one");
        // A program-sized buffer is not a combination bank.
        QVERIFY(!sliceCombinationRecords(syntheticProgramData()).ok);
    }
    void nameDecoding() {
        // Printable range is 0x20-0x7F (MIDI.md section 7).
        NameField f = decodeName(QByteArray("Warm Pad        ", 16));
        QCOMPARE(f.text, QStringLiteral("Warm Pad"));
        QVERIFY(!f.nonPrintable);
        f = decodeName(QByteArray(16, ' '));
        QCOMPARE(f.text, QString());
        // Control byte and high byte are reported, not dropped; 0x7F is in range.
        QByteArray raw = QByteArray("AB              ", 16);
        raw[2] = 0x01;
        raw[3] = static_cast<char>(0xC3);
        raw[4] = 0x7F;
        f = decodeName(raw);
        QVERIFY(f.nonPrintable);
        QCOMPARE(f.raw, raw);
        QCOMPARE(f.text.size(), 5);
        QCOMPARE(f.text.at(2), QChar(QChar::ReplacementCharacter));
        QCOMPARE(f.text.at(3), QChar(QChar::ReplacementCharacter));
        QCOMPARE(f.text.at(4), QChar(0x7F));
        // A non-printable in the trailing region is not hidden by trimming.
        raw = QByteArray(16, ' ');
        raw[15] = 0x00;
        QVERIFY(decodeName(raw).nonPrintable);
        // Program category above 15 is flagged.
        QByteArray rec = syntheticProgramData().left(444);
        rec[110] = 16;
        const auto r = sliceProgramRecords(rec);
        QVERIFY(r.ok);
        QVERIFY(!r.value.at(0).categoryValid);
        QCOMPARE(r.value.at(0).category, 16);
    }

    // Whole chain: 4C dump of synthetic memory -> parse -> slice (MIDI.md section 7).
    void dumpToRecordsEndToEnd() {
        QByteArray msg = H({0xF0, 0x42, 0x30, 0x7A, 0x4C, 0x00, 0x10, 0x00});
        msg.append(encode78(syntheticProgramData()));
        msg.append(static_cast<char>(0xF7));
        const auto d = parseProgramDump(msg, 1);
        QVERIFY2(d.ok, qPrintable(d.error));
        QCOMPARE(d.value.bank, 0);
        QCOMPARE(d.value.kind, 1);
        const auto recs = sliceProgramRecords(d.value.data);
        QVERIFY(recs.ok);
        QCOMPARE(recs.value.size(), 128);
        QCOMPARE(recs.value.at(42).name.text, QStringLiteral("Prog 042"));
    }

    // MIDI.md sections 4-8, header-only classification.
    void classifyRoutes() {
        QCOMPARE(classify(H({0xF0, 0x7E, 0x02, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x01, 0x00, 1, 5, 0, 0, 0xF7})).kind,
                 MessageKind::DeviceInquiryReply);
        QCOMPARE(classify(H({0xF0, 0x7E, 0x02, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x01, 0x00, 1, 5, 0, 0, 0xF7})).channel, 3);
        QCOMPARE(classify(H({0xF0, 0x42, 0x30, 0x7A, 0x23, 0xF7})).kind, MessageKind::Ack);
        QCOMPARE(classify(H({0xF0, 0x42, 0x30, 0x7A, 0x24, 0x02, 0xF7})).kind, MessageKind::Nak);
        QCOMPARE(classify(H({0xF0, 0x42, 0x3F, 0x7A, 0x4E, 0x02, 0xF7})).kind, MessageKind::ModeChange);
        QCOMPARE(classify(H({0xF0, 0x42, 0x3F, 0x7A, 0x4E, 0x02, 0xF7})).channel, 16);
        QCOMPARE(classify(H({0xF0, 0x42, 0x30, 0x7A, 0x42, 0x02, 0xF7})).kind, MessageKind::ModeAndState);
        QCOMPARE(classify(H({0xF0, 0x42, 0x30, 0x7A, 0x4C, 0x00, 0x10, 0x00, 0xF7})).kind, MessageKind::ProgramDump);
        QCOMPARE(classify(H({0xF0, 0x42, 0x30, 0x7A, 0x4D, 0x00, 0x10, 0x00, 0xF7})).kind, MessageKind::CombinationDump);
        QCOMPARE(classify(H({0xF0, 0x42, 0x30, 0x7A, 0x51, 0x00, 0xF7})).kind, MessageKind::GlobalDump);
        QCOMPARE(classify(H({0xF0, 0x42, 0x30, 0x7A, 0x40, 0x00, 0xF7})).kind, MessageKind::OtherKorgSysEx);
        QCOMPARE(classify(H({0xB0, 0x00, 0x3F})).kind, MessageKind::ControlChange);
        QCOMPARE(classify(H({0xC4, 0x05})).channel, 5);
        QCOMPARE(classify(H({0xF8})).kind, MessageKind::Other);
        QCOMPARE(classify(QByteArray()).kind, MessageKind::Other);
        QCOMPARE(classify(H({0xF0, 0x41, 0x10, 0x42, 0x12, 0xF7})).kind, MessageKind::Other);
    }

    // MIDI.md section 8 / 3: Bn 00 mm, Bn 20 bb, Cn pp.
    void channelMessageParse() {
        const auto cc = parseChannelMessage(H({0xB2, 0x20, 0x01}), 3);
        QVERIFY(cc.ok);
        QVERIFY(!cc.value.isProgramChange);
        QCOMPARE(cc.value.channel, 3);
        QCOMPARE(cc.value.controller, 0x20);
        QCOMPARE(cc.value.value, 1);
        const auto pc = parseChannelMessage(H({0xC0, 0x7F}));
        QVERIFY(pc.ok);
        QVERIFY(pc.value.isProgramChange);
        QCOMPARE(pc.value.value, 127);
        const QByteArray bad = H({0xB0, 0x00, 0x80});
        verifyRejected(parseChannelMessage(bad), bad, "classify rejects (status in data)");
        const QByteArray other = H({0xB1, 0x00, 0x3F});
        verifyRejected(parseChannelMessage(other, 1), other, "wrong channel");
        const QByteArray sx = H({0xF0, 0x42, 0x30, 0x7A, 0x23, 0xF7});
        verifyRejected(parseChannelMessage(sx), sx, "SysEx is not a channel message");
    }

    // Reverse of the MIDI.md section 3 table.
    void addressFromBankSelectTable() {
        using K = SoundAddress::Kind;
        using B = SoundAddress::Bank;
        QCOMPARE(*addressFromBankSelect(0x00, 0x01, 5, BankMap::Korg, K::Prog), A(K::Prog, B::B, 5));
        QCOMPARE(*addressFromBankSelect(0x3F, 0x02, 127, BankMap::Gm, K::Combi), A(K::Combi, B::C, 127));
        QVERIFY(!addressFromBankSelect(0x3F, 0x02, 127, BankMap::Korg, K::Combi));  // wrong map
        QCOMPARE(*addressFromBankSelect(0x79, 0x00, 0, BankMap::Gm, K::Prog), A(K::Prog, B::G, 1));
        QCOMPARE(*addressFromBankSelect(0x78, 0x00, 127, BankMap::Korg, K::Prog), A(K::Prog, B::Gd, 128));
        QVERIFY(!addressFromBankSelect(0x00, 0x03, 0, BankMap::Korg, K::Combi));  // no combination bank D
        QVERIFY(!addressFromBankSelect(0x00, 0x00, 128, BankMap::Korg, K::Prog));
    }

    // MIDI.md section 6: F0 7E 0g 06 02 42 7A 00 mm 00 vv nn rr bb F7.
    void buildInquiryReplyBytes() {
        DeviceInquiryReply r;
        r.channel = 2; r.memberCode = 0x01; r.major = 1; r.minor = 5; r.release = 0; r.build = 3;
        const QByteArray b = buildDeviceInquiryReply(r);
        QCOMPARE(b, H({0xF0, 0x7E, 0x01, 0x06, 0x02, 0x42, 0x7A, 0x00, 0x01, 0x00, 0x01, 0x05, 0x00, 0x03, 0xF7}));
        QVERIFY(parseDeviceInquiryReply(b, 2).ok);
    }

    // MIDI.md section 5: 27 bytes. Mode PROG PLAY (02), GM (bit 4 of byte 7),
    // byte 8 = 07 (three enables), program B005, combination C007.
    void buildModeAndStateBytes() {
        ModeAndState s;
        s.channel = 1; s.mode = Mode::ProgPlay; s.bankMap = BankMap::Gm;
        s.enableProgramChange = s.enableBankChange = s.enableCombinationChange = true;
        s.programBank = 1; s.programNumber = 5; s.combinationBank = 2; s.combinationNumber = 7;
        const auto b = buildModeAndState(s);
        QVERIFY(b.ok);
        QCOMPARE(b.value, H({0xF0, 0x42, 0x30, 0x7A, 0x42, 0x02, 0x06, 0x10, 0x07, 0x00, 0x02, 0x01, 0x05, 0x00,
                             0x00, 0x02, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0xF7}));
        const auto p = parseModeAndState(b.value, 1);
        QVERIFY2(p.ok, qPrintable(p.error));
        QCOMPARE(p.value.programNumber, 5);
        QVERIFY(!buildModeAndState([&] { ModeAndState x = s; x.channel = 0; return x; }()).ok);
    }

    // MIDI.md sections 3-4: ACK F0 42 3g 7A 23 F7, NAK ... 24 cc F7, panel 4E 0m.
    void buildAckNakNotice() {
        QCOMPARE(buildAck(1).value, H({0xF0, 0x42, 0x30, 0x7A, 0x23, 0xF7}));
        QCOMPARE(buildNak(16, 0x02).value, H({0xF0, 0x42, 0x3F, 0x7A, 0x24, 0x02, 0xF7}));
        QVERIFY(!buildAck(0).ok);
        QVERIFY(!buildNak(1, 0x80).ok);
        QCOMPARE(buildModeChangeNotice(1, Mode::Multi).value, H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x04, 0xF7}));
        QCOMPARE(buildModeChangeNotice(1, Mode::Global).value, H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x06, 0xF7}));
        QVERIFY(!buildModeChangeNotice(1, Mode::Demo).ok);
        const auto sel = buildPanelSelect(A(A::Kind::Combi, A::Bank::B, 9), BankMap::Gm, 2);
        QCOMPARE(sel.value, (QList<QByteArray>{H({0xB1, 0x00, 0x3F}), H({0xB1, 0x20, 0x01}), H({0xC1, 0x09})}));
    }

    // MIDI.md section 7 dump replies: 4C 00 kind/bank 00 data F7; 51 00 data F7.
    // 3 internal bytes 80 01 FF encode to 05 00 01 7F (see programDumpParse).
    void buildDumpBytes() {
        const QByteArray data = H({0x80, 0x01, 0xFF});
        QCOMPARE(buildProgramDump(1, 3, data).value,
                 H({0xF0, 0x42, 0x30, 0x7A, 0x4C, 0x00, 0x13, 0x00, 0x05, 0x00, 0x01, 0x7F, 0xF7}));
        QCOMPARE(buildCombinationDump(2, 2, data).value,
                 H({0xF0, 0x42, 0x31, 0x7A, 0x4D, 0x00, 0x12, 0x00, 0x05, 0x00, 0x01, 0x7F, 0xF7}));
        QCOMPARE(buildGlobalDump(1, data).value,
                 H({0xF0, 0x42, 0x30, 0x7A, 0x51, 0x00, 0x05, 0x00, 0x01, 0x7F, 0xF7}));
        QVERIFY(!buildProgramDump(1, 4, data).ok);
        QVERIFY(!buildCombinationDump(1, 3, data).ok);
        QVERIFY(!buildGlobalDump(17, data).ok);
    }
};

QTEST_APPLESS_MAIN(TestX50Protocol)
#include "test_x50_protocol.moc"
