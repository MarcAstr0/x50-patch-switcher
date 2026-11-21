// T1.3: SoundAddress. Grammar from SPEC 3.1; bytes from docs/MIDI.md section 3
// ("Bank select values" table). Expected bytes below are written out by hand.
#include <QFile>
#include <QSet>
#include <QStringList>
#include <QTest>

#include "core/SoundAddress.h"

using A = SoundAddress;

class TestSoundAddress : public QObject {
    Q_OBJECT

    static A one(const QString &s) {
        const A::ParseResult r = A::parse(s);
        return r.matches.size() == 1 ? r.matches.first() : A();
    }

private slots:
    // SPEC 3.1: canonical form, all 1 033 catalogue addresses round trip.
    void roundTripCatalogue() {
        QFile f(QStringLiteral(X50_FACTORY_TSV));
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        int n = 0;
        QSet<QString> seen;
        while (!f.atEnd()) {
            const QStringList c = QString::fromUtf8(f.readLine()).split(QLatin1Char('\t'));
            if (c.size() < 3)
                continue;
            const QString canon = c[0] + QLatin1Char(' ') + c[1] + c[2];
            const A::ParseResult r = A::parse(canon);
            QVERIFY2(r.ok(), qPrintable(canon));
            QCOMPARE(r.matches.size(), 1);
            QCOMPARE(r.matches.first().format(), canon);
            QVERIFY(r.matches.first().isValid());
            QCOMPARE(A::parse(r.matches.first().format()).matches.first(), r.matches.first());
            QVERIFY2(!seen.contains(canon), qPrintable(canon));
            seen.insert(canon);
            ++n;
        }
        QCOMPARE(n, 1033);
    }

    // SPEC 3.1 grammar examples.
    void grammarForms_data() {
        QTest::addColumn<QString>("in");
        QTest::addColumn<int>("matches");
        QTest::addColumn<QString>("first");
        QTest::addColumn<QString>("second");
        QTest::newRow("A012") << "A012" << 2 << "COMBI A012" << "PROG A012";
        QTest::newRow("a12") << "a12" << 2 << "COMBI A012" << "PROG A012";
        QTest::newRow("PROG A012") << "PROG A012" << 1 << "PROG A012" << "";
        QTest::newRow("P A012") << "P A012" << 1 << "PROG A012" << "";
        QTest::newRow("COMBI B100") << "COMBI B100" << 1 << "COMBI B100" << "";
        QTest::newRow("C B100") << "C B100" << 1 << "COMBI B100" << "";
        QTest::newRow("cB100") << "cB100" << 1 << "COMBI B100" << "";
        QTest::newRow("G1") << "G1" << 1 << "PROG G001" << "";
        QTest::newRow("G001") << "G001" << 1 << "PROG G001" << "";
        QTest::newRow("gd1") << "gd1" << 1 << "PROG g(d)001" << "";
        QTest::newRow("g(d)001") << "g(d)001" << 1 << "PROG g(d)001" << "";
        QTest::newRow("D5 prog only") << "d5" << 1 << "PROG D005" << "";
        QTest::newRow("c12 bare bank C") << "c12" << 2 << "COMBI C012" << "PROG C012";
        QTest::newRow("padded") << "  prog   g(d) 009 " << 1 << "PROG g(d)009" << "";
        QTest::newRow("G128") << "G128" << 1 << "PROG G128" << "";
        QTest::newRow("A127") << "a127" << 2 << "COMBI A127" << "PROG A127";
    }
    void grammarForms() {
        QFETCH(QString, in);
        QFETCH(int, matches);
        QFETCH(QString, first);
        QFETCH(QString, second);
        const A::ParseResult r = A::parse(in);
        QCOMPARE(r.matches.size(), matches);
        QCOMPARE(r.matches.at(0).format(), first);
        if (matches == 2)
            QCOMPARE(r.matches.at(1).format(), second);
        QVERIFY(r.error.isEmpty());
    }

    // SPEC 3.1: out-of-range numbers are rejected, not clamped.
    void rejects_data() {
        QTest::addColumn<QString>("in");
        QTest::newRow("A128") << "A128";
        QTest::newRow("COMBI D000") << "COMBI D000";
        QTest::newRow("G000") << "G000";
        QTest::newRow("G129") << "G129";
        QTest::newRow("E000") << "E000";
        QTest::newRow("gd000") << "gd000";
        QTest::newRow("gd129") << "gd129";
        QTest::newRow("COMBI G001") << "COMBI G001";
        QTest::newRow("empty") << "";
        QTest::newRow("junk") << "hello";
        QTest::newRow("no number") << "A";
        QTest::newRow("four digits") << "A0001";
    }
    void rejects() {
        QFETCH(QString, in);
        const A::ParseResult r = A::parse(in);
        QVERIFY(!r.ok());
        QVERIFY(!r.error.isEmpty());
    }

    void invalidAddresses() {
        QVERIFY(!A().isValid() || A().number() >= 0);  // default: number -1
        QVERIFY(!A().isValid());
        QVERIFY(!A(A::Kind::Combi, A::Bank::D, 0).isValid());
        QVERIFY(!A(A::Kind::Prog, A::Bank::G, 0).isValid());
        QVERIFY(A(A::Kind::Prog, A::Bank::G, 128).isValid());
        QCOMPARE(A(A::Kind::Prog, A::Bank::A, 128).format(), QString());
    }

    void ordering() {
        QVERIFY(one(QStringLiteral("COMBI C127")) < one(QStringLiteral("PROG A000")));
        QVERIFY(one(QStringLiteral("PROG A127")) < one(QStringLiteral("PROG B000")));
        QVERIFY(one(QStringLiteral("PROG D127")) < one(QStringLiteral("PROG G001")));
        QVERIFY(one(QStringLiteral("PROG G128")) < one(QStringLiteral("PROG g(d)001")));
        QVERIFY(one(QStringLiteral("PROG A001")) < one(QStringLiteral("PROG A002")));
        QVERIFY(!(one(QStringLiteral("PROG A001")) < one(QStringLiteral("PROG A001"))));
    }

    // docs/MIDI.md section 3, "Bank select values" table, every row, both maps.
    void bankSelectTable_data() {
        QTest::addColumn<QString>("addr");
        QTest::addColumn<int>("korgMsb");
        QTest::addColumn<int>("korgLsb");
        QTest::addColumn<int>("gmMsb");
        QTest::addColumn<int>("gmLsb");
        QTest::addColumn<int>("pc");
        QTest::newRow("PROG A000") << "PROG A000" << 0x00 << 0x00 << 0x3F << 0x00 << 0;
        QTest::newRow("PROG B127") << "PROG B127" << 0x00 << 0x01 << 0x3F << 0x01 << 127;
        QTest::newRow("PROG C064") << "PROG C064" << 0x00 << 0x02 << 0x3F << 0x02 << 64;
        QTest::newRow("PROG D005") << "PROG D005" << 0x00 << 0x03 << 0x3F << 0x03 << 5;
        QTest::newRow("PROG G001") << "PROG G001" << 0x79 << 0x00 << 0x79 << 0x00 << 0;
        QTest::newRow("PROG G128") << "PROG G128" << 0x79 << 0x00 << 0x79 << 0x00 << 127;
        QTest::newRow("PROG g(d)001") << "PROG g(d)001" << 0x78 << 0x00 << 0x78 << 0x00 << 0;
        QTest::newRow("PROG g(d)128") << "PROG g(d)128" << 0x78 << 0x00 << 0x78 << 0x00 << 127;
        QTest::newRow("COMBI A000") << "COMBI A000" << 0x00 << 0x00 << 0x3F << 0x00 << 0;
        QTest::newRow("COMBI B100") << "COMBI B100" << 0x00 << 0x01 << 0x3F << 0x01 << 100;
        QTest::newRow("COMBI C127") << "COMBI C127" << 0x00 << 0x02 << 0x3F << 0x02 << 127;
    }
    void bankSelectTable() {
        QFETCH(QString, addr);
        QFETCH(int, korgMsb);
        QFETCH(int, korgLsb);
        QFETCH(int, gmMsb);
        QFETCH(int, gmLsb);
        QFETCH(int, pc);
        const A a = one(addr);
        QVERIFY(a.isValid());
        QVERIFY(a.bankSelect(BankMap::Korg) == (A::BankSelect{korgMsb, korgLsb, pc}));
        QVERIFY(a.bankSelect(BankMap::Gm) == (A::BankSelect{gmMsb, gmLsb, pc}));
    }
};

QTEST_APPLESS_MAIN(TestSoundAddress)
#include "test_sound_address.moc"
