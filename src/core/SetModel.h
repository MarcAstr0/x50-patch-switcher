#pragma once

#include <optional>

#include <QString>
#include <QVector>

#include "core/SoundAddress.h"

// One set (SPEC 3.3): id, name, size 6 or 8, and `size` slots. A plain value
// type, not a QAbstractItemModel: the set editor and the live grid are fixed
// 2x3 / 2x4 grids of at most 8 cells, so a value read through slots() is the
// simplest thing both can use; Library wraps every mutation (by set id) so
// that changes are signalled and saved. Each operation returns an Result:
// ok, or an error message fit to show to the user. Failed operations leave
// the set unchanged.
class SetModel {
public:
    struct Slot {
        std::optional<SoundAddress> address;  // nullopt = empty slot
        QString label;                         // empty = none; only with an address
        bool isEmpty() const { return !address.has_value(); }
    };
    struct Result {
        QString error;
        bool ok() const { return error.isEmpty(); }
    };

    SetModel() = default;
    SetModel(const QString &id, const QString &name, int size);

    static bool isValidSize(int size) { return size == 6 || size == 8; }

    const QString &id() const { return m_id; }
    const QString &name() const { return m_name; }
    int size() const { return m_slots.size(); }
    const QVector<Slot> &allSlots() const { return m_slots; }
    // idx is 0-based here and in every operation (the UI shows idx + 1).
    // Out of range (including negative) returns an empty Slot; never reads past the end.
    const Slot &slot(int idx) const;

    Result rename(const QString &name);
    // Resize 6 <-> 8. To 6 keeps slots 1-6 and refuses if 7 or 8 are filled.
    Result resize(int newSize);
    // Moves slot `from` onto `to`, address and label together. If `to` is
    // occupied the two slots swap (positions are fixed; nothing shifts).
    Result moveSlot(int from, int to);
    // Puts a sound in a slot; an existing label stays (it belongs to the
    // song, not the sound). Refuses an invalid address.
    Result setSlot(int idx, const SoundAddress &address);
    // Empties the slot, label included.
    Result clearSlot(int idx);
    // Sets the label; empty text clears it. Refuses on an empty slot.
    Result setLabel(int idx, const QString &label);

    // For Library's loader and duplicate; not validated beyond size.
    void setId(const QString &id) { m_id = id; }
    bool assignSlots(const QVector<Slot> &newSlots);

private:
    bool inRange(int idx) const { return idx >= 0 && idx < m_slots.size(); }

    QString m_id;
    QString m_name;
    QVector<Slot> m_slots;
};
