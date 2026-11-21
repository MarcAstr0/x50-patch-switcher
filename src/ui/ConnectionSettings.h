#pragma once

#include <QString>

#include "core/BankMap.h"

class X50Link;

// What the settings dialog remembers (SPEC 4.5): the MIDI output and input port by name, and
// the manual channel and Bank Map used when the X50 gives no answer. Stored as
// "<configDir>/settings.ini" (QSettings, INI format, a sibling of window.ini and the library),
// so portable mode keeps it next to the exe and nothing goes to the registry.
struct ConnectionSettings
{
    QString outputPort;  // as listed by the backend; empty: not chosen yet
    QString inputPort;
    int channel = 1;     // 1-16
    BankMap bankMap = BankMap::Gm;

    static QString fileName() { return QStringLiteral("settings.ini"); }
    // Missing or unreadable file, or values out of range: defaults for those fields.
    static ConnectionSettings load(const QString &configDir);
    // False when the file could not be written (read-only folder). Empty configDir: nothing written, true.
    bool save(const QString &configDir) const;
    // Port names (when chosen), manual channel and Bank Map go to the link.
    void applyTo(X50Link &link) const;
};
