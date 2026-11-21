// T1.5: Library persistence, debounce, sidelining, sets. No MIDI bytes.
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "core/Library.h"

namespace {
SoundAddress combi(int n) { return SoundAddress(SoundAddress::Kind::Combi, SoundAddress::Bank::A, n); }
SoundAddress prog(SoundAddress::Bank b, int n) { return SoundAddress(SoundAddress::Kind::Prog, b, n); }

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
void writeAll(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(data);
}
QStringList brokenFiles(const QString &dir)
{
    return QDir(dir).entryList({QStringLiteral("library.json.broken-*")}, QDir::Files);
}
}

class TestLibrary : public QObject {
    Q_OBJECT
private slots:
    void emptyWhenMissing() {
        QTemporaryDir d;
        Library lib;
        QVERIFY(lib.load(d.path()));
        QVERIFY(lib.loadError().isEmpty());
        QVERIFY(lib.sets().isEmpty());
        QVERIFY(lib.currentSet() == nullptr);
        QVERIFY(!lib.hasPendingSave());
    }

    void roundTripEveryField() {
        QTemporaryDir d;
        const QDateTime t1 = QDateTime(QDate(2026, 5, 1), QTime(12, 0, 5), Qt::UTC);
        const QDateTime t2 = QDateTime(QDate(2026, 5, 2), QTime(8, 30, 0), Qt::UTC);
        QString setA, setB;
        {
            Library lib;
            lib.load(d.path());
            QVERIFY(lib.createSet(QStringLiteral("Gig \xc3\xa9"), 8, &setA).ok());
            QVERIFY(lib.createSet(QStringLiteral("Second"), 6, &setB).ok());
            QVERIFY(lib.setSlot(setA, 0, combi(3)).ok());
            QVERIFY(lib.setLabel(setA, 0, QStringLiteral("Song")).ok());
            QVERIFY(lib.setSlot(setA, 7, prog(SoundAddress::Bank::G, 128)).ok());
            QVERIFY(lib.setSlot(setB, 2, prog(SoundAddress::Bank::Gd, 1)).ok());
            QVERIFY(lib.setCurrentSet(setA).ok());
            lib.setFavourite(combi(3), true);
            lib.setFavourite(prog(SoundAddress::Bank::B, 9), true);
            lib.recordUse(combi(3), t1);
            lib.recordUse(combi(3), t2);
            lib.recordUse(prog(SoundAddress::Bank::C, 0), t2.addSecs(60));
            Catalogue::UnitName un;
            un.name = QStringLiteral("My Pad");
            un.category = QStringLiteral("Pad");
            un.readAt = t1;
            Catalogue::UnitName un2;
            un2.name = QStringLiteral("NoCat");
            un2.readAt = t2;
            lib.setUnitNames({{combi(3), un}, {prog(SoundAddress::Bank::A, 4), un2}});
            Library::UnitInfo ui;
            ui.memberCode = 1;
            ui.version = QStringLiteral("1.02");
            ui.globalChannel = 5;
            ui.bankMap = BankMap::Gm;
            ui.lastSeen = t2;
            lib.setUnit(ui);
            lib.flush();
            QVERIFY(lib.lastSaveError().isEmpty());
        }
        Library lib;
        QVERIFY(lib.load(d.path()));
        QVERIFY2(lib.loadWarnings().isEmpty(), qPrintable(lib.loadWarnings().join(QLatin1Char('\n'))));
        QCOMPARE(lib.sets().size(), 2);
        QCOMPARE(lib.currentSetId(), setA);
        const SetModel *a = lib.setById(setA);
        QVERIFY(a);
        QCOMPARE(a->name(), QStringLiteral("Gig \xc3\xa9"));
        QCOMPARE(a->size(), 8);
        QCOMPARE(*a->slot(0).address, combi(3));
        QCOMPARE(a->slot(0).label, QStringLiteral("Song"));
        QCOMPARE(*a->slot(7).address, prog(SoundAddress::Bank::G, 128));
        QVERIFY(a->slot(1).isEmpty());
        QCOMPARE(lib.setById(setB)->size(), 6);
        QCOMPARE(*lib.setById(setB)->slot(2).address, prog(SoundAddress::Bank::Gd, 1));
        QCOMPARE(lib.favourites().size(), 2);
        QVERIFY(lib.isFavourite(combi(3)));
        QVERIFY(lib.isFavourite(prog(SoundAddress::Bank::B, 9)));
        QVERIFY(!lib.isFavourite(combi(4)));
        QCOMPARE(lib.recents().size(), 2);
        QCOMPARE(lib.recentCount(combi(3)), 2);
        QCOMPARE(lib.recentCount(prog(SoundAddress::Bank::C, 0)), 1);
        QCOMPARE(lib.recents().first().address, prog(SoundAddress::Bank::C, 0));
        QCOMPARE(lib.recents().at(1).lastUsed, t2);
        QCOMPARE(lib.unitNames().size(), 2);
        QCOMPARE(lib.unitNames().value(combi(3)).name, QStringLiteral("My Pad"));
        QCOMPARE(lib.unitNames().value(combi(3)).category, QStringLiteral("Pad"));
        QCOMPARE(lib.unitNames().value(combi(3)).readAt, t1);
        QVERIFY(lib.unitNames().value(prog(SoundAddress::Bank::A, 4)).category.isEmpty());
        QVERIFY(lib.unit().has_value());
        QCOMPARE(lib.unit()->memberCode, 1);
        QCOMPARE(lib.unit()->version, QStringLiteral("1.02"));
        QCOMPARE(lib.unit()->globalChannel, 5);
        QVERIFY(lib.unit()->bankMap == BankMap::Gm);
        QCOMPARE(lib.unit()->lastSeen, t2);
    }

    void fileShape() {
        QTemporaryDir d;
        Library lib;
        lib.load(d.path());
        QString id;
        lib.createSet(QStringLiteral("S"), 6, &id);
        lib.setSlot(id, 0, combi(0));
        lib.flush();
        const QJsonObject root = QJsonDocument::fromJson(readAll(d.filePath(QStringLiteral("library.json")))).object();
        for (const char *k : {"favourites", "sets", "currentSetId", "recents", "unitNames"})
            QVERIFY2(root.contains(QString::fromLatin1(k)), k);
        const QJsonObject s = root.value(QStringLiteral("sets")).toArray().first().toObject();
        QCOMPARE(s.value(QStringLiteral("size")).toInt(), 6);
        QCOMPARE(s.value(QStringLiteral("slots")).toArray().size(), 6);
        QCOMPARE(s.value(QStringLiteral("slots")).toArray().first().toObject().value(QStringLiteral("address")).toString(),
                 QStringLiteral("COMBI A000"));
        QVERIFY(s.value(QStringLiteral("slots")).toArray().at(1).isNull());
    }

    void debounceOneWrite() {
        QTemporaryDir d;
        Library lib;
        lib.load(d.path());
        QSignalSpy saved(&lib, &Library::saved);
        lib.setFavourite(combi(1), true);
        QTest::qWait(200);
        lib.setFavourite(combi(2), true);
        QTest::qWait(200);
        lib.setFavourite(combi(3), true);
        QVERIFY(lib.hasPendingSave());
        QCOMPARE(lib.writeCount(), 0);
        QVERIFY(!QFileInfo::exists(d.filePath(QStringLiteral("library.json"))));
        // 400 ms after the first change, 200 after the last: still nothing.
        QTest::qWait(350);
        QCOMPARE(lib.writeCount(), 0);
        QTest::qWait(400);
        QCOMPARE(lib.writeCount(), 1);
        QCOMPARE(saved.count(), 1);
        QVERIFY(!lib.hasPendingSave());
        QTest::qWait(700);
        QCOMPARE(lib.writeCount(), 1);
        Library again;
        again.load(d.path());
        QCOMPARE(again.favourites().size(), 3);
    }

    void flushWritesAtOnceAndOnlyWhenDirty() {
        QTemporaryDir d;
        Library lib;
        lib.load(d.path());
        lib.flush();
        QCOMPARE(lib.writeCount(), 0);
        lib.setFavourite(combi(1), true);
        lib.flush();
        QCOMPARE(lib.writeCount(), 1);
        QTest::qWait(700);
        QCOMPARE(lib.writeCount(), 1);  // timer was cancelled
        lib.flush();
        QCOMPARE(lib.writeCount(), 1);
    }

    void noChangeNoSave() {
        QTemporaryDir d;
        Library lib;
        lib.load(d.path());
        lib.setFavourite(combi(1), false);
        QVERIFY(!lib.hasPendingSave());
        QString id;
        lib.createSet(QStringLiteral("S"), 6, &id);
        lib.flush();
        QVERIFY(!lib.resizeSet(id, 7).ok());
        QVERIFY(!lib.setLabel(id, 0, QStringLiteral("x")).ok());
        QVERIFY(!lib.hasPendingSave());
    }

    void missingDirectoryCreated() {
        QTemporaryDir d;
        const QString nested = d.filePath(QStringLiteral("a/b/c"));
        Library lib;
        QVERIFY(lib.load(nested));
        lib.setFavourite(combi(1), true);
        lib.flush();
        QVERIFY(lib.lastSaveError().isEmpty());
        QVERIFY(QFileInfo::exists(nested + QStringLiteral("/library.json")));
    }

    void brokenFileSidelined_data() {
        QTest::addColumn<QByteArray>("content");
        QTest::newRow("not json") << QByteArray("{ nope");
        QTest::newRow("empty file") << QByteArray();
        QTest::newRow("array root") << QByteArray("[]");
        QTest::newRow("sets wrong type") << QByteArray("{\"sets\": 3}");
        QTest::newRow("bad size") << QByteArray(
            "{\"sets\":[{\"id\":\"a\",\"name\":\"n\",\"size\":7,\"slots\":[null,null,null,null,null,null,null]}]}");
        QTest::newRow("slot count mismatch") << QByteArray(
            "{\"sets\":[{\"id\":\"a\",\"name\":\"n\",\"size\":6,\"slots\":[null]}]}");
        QTest::newRow("duplicate id") << QByteArray(
            "{\"sets\":[{\"id\":\"a\",\"name\":\"n\",\"size\":6,\"slots\":[null,null,null,null,null,null]},"
            "{\"id\":\"a\",\"name\":\"m\",\"size\":6,\"slots\":[null,null,null,null,null,null]}]}");
        QTest::newRow("bad bankMap") << QByteArray(
            "{\"unit\":{\"memberCode\":1,\"version\":\"1\",\"globalChannel\":1,\"bankMap\":\"x\"}}");
    }
    void brokenFileSidelined() {
        QFETCH(QByteArray, content);
        QTemporaryDir d;
        const QString path = d.filePath(QStringLiteral("library.json"));
        writeAll(path, content);
        Library lib;
        QVERIFY(!lib.load(d.path()));
        QVERIFY(!lib.loadError().isEmpty());
        QVERIFY(lib.sets().isEmpty());
        QVERIFY(!QFileInfo::exists(path));
        const QStringList broken = brokenFiles(d.path());
        QCOMPARE(broken.size(), 1);
        QCOMPARE(readAll(d.filePath(broken.first())), content);
        QCOMPARE(lib.brokenFilePath(), d.filePath(broken.first()));
        // Later saves do not touch the broken copy.
        lib.setFavourite(combi(1), true);
        lib.flush();
        QCOMPARE(readAll(d.filePath(broken.first())), content);
        QVERIFY(QFileInfo::exists(path));
    }

    void twoBrokenFilesBothKept() {
        QTemporaryDir d;
        const QString path = d.filePath(QStringLiteral("library.json"));
        for (int i = 0; i < 2; ++i) {
            writeAll(path, QByteArray("garbage") + QByteArray::number(i));
            Library lib;
            QVERIFY(!lib.load(d.path()));
        }
        QCOMPARE(brokenFiles(d.path()).size(), 2);
    }

    void invalidStoredAddressReported() {
        QTemporaryDir d;
        const QByteArray json =
            "{\"favourites\":[\"COMBI A001\",\"COMBI Z999\"],"
            "\"sets\":[{\"id\":\"a\",\"name\":\"n\",\"size\":6,\"slots\":["
            "{\"address\":\"COMBI A002\",\"label\":\"ok\"},{\"address\":\"rubbish\",\"label\":\"x\"},null,null,null,null]}],"
            "\"currentSetId\":\"a\","
            "\"recents\":[{\"address\":\"PROG Q1\",\"count\":1,\"lastUsed\":\"2026-01-01T00:00:00Z\"}],"
            "\"unitNames\":{\"nonsense\":{\"name\":\"x\",\"readAt\":\"2026-01-01T00:00:00Z\"}}}";
        writeAll(d.filePath(QStringLiteral("library.json")), json);
        Library lib;
        QVERIFY(lib.load(d.path()));  // not sidelined
        QVERIFY(brokenFiles(d.path()).isEmpty());
        QCOMPARE(lib.loadWarnings().size(), 4);
        QVERIFY(lib.loadWarnings().join(QLatin1Char(' ')).contains(QStringLiteral("rubbish")));
        QCOMPARE(lib.favourites().size(), 1);
        QCOMPARE(*lib.setById(QStringLiteral("a"))->slot(0).address, combi(2));
        QVERIFY(lib.setById(QStringLiteral("a"))->slot(1).isEmpty());
        QVERIFY(lib.recents().isEmpty());
        QVERIFY(lib.unitNames().isEmpty());
    }

    void unknownFieldsIgnored() {
        QTemporaryDir d;
        writeAll(d.filePath(QStringLiteral("library.json")),
                 "{\"future\":{\"x\":1},\"favourites\":[\"COMBI A001\"],\"sets\":[]}");
        Library lib;
        QVERIFY(lib.load(d.path()));
        QCOMPARE(lib.favourites().size(), 1);
        QVERIFY(lib.loadWarnings().isEmpty());
    }

    void badCurrentSetIdFallsBack() {
        QTemporaryDir d;
        writeAll(d.filePath(QStringLiteral("library.json")),
                 "{\"currentSetId\":\"zz\",\"sets\":[{\"id\":\"a\",\"name\":\"n\",\"size\":6,"
                 "\"slots\":[null,null,null,null,null,null]}]}");
        Library lib;
        QVERIFY(lib.load(d.path()));
        QCOMPARE(lib.currentSetId(), QStringLiteral("a"));
        QCOMPARE(lib.loadWarnings().size(), 1);
    }

    void readOnlyDirectoryReportsError() {
        QTemporaryDir d;
        QVERIFY(QFile::setPermissions(d.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        QFile probe(d.filePath(QStringLiteral("probe")));
        if (probe.open(QIODevice::WriteOnly)) {
            probe.close();
            QFile::setPermissions(d.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
            QSKIP("directory stays writable (running as a privileged user)");
        }
        Library lib;
        QVERIFY(lib.load(d.path()));
        QSignalSpy failed(&lib, &Library::saveFailed);
        lib.setFavourite(combi(1), true);
        const QString err = lib.save();
        QVERIFY(!err.isEmpty());
        QCOMPARE(failed.count(), 1);
        QVERIFY(lib.hasPendingSave());  // stays dirty
        QCOMPARE(lib.writeCount(), 0);
        QFile::setPermissions(d.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        lib.flush();  // retry works once writable
        QVERIFY(lib.lastSaveError().isEmpty());
        QCOMPARE(lib.writeCount(), 1);
    }

    void unreadableDirectoryPathIsFile() {
        QTemporaryDir d;
        const QString notDir = d.filePath(QStringLiteral("file"));
        writeAll(notDir, "x");
        Library lib;
        lib.load(notDir);  // "file/library.json" does not exist
        lib.setFavourite(combi(1), true);
        QVERIFY(!lib.save().isEmpty());
    }

    void recentsCap() {
        QTemporaryDir d;
        Library lib;
        lib.load(d.path());
        const QDateTime base(QDate(2026, 1, 1), QTime(0, 0), Qt::UTC);
        for (int i = 0; i < 60; ++i)
            lib.recordUse(combi(i), base.addSecs(i));
        QCOMPARE(lib.recents().size(), Library::kRecentsCap);
        QCOMPARE(lib.recents().first().address, combi(59));
        QCOMPARE(lib.recents().last().address, combi(10));
        QCOMPARE(lib.recentCount(combi(0)), 0);
        // Using an old one moves it to the front and counts.
        lib.recordUse(combi(10), base.addSecs(100));
        QCOMPARE(lib.recents().first().address, combi(10));
        QCOMPARE(lib.recentCount(combi(10)), 2);
        QCOMPARE(lib.recents().size(), Library::kRecentsCap);
        lib.flush();
        Library again;
        again.load(d.path());
        QCOMPARE(again.recents().size(), Library::kRecentsCap);
        QCOMPARE(again.recents().first().address, combi(10));
    }

    void favouritesToggle() {
        Library lib;
        QTemporaryDir d;
        lib.load(d.path());
        QVERIFY(lib.toggleFavourite(combi(1)));
        QVERIFY(!lib.toggleFavourite(combi(1)));
        lib.setFavourite(SoundAddress(), true);  // invalid: ignored
        QVERIFY(lib.favourites().isEmpty());
    }

    void setLifecycle() {
        QTemporaryDir d;
        Library lib;
        lib.load(d.path());
        QString a, b, c;
        QVERIFY(lib.createSet(QStringLiteral("A"), 6, &a).ok());
        QCOMPARE(lib.currentSetId(), a);
        QVERIFY(!lib.createSet(QStringLiteral("  "), 6).ok());
        QVERIFY(!lib.createSet(QStringLiteral("X"), 5).ok());
        QCOMPARE(lib.sets().size(), 1);
        QVERIFY(lib.createSet(QStringLiteral("B"), 8, &b).ok());
        QVERIFY(lib.renameSet(b, QStringLiteral("B2")).ok());
        QVERIFY(!lib.renameSet(b, QString()).ok());
        QVERIFY(!lib.renameSet(QStringLiteral("nope"), QStringLiteral("x")).ok());
        QCOMPARE(lib.setById(b)->name(), QStringLiteral("B2"));

        lib.setSlot(b, 0, combi(7));
        lib.setLabel(b, 0, QStringLiteral("L"));
        QVERIFY(lib.duplicateSet(b, &c).ok());
        QVERIFY(c != b);
        QCOMPARE(lib.sets().at(2).id(), c);  // inserted after the original
        QCOMPARE(lib.setById(c)->name(), QStringLiteral("B2 copy"));
        QCOMPARE(lib.setById(c)->size(), 8);
        QCOMPARE(lib.setById(c)->slot(0).label, QStringLiteral("L"));
        QCOMPARE(lib.currentSetId(), b);  // not switched
        lib.clearSlot(c, 0);
        QVERIFY(!lib.setById(b)->slot(0).isEmpty());  // independent copy
        QVERIFY(!lib.duplicateSet(QStringLiteral("nope")).ok());

        QVERIFY(lib.setCurrentSet(a).ok());
        QVERIFY(!lib.setCurrentSet(QStringLiteral("nope")).ok());
        QVERIFY(lib.deleteSet(a).ok());  // current deleted: neighbour becomes current
        QCOMPARE(lib.currentSetId(), b);
        QVERIFY(!lib.deleteSet(a).ok());
        QVERIFY(lib.deleteSet(c).ok());
        QCOMPARE(lib.currentSetId(), b);
        QVERIFY(lib.deleteSet(b).ok());
        QVERIFY(lib.sets().isEmpty());
        QVERIFY(lib.currentSetId().isEmpty());
        QVERIFY(lib.currentSet() == nullptr);
    }

    void slotOperationsThroughLibrary() {
        QTemporaryDir d;
        Library lib;
        lib.load(d.path());
        QString id;
        lib.createSet(QStringLiteral("S"), 8, &id);
        QSignalSpy changed(&lib, &Library::changed);
        QVERIFY(lib.setSlot(id, 6, combi(1)).ok());
        QVERIFY(lib.setSlot(id, 7, combi(2)).ok());
        QCOMPARE(changed.count(), 2);
        const auto r = lib.resizeSet(id, 6);
        QVERIFY(!r.ok());
        QVERIFY(r.error.contains(QStringLiteral("7 and 8")));
        QCOMPARE(changed.count(), 2);  // refusal changes nothing
        QCOMPARE(lib.setById(id)->size(), 8);
        QVERIFY(lib.moveSlot(id, 6, 0).ok());
        QVERIFY(lib.moveSlot(id, 7, 1).ok());
        QVERIFY(lib.resizeSet(id, 6).ok());
        QCOMPARE(lib.setById(id)->size(), 6);
        QVERIFY(lib.setLabel(id, 0, QStringLiteral("a")).ok());
        QVERIFY(lib.clearSlot(id, 1).ok());
        QVERIFY(!lib.setSlot(QStringLiteral("nope"), 0, combi(1)).ok());
        lib.flush();
        Library again;
        again.load(d.path());
        QCOMPARE(again.setById(id)->size(), 6);
        QCOMPARE(again.setById(id)->slot(0).label, QStringLiteral("a"));
        QVERIFY(again.setById(id)->slot(1).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "test_library.moc"
