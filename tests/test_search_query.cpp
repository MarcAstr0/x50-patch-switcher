// T1.4: SearchQuery. Rules from SPEC 4.1 and 3.1 (address grammar); the choices
// where the spec is silent are listed in SearchQuery.h. Expected counts and
// addresses were read from data/x50-factory.tsv by hand (grep/awk), not from
// the code under test. No MIDI bytes are involved.
#include <QTest>

#include "core/Catalogue.h"
#include "core/SearchQuery.h"

Q_DECLARE_METATYPE(QStringList)

class TestSearchQuery : public QObject {
    Q_OBJECT
    Catalogue m_cat;

private slots:
    void initTestCase() {
        QString err;
        QVERIFY2(m_cat.loadFromFile(QStringLiteral(X50_FACTORY_TSV), &err), qPrintable(err));
        QCOMPARE(m_cat.size(), 1033);
    }

    // SPEC 4.1: counts against the real catalogue.
    void counts_data() {
        QTest::addColumn<QString>("query");
        QTest::addColumn<int>("count");
        auto row = [](const char *name, const QString &q, int n) {
            QTest::newRow(name) << q << n;
        };
        // empty and plain text
        row("empty", QString(), 1033);
        row("spaces only", QStringLiteral("   "), 1033);
        row("text piano", QStringLiteral("piano"), 37);
        row("text case-insensitive", QStringLiteral("PiAnO"), 37);
        row("text in category", QStringLiteral("strings"), 67);
        row("text no match", QStringLiteral("zzzzq"), 0);
        // multi-token AND
        row("and piano grand", QStringLiteral("piano grand"), 1);
        row("and order independent", QStringLiteral("grand piano"), 1);
        row("and bass pad", QStringLiteral("bass pad"), 1);
        row("and one token fails", QStringLiteral("piano zzzzq"), 0);
        // address tokens
        row("bare address both kinds", QStringLiteral("A012"), 2);
        row("bare lower no zero pad", QStringLiteral("a12"), 2);
        row("bare D only prog", QStringLiteral("d5"), 1);
        row("PROG A012", QStringLiteral("PROG A012"), 1);
        row("P A012 spaced", QStringLiteral("P A012"), 1);
        row("COMBI B100", QStringLiteral("COMBI B100"), 1);
        row("C B100 spaced", QStringLiteral("C B100"), 1);
        row("cB100 glued", QStringLiteral("cB100"), 1);
        row("G1", QStringLiteral("G1"), 1);
        row("G001", QStringLiteral("G001"), 1);
        row("gd1", QStringLiteral("gd1"), 1);
        row("g(d)001", QStringLiteral("g(d)001"), 1);
        row("out of range rejected", QStringLiteral("a200"), 0);
        row("G000 rejected", QStringLiteral("g0"), 0);
        row("bank E rejected", QStringLiteral("e5"), 0);
        // R-T1.4 fixes: atomic split rejection, tight joining
        row("C D000 atomic reject", QStringLiteral("C D000"), 0);
        row("c d 0 atomic reject", QStringLiteral("c d 0"), 0);
        row("piano c 12 is text plus C012", QStringLiteral("piano c 12"), 0);
        row("bell c 12 is text plus C012", QStringLiteral("bell c 12"), 1);  // COMBI C012 cat Bell/Mallet/Perc
        row("piano c stays text", QStringLiteral("piano c"), 9);  // text AND text, counted with awk
        row("e 5 bank E", QStringLiteral("e 5"), 0);
        row("a 12 bank+digits", QStringLiteral("a 12"), 2);
        row("prog a 12 three tokens", QStringLiteral("prog a 12"), 1);
        row("quote only", QStringLiteral("\""), 1033);
        row("empty quotes", QStringLiteral("\"\""), 1033);
        row("address AND text hit", QStringLiteral("a012 bell"), 1);   // COMBI A012 Bellagio
        row("address AND text other kind", QStringLiteral("a012 guitar"), 1);  // PROG A012
        row("address AND text miss", QStringLiteral("a012 piano"), 0);
        row("address words around", QStringLiteral("nylon p a012"), 1);
        // cat:
        row("cat bell", QStringLiteral("cat:bell"), 54);
        row("cat mallet part", QStringLiteral("cat:mallet"), 54);
        row("cat complex", QStringLiteral("cat:complex"), 18);
        row("cat quoted multi-word", QStringLiteral("cat:\"complex & se\""), 18);
        row("cat uppercase key", QStringLiteral("CAT:Complex"), 18);
        row("cat bass", QStringLiteral("cat:bass"), 145);
        row("cat prefix not substring", QStringLiteral("cat:ass"), 0);
        row("cat ds/hits slash", QStringLiteral("cat:ds/hits"), 8);
        row("cat hit part", QStringLiteral("cat:arpg"), 32);
        row("cat empty matches all", QStringLiteral("cat:"), 1033);
        row("cat none", QStringLiteral("cat:zzz"), 0);
        row("cat drums and text", QStringLiteral("cat:drums standard"), 4);
        row("cat and cat", QStringLiteral("cat:bell cat:bass"), 0);
        row("text kit", QStringLiteral("kit"), 28);
    }
    void counts() {
        QFETCH(QString, query);
        QFETCH(int, count);
        QCOMPARE(SearchQuery::parse(query).search(m_cat).size(), count);
    }

    // Specific addresses (TSV lines in comments).
    void specificAddresses() {
        using A = SoundAddress;
        // COMBI A012 Bellagio, PROG A012 Nylon Guitar 2
        QVector<A> r = SearchQuery::parse(QStringLiteral("a12")).search(m_cat);
        QCOMPARE(r.size(), 2);
        QCOMPARE(r[0], A(A::Kind::Combi, A::Bank::A, 12));  // catalogue order: COMBI first
        QCOMPARE(r[1], A(A::Kind::Prog, A::Bank::A, 12));
        // PROG g(d)001 STANDARD Kit
        r = SearchQuery::parse(QStringLiteral("gd1")).search(m_cat);
        QCOMPARE(r, QVector<A>{A(A::Kind::Prog, A::Bank::Gd, 1)});
        // COMBI C127 Init.Combination, category User
        r = SearchQuery::parse(QStringLiteral("cat:user")).search(m_cat);
        QCOMPARE(r, QVector<A>{A(A::Kind::Combi, A::Bank::C, 127)});
        // PROG G012 Vibraphone
        r = SearchQuery::parse(QStringLiteral("vibraphone")).search(m_cat);
        QVERIFY(r.contains(A(A::Kind::Prog, A::Bank::G, 12)));
    }

    // Results are in catalogue (SoundAddress) order.
    void resultOrder() {
        const QVector<SoundAddress> r = SearchQuery::parse(QStringLiteral("piano")).search(m_cat);
        for (int i = 1; i < r.size(); ++i)
            QVERIFY(r[i - 1] < r[i]);
    }

    // Accent folding on a synthetic catalogue (the factory list is ASCII only).
    void accents_data() {
        QTest::addColumn<QString>("query");
        QTest::addColumn<int>("count");
        QTest::newRow("plain finds accented") << QStringLiteral("cafe") << 1;
        QTest::newRow("accented finds accented") << QStringLiteral("café") << 1;
        QTest::newRow("accented finds plain") << QStringLiteral("éclat") << 1;
        QTest::newRow("upper accented") << QStringLiteral("CAFÉ") << 1;
        QTest::newRow("decomposed input") << QStringLiteral("café") << 1;
        QTest::newRow("accented category") << QStringLiteral("cat:électro") << 1;
        QTest::newRow("accent mismatch letter") << QStringLiteral("cafo") << 0;
    }
    void accents() {
        Catalogue c;
        QString err;
        const QByteArray tsv =
            "COMBI\tA\t000\tCaf\xc3\xa9 Pad\tKeyboard\t\n"
            "PROG\tA\t001\tEclat\tElectro\td\n";
        // Second row's category stays ASCII; accents on the overlay below.
        QVERIFY2(c.loadFromText(tsv, &err), qPrintable(err));
        c.setUnitName(SoundAddress(SoundAddress::Kind::Prog, SoundAddress::Bank::A, 1),
                      QStringLiteral("\u00c9clat"), QStringLiteral("\u00c9lectro"),
                      QDateTime());
        QFETCH(QString, query);
        QFETCH(int, count);
        QCOMPARE(SearchQuery::parse(query).search(c).size(), count);
    }

    // Edited slot: the unit name is searched, the factory name is not.
    void editedSlotUsesUnitName() {
        Catalogue c;
        QVERIFY(c.loadFromFile(QStringLiteral(X50_FACTORY_TSV)));
        const SoundAddress a(SoundAddress::Kind::Prog, SoundAddress::Bank::A, 12);  // Nylon Guitar 2
        c.setUnitName(a, QStringLiteral("Zebra Banjo"), QString(), QDateTime());
        QCOMPARE(SearchQuery::parse(QStringLiteral("zebra")).search(c), QVector<SoundAddress>{a});
        QVERIFY(!SearchQuery::parse(QStringLiteral("nylon")).search(c).contains(a));
        // Category falls back to the factory one when the overlay has none.
        QVERIFY(SearchQuery::parse(QStringLiteral("cat:guitar")).search(c).contains(a));
    }

    // R-T1.4: quoted tokens are text, never an address.
    void quotedIsText() {
        Catalogue c;
        QVERIFY(c.loadFromText(
            "COMBI\tB\t003\tOrgan Zed\tKeyboard\t\n"
            "PROG\tA\t001\tB3 Organ\tOrgan\td\n", nullptr));
        const SoundAddress a(SoundAddress::Kind::Prog, SoundAddress::Bank::A, 1);
        const SoundAddress b3(SoundAddress::Kind::Combi, SoundAddress::Bank::B, 3);
        QCOMPARE(SearchQuery::parse(QStringLiteral("b3 organ")).search(c), QVector<SoundAddress>{b3});
        QCOMPARE(SearchQuery::parse(QStringLiteral("\"b3\"")).search(c), QVector<SoundAddress>{a});
        QCOMPARE(SearchQuery::parse(QStringLiteral("\"b3\" organ")).search(c), QVector<SoundAddress>{a});
        // A quoted token is not joined into an address either.
        QCOMPARE(SearchQuery::parse(QStringLiteral("\"b\" 3")).search(c), QVector<SoundAddress>{a});
    }

    void emptyTokensDropped() {
        QVERIFY(SearchQuery::parse(QStringLiteral("\"\"")).isEmpty());
        QVERIFY(SearchQuery::parse(QStringLiteral("\"")).isEmpty());
        QVERIFY(SearchQuery::parse(QStringLiteral("\"\" \"\"")).isEmpty());
        QVERIFY(!SearchQuery::parse(QStringLiteral("cat:")).isEmpty());
        QVERIFY(!SearchQuery::parse(QStringLiteral("\"\" piano")).isEmpty());
        QCOMPARE(SearchQuery::parse(QStringLiteral("\"\" piano")).search(m_cat).size(), 37);
    }

    void emptiness() {
        QVERIFY(SearchQuery::parse(QString()).isEmpty());
        QVERIFY(SearchQuery::parse(QStringLiteral("  ")).isEmpty());
        QVERIFY(!SearchQuery::parse(QStringLiteral("a")).isEmpty());
        QVERIFY(!SearchQuery::parse(QStringLiteral("a200")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSearchQuery)
#include "test_search_query.moc"
