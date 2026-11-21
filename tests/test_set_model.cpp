// T1.5: set operations. No MIDI bytes involved.
#include <QTest>

#include "core/SetModel.h"

namespace {
SoundAddress combi(int n) { return SoundAddress(SoundAddress::Kind::Combi, SoundAddress::Bank::A, n); }
SoundAddress prog(int n) { return SoundAddress(SoundAddress::Kind::Prog, SoundAddress::Bank::B, n); }
}

class TestSetModel : public QObject {
    Q_OBJECT
private slots:
    void construct() {
        SetModel s(QStringLiteral("id1"), QStringLiteral("Gig"), 8);
        QCOMPARE(s.size(), 8);
        QCOMPARE(s.name(), QStringLiteral("Gig"));
        for (const auto &sl : s.allSlots())
            QVERIFY(sl.isEmpty());
    }
    void rename() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 6);
        QVERIFY(s.rename(QStringLiteral("  Encore ")).ok());
        QCOMPARE(s.name(), QStringLiteral("Encore"));
        const auto r = s.rename(QStringLiteral("   "));
        QVERIFY(!r.ok());
        QVERIFY(!r.error.isEmpty());
        QCOMPARE(s.name(), QStringLiteral("Encore"));
    }
    void setClearLabel() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 6);
        QVERIFY(s.setSlot(2, combi(5)).ok());
        QCOMPARE(*s.slot(2).address, combi(5));
        QVERIFY(s.setLabel(2, QStringLiteral(" Song ")).ok());
        QCOMPARE(s.slot(2).label, QStringLiteral("Song"));
        // Replacing the sound keeps the label.
        QVERIFY(s.setSlot(2, prog(9)).ok());
        QCOMPARE(s.slot(2).label, QStringLiteral("Song"));
        QVERIFY(s.setLabel(2, QString()).ok());
        QVERIFY(s.slot(2).label.isEmpty());
        QVERIFY(s.setLabel(2, QStringLiteral("x")).ok());
        QVERIFY(s.clearSlot(2).ok());
        QVERIFY(s.slot(2).isEmpty());
        QVERIFY(s.slot(2).label.isEmpty());
    }
    void labelOnEmptySlotRefused() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 6);
        const auto r = s.setLabel(0, QStringLiteral("x"));
        QVERIFY(!r.ok());
        QVERIFY(r.error.contains(QStringLiteral("Slot 1")));
    }
    void rangeAndValidity() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 6);
        QVERIFY(!s.setSlot(6, combi(0)).ok());
        QVERIFY(!s.setSlot(-1, combi(0)).ok());
        QVERIFY(!s.setSlot(0, SoundAddress()).ok());
        QVERIFY(!s.setSlot(0, combi(128)).ok());
        QVERIFY(!s.clearSlot(6).ok());
        QVERIFY(!s.setLabel(6, QStringLiteral("x")).ok());
        QVERIFY(!s.moveSlot(0, 6).ok());
        QVERIFY(!s.moveSlot(-1, 0).ok());
        QVERIFY(s.slot(0).isEmpty());
    }
    void moveToEmpty() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 6);
        s.setSlot(0, combi(1));
        s.setLabel(0, QStringLiteral("L"));
        QVERIFY(s.moveSlot(0, 4).ok());
        QVERIFY(s.slot(0).isEmpty());
        QCOMPARE(*s.slot(4).address, combi(1));
        QCOMPARE(s.slot(4).label, QStringLiteral("L"));
    }
    void moveSwaps() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 6);
        s.setSlot(0, combi(1));
        s.setLabel(0, QStringLiteral("L"));
        s.setSlot(1, combi(2));
        QVERIFY(s.moveSlot(0, 1).ok());
        QCOMPARE(*s.slot(0).address, combi(2));
        QVERIFY(s.slot(0).label.isEmpty());
        QCOMPARE(*s.slot(1).address, combi(1));
        QCOMPARE(s.slot(1).label, QStringLiteral("L"));
        QVERIFY(s.moveSlot(1, 1).ok());
    }
    void resizeUp() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 6);
        s.setSlot(5, combi(1));
        QVERIFY(s.resize(8).ok());
        QCOMPARE(s.size(), 8);
        QCOMPARE(*s.slot(5).address, combi(1));
        QVERIFY(s.slot(6).isEmpty() && s.slot(7).isEmpty());
    }
    void resizeDownEmpty() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 8);
        s.setSlot(0, combi(1));
        s.setSlot(5, combi(2));
        QVERIFY(s.resize(6).ok());
        QCOMPARE(s.size(), 6);
        QCOMPARE(*s.slot(0).address, combi(1));
        QCOMPARE(*s.slot(5).address, combi(2));
    }
    void resizeDownRefusedNamesSlots() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 8);
        s.setSlot(6, combi(1));
        s.setSlot(7, combi(2));
        auto r = s.resize(6);
        QVERIFY(!r.ok());
        QVERIFY2(r.error.contains(QStringLiteral("7 and 8")), qPrintable(r.error));
        QCOMPARE(s.size(), 8);
        QCOMPARE(*s.slot(6).address, combi(1));
        s.clearSlot(6);
        r = s.resize(6);
        QVERIFY(!r.ok());
        QVERIFY2(r.error.contains(QStringLiteral("slot 8 is")), qPrintable(r.error));
        QCOMPARE(s.size(), 8);
    }
    void resizeBadSizeAndSame() {
        SetModel s(QStringLiteral("i"), QStringLiteral("A"), 6);
        QVERIFY(!s.resize(7).ok());
        QVERIFY(!s.resize(0).ok());
        QVERIFY(s.resize(6).ok());
        QCOMPARE(s.size(), 6);
    }
};

QTEST_GUILESS_MAIN(TestSetModel)
#include "test_set_model.moc"
