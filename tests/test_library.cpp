// T1.5: Library persistence, debounce, sidelining, sets. No MIDI bytes.
#include <QDir>
#include <QTimeZone>
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
        const QDateTime t1 = QDateTime(QDate(2026, 5, 1), QTime(12, 0, 5), QTimeZone::utc());
        const QDateTime t2 = QDateTime(QDate(2026, 5, 2), QTime(8, 30, 0), QTimeZone::utc());
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

    void defaultDebounceIs500ms() {
        Library lib;
        QCOMPARE(lib.saveDebounceMs(), 500);
        QCOMPARE(Library::kSaveDebounceMs, 500);
    }

    void debounceOneWrite() {
        QTemporaryDir d;
        Library lib;
        lib.setSaveDebounceMs(300);
        lib.load(d.path());
        QSignalSpy saved(&lib, &Library::saved);
        lib.setFavourite(combi(1), true);
        // Immediately after the change: nothing written yet (no waiting involved).
        QVERIFY(lib.hasPendingSave());
        QCOMPARE(lib.writeCount(), 0);
        QVERIFY(!QFileInfo::exists(d.filePath(QStringLiteral("library.json"))));
        lib.setFavourite(combi(2), true);
        lib.setFavourite(combi(3), true);
        QCOMPARE(lib.writeCount(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(lib.writeCount(), 1, 3000);
        QCOMPARE(saved.count(), 1);
        QVERIFY(!lib.hasPendingSave());
        QTest::qWait(600);  // 2x the interval: still exactly one write
        QCOMPARE(lib.writeCount(), 1);
        Library again;
        again.load(d.path());
        QCOMPARE(again.favourites().size(), 3);
    }

    void flushWritesAtOnceAndOnlyWhenDirty() {
        QTemporaryDir d;
        Library lib;
        lib.setSaveDebounceMs(300);
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
            "{\"unit\":{\"memberCode\":1,\"version\":\"1\",\"globalChannel\":1,\"bankMap\":\"x\","
            "\"lastSeen\":\"2026-01-01T00:00:00Z\"}}");
    }
    void brokenFileSidelined() {
        QFETCH(QByteArray, content);
        checkSidelined(content);
    }

    // Item 2: empty, blank, missing or non-string set name.
    void setNameInvalidSidelined_data() {
        QTest::addColumn<QByteArray>("content");
        const char *slotsJson = "\"size\":6,\"slots\":[null,null,null,null,null,null]";
        for (const char *n : {"\"name\":\"\",", "\"name\":\"  \\t\",", "", "\"name\":5,"})
            QTest::newRow(*n ? n : "missing")
                << (QByteArray("{\"sets\":[{\"id\":\"a\",") + n + slotsJson + "}]}");
    }
    void setNameInvalidSidelined() {
        QFETCH(QByteArray, content);
        checkSidelined(content);
    }

    // Item 3: recents with a bad count or time are structural errors.
    void recentsInvalidSidelined_data() {
        QTest::addColumn<QByteArray>("content");
        const char *t = "\"lastUsed\":\"2026-01-01T00:00:00Z\"";
        auto row = [&](const char *name, const QByteArray &body) {
            QTest::newRow(name) << (QByteArray("{\"recents\":[{\"address\":\"COMBI A001\",") + body + "}]}");
        };
        row("count string", QByteArray("\"count\":\"2\",") + t);
        row("count fractional", QByteArray("\"count\":1.5,") + t);
        row("count missing", QByteArray(t));
        row("count zero", QByteArray("\"count\":0,") + t);
        row("lastUsed missing", "\"count\":1");
        row("lastUsed garbage", "\"count\":1,\"lastUsed\":\"yesterday\"");
        row("lastUsed number", "\"count\":1,\"lastUsed\":5");
    }
    void recentsInvalidSidelined() {
        QFETCH(QByteArray, content);
        checkSidelined(content);
    }

    // Item 4: unitNames entries with a bad category or readAt.
    void unitNamesInvalidSidelined_data() {
        QTest::addColumn<QByteArray>("content");
        auto row = [&](const char *name, const char *body) {
            QTest::newRow(name) << (QByteArray("{\"unitNames\":{\"COMBI A001\":{\"name\":\"x\",") + body + "}}}");
        };
        row("category number", "\"category\":5,\"readAt\":\"2026-01-01T00:00:00Z\"");
        row("category null", "\"category\":null,\"readAt\":\"2026-01-01T00:00:00Z\"");
        row("readAt missing", "\"category\":\"Pad\"");
        row("readAt garbage", "\"readAt\":\"never\"");
    }
    void unitNamesInvalidSidelined() {
        QFETCH(QByteArray, content);
        checkSidelined(content);
    }

    // Item 5: unit with a missing or bad version or lastSeen. Absent unit is fine.
    void unitInvalidSidelined_data() {
        QTest::addColumn<QByteArray>("content");
        auto row = [&](const char *name, const char *body) {
            QTest::newRow(name) << (QByteArray("{\"unit\":{\"memberCode\":1,\"globalChannel\":1,\"bankMap\":\"korg\",")
                                    + body + "}}");
        };
        row("version missing", "\"lastSeen\":\"2026-01-01T00:00:00Z\"");
        row("version number", "\"version\":1,\"lastSeen\":\"2026-01-01T00:00:00Z\"");
        row("lastSeen missing", "\"version\":\"1\"");
        row("lastSeen garbage", "\"version\":\"1\",\"lastSeen\":\"x\"");
    }
    void unitInvalidSidelined() {
        QFETCH(QByteArray, content);
        checkSidelined(content);
    }
    void unitAbsentIsFine() {
        QTemporaryDir d;
        writeAll(d.filePath(QStringLiteral("library.json")), "{\"favourites\":[]}");
        Library lib;
        QVERIFY(lib.load(d.path()));
        QVERIFY(!lib.unit().has_value());
        QVERIFY(brokenFiles(d.path()).isEmpty());
    }

    // Item 6: huge or non-integral numbers are structural errors, not UB.
    void hugeNumbersSidelined_data() {
        QTest::addColumn<QByteArray>("content");
        QTest::newRow("size 1e300") << QByteArray("{\"sets\":[{\"id\":\"a\",\"name\":\"n\",\"size\":1e300,\"slots\":[]}]}");
        QTest::newRow("count 1e300") << QByteArray(
            "{\"recents\":[{\"address\":\"COMBI A001\",\"count\":1e300,\"lastUsed\":\"2026-01-01T00:00:00Z\"}]}");
        QTest::newRow("count 2^60") << QByteArray(
            "{\"recents\":[{\"address\":\"COMBI A001\",\"count\":1152921504606846976,\"lastUsed\":\"2026-01-01T00:00:00Z\"}]}");
        QTest::newRow("channel -1e300") << QByteArray(
            "{\"unit\":{\"memberCode\":1,\"version\":\"1\",\"globalChannel\":-1e300,\"bankMap\":\"korg\","
            "\"lastSeen\":\"2026-01-01T00:00:00Z\"}}");
    }
    void hugeNumbersSidelined() {
        QFETCH(QByteArray, content);
        checkSidelined(content);
    }

    // Item 1: an unreadable file is not corruption.
    void unreadableFileNotSidelined() {
        QTemporaryDir d;
        const QString path = d.filePath(QStringLiteral("library.json"));
        const QByteArray content = "{\"favourites\":[\"COMBI A001\"]}";
        writeAll(path, content);
        QVERIFY(QFile::setPermissions(path, QFileDevice::WriteOwner));
        QFile probe(path);
        if (probe.open(QIODevice::ReadOnly)) {
            probe.close();
            QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            QSKIP("file stays readable (running as a privileged user)");
        }
        Library lib;
        QVERIFY(!lib.load(d.path()));
        QVERIFY(lib.loadError().contains(QStringLiteral("Cannot read")));
        QVERIFY(brokenFiles(d.path()).isEmpty());
        QVERIFY(QFileInfo::exists(path));
        lib.setFavourite(combi(2), true);
        QVERIFY(!lib.save().isEmpty());  // writes blocked
        lib.flush();
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QCOMPARE(readAll(path), content);
    }

    // Item 7: an empty config directory must never write a relative library.json.
    void emptyConfigDirRefused() {
        QTemporaryDir cwd;
        const QString old = QDir::currentPath();
        QVERIFY(QDir::setCurrent(cwd.path()));
        {
            Library lib;
            QVERIFY(lib.load(QString()));
            QVERIFY(!lib.lastSaveError().isEmpty());
            lib.setFavourite(combi(1), true);
            QVERIFY(!lib.save().isEmpty());
            lib.flush();
        }  // destructor flush must not write either
        QDir::setCurrent(old);
        QVERIFY(!QFileInfo::exists(cwd.filePath(QStringLiteral("library.json"))));
        QVERIFY(QDir(cwd.path()).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty());
    }

    // Item 8: entries dropped at load: original copied once to .bak-<stamp> before the first save.
    void backupBeforeFirstSaveAfterWarnings() {
        QTemporaryDir d;
        const QString path = d.filePath(QStringLiteral("library.json"));
        const QByteArray content = "{\"favourites\":[\"COMBI A001\",\"nonsense\"]}";
        writeAll(path, content);
        // A pre-existing .bak with the same stamp must not be overwritten.
        const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss"));
        const QString taken = path + QStringLiteral(".bak-") + stamp;
        writeAll(taken, "old backup");
        Library lib;
        QVERIFY(lib.load(d.path()));
        QCOMPARE(lib.loadWarnings().size(), 1);
        QVERIFY(QDir(d.path()).entryList({QStringLiteral("library.json.bak-*")}, QDir::Files).size() == 1);
        lib.setFavourite(combi(2), true);
        lib.flush();
        lib.setFavourite(combi(3), true);
        lib.flush();  // second save: no second backup
        const QStringList baks = QDir(d.path()).entryList({QStringLiteral("library.json.bak-*")}, QDir::Files);
        QVERIFY2(baks.size() <= 2, qPrintable(baks.join(QLatin1Char(' '))));
        int original = 0;
        for (const QString &b : baks) {
            const QByteArray c = readAll(d.filePath(b));
            if (c == content)
                ++original;
            else
                QCOMPARE(c, QByteArray("old backup"));
        }
        QCOMPARE(original, 1);
        QCOMPARE(readAll(taken), QByteArray("old backup"));
        QVERIFY(readAll(path) != content);
        // A clean load makes no backup.
        QTemporaryDir d2;
        Library clean;
        clean.load(d2.path());
        clean.setFavourite(combi(1), true);
        clean.flush();
        Library clean2;
        QVERIFY(clean2.load(d2.path()));
        clean2.setFavourite(combi(2), true);
        clean2.flush();
        QVERIFY(QDir(d2.path()).entryList({QStringLiteral("library.json.bak-*")}, QDir::Files).isEmpty());
    }

    // Item 9: a pending change is written when the Library is destroyed.
    void destructorFlushes() {
        QTemporaryDir d;
        {
            Library lib;
            lib.load(d.path());
            lib.setFavourite(combi(5), true);
        }
        Library again;
        QVERIFY(again.load(d.path()));
        QVERIFY(again.isFavourite(combi(5)));
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
        const QDateTime base(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::utc());
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

private:
    void checkSidelined(const QByteArray &content) {
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
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "test_library.moc"
