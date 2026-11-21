#pragma once

#include <functional>
#include <optional>

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "core/BankMap.h"
#include "core/Catalogue.h"
#include "core/SetModel.h"
#include "core/SoundAddress.h"

// The persistent library (SPEC 3.3): favourites, sets, recents, unit names and
// the last probed unit, in library.json in a config directory. No GUI, no MIDI.
//
// Persistence: QSaveFile (atomic); every change restarts a 500 ms single-shot
// timer and the save happens when it fires; flush() saves at once (exit).
// A file that fails to parse or validate is renamed library.json.broken-<UTC
// timestamp>, an empty library starts, loadError() says so, and nothing is
// written to the path until the next load, so a broken file is never lost.
//
// Choices where SPEC 3.3 is silent (builder's, flagged to the foreman):
//  - Unknown JSON fields are ignored (not preserved). No version field (the
//    spec has none).
//  - A stored address that does not parse does not sideline the file: the slot
//    (or favourite, recent, unit name) is dropped and loadWarnings() names it.
//  - Types: timestamps are ISO 8601 UTC strings; unit.memberCode int,
//    unit.version string, unit.globalChannel 1-16, unit.bankMap "korg"|"gm".
class Library : public QObject {
    Q_OBJECT
public:
    static constexpr int kRecentsCap = 50;
    static constexpr int kSaveDebounceMs = 500;

    struct Recent {
        SoundAddress address;
        int count = 0;
        QDateTime lastUsed;
    };
    struct UnitInfo {
        int memberCode = 0;
        QString version;
        int globalChannel = 0;
        BankMap bankMap = BankMap::Korg;
        QDateTime lastSeen;
    };
    using Result = SetModel::Result;

    explicit Library(QObject *parent = nullptr);

    // Reads <configDir>/library.json. A missing file or directory is not an
    // error (empty library; the directory is created on first save). Returns
    // true unless the file was broken (then loadError() is set).
    bool load(const QString &configDir);
    QString filePath() const { return m_path; }
    QString loadError() const { return m_loadError; }
    QStringList loadWarnings() const { return m_loadWarnings; }
    QString brokenFilePath() const { return m_brokenPath; }

    // Saving. save() writes now and returns an error string (empty = ok).
    QString save();
    // Saves now if a change is pending; stops the timer. Call on exit.
    void flush();
    bool hasPendingSave() const { return m_dirty; }
    QString lastSaveError() const { return m_saveError; }
    int writeCount() const { return m_writeCount; }

    // Favourites.
    bool isFavourite(const SoundAddress &a) const { return m_favourites.contains(a.format()); }
    QVector<SoundAddress> favourites() const;  // SoundAddress order
    void setFavourite(const SoundAddress &a, bool on);
    bool toggleFavourite(const SoundAddress &a);  // returns the new state

    // Recents: most recently used first, capped at kRecentsCap (oldest drop).
    const QVector<Recent> &recents() const { return m_recents; }
    int recentCount(const SoundAddress &a) const;
    void recordUse(const SoundAddress &a, const QDateTime &when = QDateTime::currentDateTimeUtc());

    // Unit names (SPEC 4.6) and last probe.
    const QMap<SoundAddress, Catalogue::UnitName> &unitNames() const { return m_unitNames; }
    void setUnitNames(const QMap<SoundAddress, Catalogue::UnitName> &names);
    const std::optional<UnitInfo> &unit() const { return m_unit; }
    void setUnit(const UnitInfo &info);

    // Sets.
    const QVector<SetModel> &sets() const { return m_sets; }
    const SetModel *setById(const QString &id) const;
    QString currentSetId() const { return m_currentSetId; }
    const SetModel *currentSet() const { return setById(m_currentSetId); }
    Result setCurrentSet(const QString &id);
    // Creates a set and makes it current. Returns its id via *newId.
    Result createSet(const QString &name, int size, QString *newId = nullptr);
    Result renameSet(const QString &id, const QString &name);
    // Copy named "<name> copy", inserted after the original; not made current.
    Result duplicateSet(const QString &id, QString *newId = nullptr);
    // If the current set is deleted, the neighbouring set becomes current.
    Result deleteSet(const QString &id);
    Result resizeSet(const QString &id, int newSize);
    Result moveSlot(const QString &id, int from, int to);
    Result setSlot(const QString &id, int idx, const SoundAddress &address);
    Result clearSlot(const QString &id, int idx);
    Result setLabel(const QString &id, int idx, const QString &label);

signals:
    void changed();                       // any content change
    void saved();                         // a write succeeded
    void saveFailed(const QString &error);

private:
    SetModel *mutableSet(const QString &id);
    Result applyToSet(const QString &id, const std::function<SetModel::Result(SetModel &)> &op);
    void touch();
    QByteArray toJson() const;
    bool fromJson(const QByteArray &data, QString *error);
    void clearContent();
    bool sideline();

    QString m_path;
    QString m_loadError;
    QStringList m_loadWarnings;
    QString m_brokenPath;
    QString m_saveError;
    bool m_dirty = false;
    bool m_writeBlocked = false;  // a broken file could not be moved aside
    int m_writeCount = 0;
    QTimer m_timer;

    QSet<QString> m_favourites;  // canonical format()
    QVector<Recent> m_recents;
    QMap<SoundAddress, Catalogue::UnitName> m_unitNames;
    std::optional<UnitInfo> m_unit;
    QVector<SetModel> m_sets;
    QString m_currentSetId;
};
