// R-T1.7 item 1: SysExAssembler joins SysEx that WinMM delivers buffer by buffer.
// Bytes: SysEx framing F0 ... F7 and channel messages as in docs/MIDI.md sections 3-4
// (Mode Change F0 42 3g 7A 4E 0m F7, bank select B0 00 xx).
#include <initializer_list>

#include <QTest>

#include "midi/SysExAssembler.h"

namespace {
QByteArray H(std::initializer_list<int> list) {
    QByteArray b;
    for (int v : list)
        b.append(static_cast<char>(v));
    return b;
}
const QByteArray kMode = H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x02, 0xF7});
}  // namespace

class TestSysExAssembler : public QObject {
    Q_OBJECT
private slots:
    void wholeMessageIsPassedThrough() {
        SysExAssembler a;
        QCOMPARE(a.feed(kMode), QList<QByteArray>{kMode});
        QVERIFY(!a.assembling());
    }

    void shortMessagesAreEmittedAlone() {
        SysExAssembler a;
        QCOMPARE(a.feed(H({0xB0, 0x00, 0x3F})), QList<QByteArray>{H({0xB0, 0x00, 0x3F})});
        QCOMPARE(a.feed(H({0xC0, 0x05})), QList<QByteArray>{H({0xC0, 0x05})});
    }

    void fragmentsAreJoined() {
        SysExAssembler a;
        QVERIFY(a.feed(H({0xF0, 0x42, 0x30})).isEmpty());
        QVERIFY(a.assembling());
        QVERIFY(a.feed(H({0x7A, 0x4E})).isEmpty());
        QCOMPARE(a.feed(H({0x02, 0xF7})), QList<QByteArray>{kMode});
        QVERIFY(!a.assembling());
    }

    void loneEndByteCompletes() {
        SysExAssembler a;
        QVERIFY(a.feed(H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x02})).isEmpty());
        QCOMPARE(a.feed(H({0xF7})), QList<QByteArray>{kMode});
    }

    void shortMessageMidSysExIsEmittedAndSysExContinues() {
        SysExAssembler a;
        QVERIFY(a.feed(H({0xF0, 0x42, 0x30, 0x7A})).isEmpty());
        QCOMPARE(a.feed(H({0xB0, 0x00, 0x3F})), QList<QByteArray>{H({0xB0, 0x00, 0x3F})});
        QVERIFY(a.assembling());
        QCOMPARE(a.feed(H({0x4E, 0x02, 0xF7})), QList<QByteArray>{kMode});
    }

    void newStartDiscardsThePartial() {
        SysExAssembler a;
        QStringList problems;
        QVERIFY(a.feed(H({0xF0, 0x42, 0x30}), &problems).isEmpty());
        QCOMPARE(a.feed(kMode, &problems), QList<QByteArray>{kMode});
        QCOMPARE(problems.size(), 1);
        QVERIFY(!a.assembling());
        // A new start that is itself a fragment.
        QVERIFY(a.feed(H({0xF0, 0x01}), &problems).isEmpty());
        QVERIFY(a.feed(H({0xF0, 0x42, 0x30}), &problems).isEmpty());
        QCOMPARE(problems.size(), 2);
        QCOMPARE(a.pendingSize(), 3);
    }

    void strayContinuationIsDropped() {
        SysExAssembler a;
        QStringList problems;
        QVERIFY(a.feed(H({0x01, 0x02, 0xF7}), &problems).isEmpty());
        QCOMPARE(problems.size(), 1);
    }

    void runawayDataIsDropped() {
        SysExAssembler a;
        QStringList problems;
        QByteArray chunk(16384, '\x01');
        QVERIFY(a.feed(H({0xF0}) + chunk, &problems).isEmpty());
        for (int i = 0; i < 20; ++i)
            QVERIFY(a.feed(chunk, &problems).isEmpty());
        QVERIFY(!problems.isEmpty());
        QVERIFY(!a.assembling());
        // The assembler recovers.
        QCOMPARE(a.feed(kMode), QList<QByteArray>{kMode});
    }

    void largeDumpInManyFragments() {
        // About the size of the largest dump (~80 KB) in 1024-byte pieces.
        QByteArray msg = H({0xF0, 0x42, 0x30, 0x7A, 0x4D});
        msg.append(QByteArray(80000, '\x11'));
        msg.append('\xF7');
        SysExAssembler a;
        QList<QByteArray> out;
        for (int pos = 0; pos < msg.size(); pos += 1024)
            out << a.feed(msg.mid(pos, 1024));
        QCOMPARE(out.size(), 1);
        QCOMPARE(out.at(0), msg);
    }
};

QTEST_MAIN(TestSysExAssembler)
#include "test_sysex_assembler.moc"
