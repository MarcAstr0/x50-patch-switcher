// T1.1: checks data/x50-factory.tsv (SPEC 3.2). Counts and spot rows come from
// the plan and from VNL (docs/MIDI.md "Sources"): COMBI A000 StereophonicKeys
// (Keyboard), PROG A000 Portamento Pad (SlowSynth), PROG A065 M1 Piano,
// PROG G001 Acoustic Piano.
#include <QFile>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVector>

namespace {
struct Row {
    QString kind, bank, number, name, category, osc;
};
}

class TestFactoryTable : public QObject {
    Q_OBJECT
    QVector<Row> rows;

    const Row *find(const QString &kind, const QString &bank, const QString &num) const {
        for (const Row &r : rows)
            if (r.kind == kind && r.bank == bank && r.number == num)
                return &r;
        return nullptr;
    }
    int count(const QString &kind, const QStringList &banks) const {
        int n = 0;
        for (const Row &r : rows)
            if (r.kind == kind && banks.contains(r.bank))
                ++n;
        return n;
    }

private slots:
    void initTestCase() {
        QFile f(QStringLiteral(X50_FACTORY_TSV));
        QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "cannot open factory TSV");
        const QString text = QString::fromUtf8(f.readAll());
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList c = line.split(QLatin1Char('\t'));
            QCOMPARE(c.size(), 6);
            rows.push_back({c[0], c[1], c[2], c[3], c[4], c[5]});
        }
    }

    void counts() {
        QCOMPARE(count(QStringLiteral("COMBI"), {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")}), 384);
        QCOMPARE(count(QStringLiteral("PROG"), {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D")}), 512);
        QCOMPARE(count(QStringLiteral("PROG"), {QStringLiteral("G")}), 128);
        QCOMPARE(count(QStringLiteral("PROG"), {QStringLiteral("g(d)")}), 9);
        QCOMPARE(rows.size(), 384 + 512 + 128 + 9);
    }

    void spotRows() {
        const Row *r = find(QStringLiteral("COMBI"), QStringLiteral("A"), QStringLiteral("000"));
        QVERIFY(r);
        QCOMPARE(r->name, QStringLiteral("StereophonicKeys"));
        QCOMPARE(r->category, QStringLiteral("Keyboard"));

        r = find(QStringLiteral("PROG"), QStringLiteral("A"), QStringLiteral("000"));
        QVERIFY(r);
        QCOMPARE(r->name, QStringLiteral("Portamento Pad"));
        QCOMPARE(r->category, QStringLiteral("SlowSynth"));

        r = find(QStringLiteral("PROG"), QStringLiteral("A"), QStringLiteral("065"));
        QVERIFY(r);
        QCOMPARE(r->name, QStringLiteral("M1 Piano"));

        r = find(QStringLiteral("PROG"), QStringLiteral("G"), QStringLiteral("001"));
        QVERIFY(r);
        QCOMPARE(r->name, QStringLiteral("Acoustic Piano"));
    }

    void namesAndAddresses() {
        QSet<QString> seen;
        for (const Row &r : rows) {
            QVERIFY2(!r.name.isEmpty(), qPrintable(r.bank + r.number));
            QVERIFY2(r.name.length() <= 16, qPrintable(r.name));
            QVERIFY2(!r.category.isEmpty(), qPrintable(r.bank + r.number));
            const QString key = r.kind + QLatin1Char(' ') + r.bank + r.number;
            QVERIFY2(!seen.contains(key), qPrintable(key));
            seen.insert(key);
        }
    }
};

QTEST_APPLESS_MAIN(TestFactoryTable)
#include "test_factory_table.moc"
