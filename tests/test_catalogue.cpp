// T1.4: Catalogue. SPEC 3.2: expected counts 384 combinations, 512 programs,
// 128 GM, 9 GM drum kits; unit-name overlay and edited flag. Names below are
// read by hand from data/x50-factory.tsv. No MIDI bytes involved.
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTest>

#include "core/Catalogue.h"

using A = SoundAddress;

class TestCatalogue : public QObject {
    Q_OBJECT
    Catalogue m_cat;

private slots:
    void initTestCase() {
        QString err;
        QVERIFY2(m_cat.loadFromFile(QStringLiteral(X50_FACTORY_TSV), &err), qPrintable(err));
    }

    void counts() {
        int combi = 0, prog = 0, g = 0, gd = 0;
        for (const Catalogue::Entry &e : m_cat.entries()) {
            if (e.address.kind() == A::Kind::Combi) ++combi;
            else if (e.address.bank() == A::Bank::G) ++g;
            else if (e.address.bank() == A::Bank::Gd) ++gd;
            else ++prog;
        }
        QCOMPARE(combi, 384);
        QCOMPARE(prog, 512);
        QCOMPARE(g, 128);
        QCOMPARE(gd, 9);
        QCOMPARE(m_cat.size(), 1033);
    }

    void order() {
        const auto &v = m_cat.entries();
        for (int i = 1; i < v.size(); ++i)
            QVERIFY(v[i - 1].address < v[i].address);
        QCOMPARE(v.first().address, A(A::Kind::Combi, A::Bank::A, 0));
        QCOMPARE(v.last().address, A(A::Kind::Prog, A::Bank::Gd, 57));  // PROG g(d)057 SFX Kit
    }

    void lookup() {
        // COMBI A000 StereophonicKeys / Keyboard
        const Catalogue::Entry *e = m_cat.find(A(A::Kind::Combi, A::Bank::A, 0));
        QVERIFY(e);
        QCOMPARE(e->name, QStringLiteral("StereophonicKeys"));
        QCOMPARE(e->category, QStringLiteral("Keyboard"));
        QVERIFY(e->osc.isNull());
        // PROG A001 StereoPiano mf/f, Keyboard, d
        e = m_cat.find(A(A::Kind::Prog, A::Bank::A, 1));
        QVERIFY(e);
        QCOMPARE(e->name, QStringLiteral("StereoPiano mf/f"));
        QCOMPARE(e->osc, QChar::fromLatin1('d'));
        // PROG A100 "! {Tricky} Kit !", Drums, blank osc
        e = m_cat.find(A(A::Kind::Prog, A::Bank::A, 100));
        QVERIFY(e);
        QCOMPARE(e->category, QStringLiteral("Drums"));
        QVERIFY(e->osc.isNull());
        // PROG G001 Acoustic Piano, s
        e = m_cat.find(A(A::Kind::Prog, A::Bank::G, 1));
        QVERIFY(e);
        QCOMPARE(e->name, QStringLiteral("Acoustic Piano"));
        QCOMPARE(e->osc, QChar::fromLatin1('s'));
        // PROG g(d)001 STANDARD Kit
        e = m_cat.find(A(A::Kind::Prog, A::Bank::Gd, 1));
        QVERIFY(e);
        QCOMPARE(e->name, QStringLiteral("STANDARD Kit"));
        // not in the catalogue
        QVERIFY(!m_cat.find(A(A::Kind::Prog, A::Bank::Gd, 2)));
        QVERIFY(!m_cat.find(A(A::Kind::Combi, A::Bank::D, 0)));
        QVERIFY(!m_cat.find(A()));
    }

    void categories() {
        const QStringList c = m_cat.categories(A::Kind::Combi);
        QVERIFY(c.contains(QStringLiteral("Complex & SE")));
        QVERIFY(c.contains(QStringLiteral("Bell/Mallet/Perc")));
        QVERIFY(c.contains(QStringLiteral("User")));
        QVERIFY(!c.contains(QStringLiteral("Drums")));
        QCOMPARE(c.size(), 16);  // SPEC 4.1: 16 combination categories
        const QStringList p = m_cat.categories(A::Kind::Prog);
        QVERIFY(p.contains(QStringLiteral("Drums")));
        QVERIFY(!p.contains(QStringLiteral("User")));
        QCOMPARE(p.size(), 16);  // SPEC 4.1: 16 program categories
        QStringList sorted = c;
        std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
        QCOMPARE(c, sorted);
        QCOMPARE(QSet<QString>(c.begin(), c.end()).size(), c.size());
    }

    void overlayAndEdited() {
        Catalogue c;
        QVERIFY(c.loadFromFile(QStringLiteral(X50_FACTORY_TSV)));
        const A a(A::Kind::Combi, A::Bank::A, 0);  // StereophonicKeys
        const QDateTime when(QDate(2026, 5, 1), QTime(12, 0), QTimeZone::utc());
        QVERIFY(!c.unitName(a));
        QVERIFY(!c.isEdited(a));
        QCOMPARE(c.effectiveName(a), QStringLiteral("StereophonicKeys"));

        // same name, padded as the X50 sends it: not edited
        c.setUnitName(a, QStringLiteral("StereophonicKeys  "), QStringLiteral("Keyboard"), when);
        QVERIFY(c.unitName(a));
        QCOMPARE(c.unitName(a)->readAt, when);
        QVERIFY(!c.isEdited(a));

        c.setUnitName(a, QStringLiteral("My Keys"), QString(), when);
        QVERIFY(c.isEdited(a));
        QCOMPARE(c.effectiveName(a), QStringLiteral("My Keys"));
        QCOMPARE(c.effectiveCategory(a), QStringLiteral("Keyboard"));  // fallback
        c.setUnitName(a, QStringLiteral("My Keys"), QStringLiteral("Pads"), when);
        QCOMPARE(c.effectiveCategory(a), QStringLiteral("Pads"));
        // the other kind at the same bank/number is separate
        QVERIFY(!c.isEdited(A(A::Kind::Prog, A::Bank::A, 0)));
        QCOMPARE(c.unitNameCount(), 1);

        c.clearUnitNames();
        QCOMPARE(c.unitNameCount(), 0);
        QVERIFY(!c.isEdited(a));
        QCOMPARE(c.effectiveName(a), QStringLiteral("StereophonicKeys"));
    }

    void resource() {
        Catalogue c;
        QString err;
        QVERIFY2(c.loadFromResource(&err), qPrintable(err));
        QCOMPARE(c.size(), 1033);
        QCOMPARE(c.entries().first().name, QStringLiteral("StereophonicKeys"));
    }

    void missingFile() {
        Catalogue c;
        QString err;
        QVERIFY(!c.loadFromFile(QStringLiteral("/nonexistent/x50.tsv"), &err));
        QVERIFY(!err.isEmpty());
        QCOMPARE(c.size(), 0);
    }

    void malformed_data() {
        QTest::addColumn<QByteArray>("tsv");
        QTest::addColumn<QString>("expect");
        QTest::newRow("too few columns") << QByteArray("COMBI\tA\t000\tName\n") << "columns";
        QTest::newRow("bad kind") << QByteArray("SONG\tA\t000\tName\tPads\t\n") << "kind";
        QTest::newRow("bad bank") << QByteArray("PROG\tZ\t000\tName\tPads\td\n") << "bad address";
        QTest::newRow("combi in D") << QByteArray("COMBI\tD\t000\tName\tPads\t\n") << "bad address";
        QTest::newRow("number out of range") << QByteArray("PROG\tA\t128\tName\tPads\td\n") << "bad address";
        QTest::newRow("G zero") << QByteArray("PROG\tG\t000\tName\tPads\td\n") << "bad address";
        QTest::newRow("short number") << QByteArray("PROG\tA\t1\tName\tPads\td\n") << "3 digits";
        QTest::newRow("empty name") << QByteArray("PROG\tA\t001\t\tPads\td\n") << "name";
        QTest::newRow("long name") << QByteArray("PROG\tA\t001\t12345678901234567\tPads\td\n") << "name";
        QTest::newRow("empty category") << QByteArray("PROG\tA\t001\tName\t\td\n") << "category";
        QTest::newRow("bad osc") << QByteArray("PROG\tA\t001\tName\tPads\tx\n") << "oscillator";
        QTest::newRow("osc on combi") << QByteArray("COMBI\tA\t001\tName\tPads\td\n") << "oscillator";
        QTest::newRow("duplicate")
            << QByteArray("PROG\tA\t001\tName\tPads\td\nPROG\tA\t001\tOther\tPads\td\n") << "duplicate";
        QTest::newRow("empty") << QByteArray() << "empty";
    }
    void malformed() {
        QFETCH(QByteArray, tsv);
        QFETCH(QString, expect);
        Catalogue c;
        QString err;
        QVERIFY(!c.loadFromText(tsv, &err));
        QVERIFY2(err.contains(expect), qPrintable(err));
        QCOMPARE(c.size(), 0);
    }

    // R-T1.4: unit category names (MIDI.md 7, TABLE 4); parsing is T1.6, so the
    // lists here are arbitrary test strings.
    void unitCategories() {
        Catalogue c;
        QVERIFY(c.loadFromFile(QStringLiteral(X50_FACTORY_TSV)));
        const QStringList factoryP = c.categories(A::Kind::Prog);
        const QStringList factoryC = c.categories(A::Kind::Combi);
        QVERIFY(!c.hasUnitCategories());
        QStringList p, k;
        for (int i = 0; i < 16; ++i) {
            p << QStringLiteral("UP%1").arg(i);
            k << QStringLiteral("UC%1").arg(i);
        }
        QString err;
        QVERIFY2(c.setUnitCategories(p, k, &err), qPrintable(err));
        QVERIFY(c.hasUnitCategories());
        QCOMPARE(c.categories(A::Kind::Prog), p);   // unit order, not sorted
        QCOMPARE(c.categories(A::Kind::Combi), k);
        // Per-slot categories (including GM banks G and g(d)) stay factory ones.
        QCOMPARE(c.effectiveCategory(A(A::Kind::Prog, A::Bank::G, 1)),
                 c.find(A(A::Kind::Prog, A::Bank::G, 1))->category);
        c.clearUnitCategories();
        QVERIFY(!c.hasUnitCategories());
        QCOMPARE(c.categories(A::Kind::Prog), factoryP);
        QCOMPARE(c.categories(A::Kind::Combi), factoryC);
    }
    void unitCategoriesWrongLength() {
        Catalogue c;
        QVERIFY(c.loadFromFile(QStringLiteral(X50_FACTORY_TSV)));
        const QStringList sixteen(16, QStringLiteral("x"));
        QString err;
        QVERIFY(!c.setUnitCategories(QStringList(15, QStringLiteral("x")), sixteen, &err));
        QVERIFY2(err.contains(QStringLiteral("16")), qPrintable(err));
        QVERIFY(!c.setUnitCategories(sixteen, QStringList(17, QStringLiteral("x"))));
        QVERIFY(!c.setUnitCategories(QStringList(), QStringList()));
        QVERIFY(!c.hasUnitCategories());
        QCOMPARE(c.categories(A::Kind::Prog).size(), 16);  // still factory
        QVERIFY(c.categories(A::Kind::Prog) != sixteen);
    }

    void acceptsCrlfAndUnsortedInput() {
        Catalogue c;
        QString err;
        QVERIFY2(c.loadFromText("PROG\tA\t001\tB Name\tPads\td\r\nCOMBI\tA\t000\tA Name\tPads\t\r\n", &err),
                 qPrintable(err));
        QCOMPARE(c.size(), 2);
        QCOMPARE(c.entries()[0].address, A(A::Kind::Combi, A::Bank::A, 0));
        QCOMPARE(c.entries()[1].name, QStringLiteral("B Name"));
    }
};

QTEST_GUILESS_MAIN(TestCatalogue)
#include "test_catalogue.moc"
