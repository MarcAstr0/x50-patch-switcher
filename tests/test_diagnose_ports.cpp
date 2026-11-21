// R1: the output/input pairs --diagnose tries when no --port is given. RtMidi 6.0.0's WinMM
// backend appends " <portIndex>" to every port name (RtMidi.cpp getPortName), so an output and
// the input of one device never have the same name; the trailing number is ignored.
#include <QTest>

#include "app/DiagnoseRunner.h"

using Pair = DiagnoseRunner::PortPair;

class TestDiagnosePorts : public QObject {
    Q_OBJECT

    static QStringList L(std::initializer_list<const char *> names) {
        QStringList l;
        for (const char *n : names)
            l << QString::fromLatin1(n);
        return l;
    }
    static QStringList outs(const QVector<Pair> &p) {
        QStringList l;
        for (const Pair &x : p)
            l << x.out + QStringLiteral(" -> ") + x.in;
        return l;
    }

private slots:
    // The GS Wavetable Synth is skipped; the USB device's output "1" meets its input "0".
    void indexSuffixedNamesPair() {
        const auto p = DiagnoseRunner::planPortPairs(L({"Microsoft GS Wavetable Synth 0", "USB MIDI Device 1"}),
                                                     L({"USB MIDI Device 0"}));
        QCOMPARE(outs(p), (QStringList{QStringLiteral("USB MIDI Device 1 -> USB MIDI Device 0")}));
    }

    void x50NamesComeFirstThenSameNameThenTheRest() {
        bool single = true;
        const auto p = DiagnoseRunner::planPortPairs(L({"Other Synth 0", "USB MIDI Device 1", "KORG X50 2"}),
                                                     L({"USB MIDI Device 0", "Thing 1", "KORG X50 1"}), &single);
        QVERIFY(single);  // exactly one pair has X50 in both names: it is the only one tried
        QCOMPARE(outs(p), (QStringList{QStringLiteral("KORG X50 2 -> KORG X50 1")}));
    }

    void twoX50PairsAreBothTriedInOrder() {
        bool single = true;
        const auto p = DiagnoseRunner::planPortPairs(L({"Other 0", "X50 PORT 1", "X50 PORT 2"}),
                                                     L({"USB MIDI Device 0", "X50 PORT 1", "X50 PORT 2"}), &single);
        QVERIFY(!single);
        const QStringList o = outs(p);
        QCOMPARE(o.at(0), QStringLiteral("X50 PORT 1 -> X50 PORT 1"));
        QCOMPARE(o.at(1), QStringLiteral("X50 PORT 1 -> X50 PORT 2"));
        QCOMPARE(o.at(2), QStringLiteral("X50 PORT 2 -> X50 PORT 1"));
        QCOMPARE(o.at(3), QStringLiteral("X50 PORT 2 -> X50 PORT 2"));
        // The rest follow, including the unrelated output.
        QVERIFY(o.contains(QStringLiteral("Other 0 -> USB MIDI Device 0")));
        QCOMPARE(p.at(0).why, QStringLiteral("both names contain X50"));
    }

    void sameNameBeforeTheRest() {
        const auto p = DiagnoseRunner::planPortPairs(L({"Alpha 0", "USB MIDI Device 1"}),
                                                     L({"Alpha 1", "USB MIDI Device 0"}));
        const QStringList o = outs(p);
        QCOMPARE(o.size(), 4);
        QCOMPARE(o.at(0), QStringLiteral("Alpha 0 -> Alpha 1"));
        QCOMPARE(o.at(1), QStringLiteral("USB MIDI Device 1 -> USB MIDI Device 0"));
        QCOMPARE(p.at(0).why, QStringLiteral("output and input have the same name"));
        QCOMPARE(p.at(2).why, QStringLiteral("any other pair"));
    }

    // Two identical devices: the pair of each device comes before the crossed ones, and the
    // full names (with their numbers) are kept so each attempt opens its own ports.
    void twoIdenticalDevices() {
        const auto p = DiagnoseRunner::planPortPairs(L({"USB MIDI Device 0", "USB MIDI Device 1"}),
                                                     L({"USB MIDI Device 2", "USB MIDI Device 3"}));
        const QStringList o = outs(p);
        QCOMPARE(o.size(), 4);  // all the same stripped name: all are tier 2, in list order
        QCOMPARE(o.at(0), QStringLiteral("USB MIDI Device 0 -> USB MIDI Device 2"));
        QCOMPARE(o.at(3), QStringLiteral("USB MIDI Device 1 -> USB MIDI Device 3"));
        QCOMPARE(QSet<QString>(o.begin(), o.end()).size(), 4);  // no duplicates
    }

    void microsoftSynthOutputIsAlwaysSkippedAndNothingPairsToNothing() {
        QVERIFY(DiagnoseRunner::planPortPairs(L({"Microsoft GS Wavetable Synth 0"}), L({"USB MIDI Device 0"})).isEmpty());
        QVERIFY(DiagnoseRunner::planPortPairs(QStringList(), L({"USB MIDI Device 0"})).isEmpty());
        QVERIFY(DiagnoseRunner::planPortPairs(L({"USB MIDI Device 1"}), QStringList()).isEmpty());
    }

    // 4x4 with no X50 name and no equal stripped names: the right pair shares the most words but
    // is last in plain cross-product order.
    void thirdTierOrderedBySharedWords() {
        const auto p = DiagnoseRunner::planPortPairs(
            L({"Alpha Box 0", "Beta Box 1", "Gamma Box 2", "Delta Pad Mk2 3"}),
            L({"One Two 0", "Three Four 1", "Five Six 2", "Delta Pad 3"}));
        QVERIFY(p.size() == 16);
        QCOMPARE(outs(p).at(0), QStringLiteral("Delta Pad Mk2 3 -> Delta Pad 3"));
        // Ties keep plain order: Alpha Box 0 -> One Two 0 comes first among the zero-share pairs.
        QCOMPARE(outs(p).at(1), QStringLiteral("Alpha Box 0 -> One Two 0"));
    }

    void attemptsAreCapped() {
        QStringList o, i;
        for (int n = 0; n < 6; ++n) {
            o << QStringLiteral("Out %1").arg(n);
            i << QStringLiteral("In %1").arg(n);
        }
        QCOMPARE(DiagnoseRunner::planPortPairs(o, i).size(), DiagnoseRunner::kMaxAttempts);
    }
};

QTEST_GUILESS_MAIN(TestDiagnosePorts)
#include "test_diagnose_ports.moc"
