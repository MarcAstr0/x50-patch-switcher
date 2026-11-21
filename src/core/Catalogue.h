#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/SoundAddress.h"

// The sound catalogue (SPEC 3.2): factory names from data/x50-factory.tsv plus
// an optional per-address overlay of names read from the unit (SPEC 4.6).
// No GUI, no MIDI.
//
// Choices where the spec is silent (builder's, flagged to the foreman):
//  - Entries are kept in SoundAddress order (COMBI before PROG, bank, number).
//  - "Edited" means a unit name exists for the address and differs from the
//    factory name by exact string comparison after trimming trailing spaces
//    (the X50 pads names to 16 characters).
//  - effectiveName()/effectiveCategory() are what the UI shows and search
//    matches: the unit's value when an overlay exists (category falls back to
//    the factory one when the overlay has none), else the factory value.
//  - categories(kind) lists, for a kind, the unit's 16 category names once
//    setUnitCategories() has been called (in the unit's order, as read from the
//    global dump, MIDI.md 7 TABLE 4), else the distinct factory categories
//    sorted case-insensitively. The GM banks G and g(d) always keep their
//    factory categories (they are not part of the unit's 16 names) and are
//    not affected by the unit names. Parsing the dump is T1.6, not here.
class Catalogue {
public:
    struct Entry {
        SoundAddress address;
        QString name;      // factory name
        QString category;  // factory category
        QChar osc;         // 's', 'd', 'D' for programs; null QChar if blank
    };

    struct UnitName {
        QString name;
        QString category;  // may be empty
        QDateTime readAt;
    };

    // Replace the contents. Return false and fill *error (if given) on an
    // unreadable file or any malformed row; the catalogue is left empty then.
    bool loadFromFile(const QString &path, QString *error = nullptr);
    bool loadFromResource(QString *error = nullptr);  // :/data/x50-factory.tsv
    bool loadFromText(const QByteArray &tsv, QString *error = nullptr);

    const QVector<Entry> &entries() const { return m_entries; }
    int size() const { return m_entries.size(); }
    // nullptr if the address is not in the catalogue.
    const Entry *find(const SoundAddress &a) const;

    void setUnitName(const SoundAddress &a, const QString &name, const QString &category,
                     const QDateTime &readAt);
    void clearUnitNames();
    // nullptr if no overlay for the address.
    const UnitName *unitName(const SoundAddress &a) const;
    int unitNameCount() const { return m_unit.size(); }

    bool isEdited(const SoundAddress &a) const;
    QString effectiveName(const SoundAddress &a) const;
    QString effectiveCategory(const SoundAddress &a) const;

    // The unit's 16 program and 16 combination category names. Returns false
    // and fills *error (if given) unless both lists have exactly 16 entries;
    // nothing is changed then.
    bool setUnitCategories(const QStringList &program, const QStringList &combi,
                           QString *error = nullptr);
    void clearUnitCategories();
    bool hasUnitCategories() const { return m_haveUnitCats; }

    // Unit names for the kind once set, else the distinct factory categories, sorted.
    QStringList categories(SoundAddress::Kind kind) const;

private:
    int indexOf(const SoundAddress &a) const;
    static QString key(const SoundAddress &a) { return a.format(); }

    QVector<Entry> m_entries;
    QHash<QString, UnitName> m_unit;
    bool m_haveUnitCats = false;
    QStringList m_unitProgCats, m_unitCombiCats;
};
