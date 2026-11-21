#pragma once

#include <QString>

// Where library.json lives (SPEC 7 item 2, SPEC 6 --config-dir). Pure: takes
// its inputs, so it is testable without moving the executable.
namespace ConfigLocation {

// Name of the marker file that makes the exe's folder the config directory.
QString portableMarkerName();

// Precedence: a non-empty cliDir wins (made absolute); else, if a file named
// "portable" exists in exeDir, exeDir; else appConfigDir (the caller passes
// QStandardPaths::AppConfigLocation).
QString resolve(const QString &cliDir, const QString &exeDir, const QString &appConfigDir);

// resolve() fed from the running application. Needs a QCoreApplication with
// organisation/application names already set.
QString resolveForApp(const QString &cliDir);

} // namespace ConfigLocation
