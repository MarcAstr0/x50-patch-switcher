#pragma once

#include <QString>
#include <QStringList>

class CommandLine
{
public:
    enum class Result { Gui, Exit };
    // Parses argv-style arguments (first element is the program name). Needs
    // no application object. Gui: start the GUI; Exit: a flag was handled and
    // the process should exit with exitCode().
    Result process(const QStringList &arguments);
    int exitCode() const { return m_exitCode; }
    // Value of --config-dir, empty if not given (SPEC 6).
    QString configDir() const { return m_configDir; }

private:
    int m_exitCode = 0;
    QString m_configDir;
};
