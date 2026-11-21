#pragma once

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

private:
    int m_exitCode = 0;
};
