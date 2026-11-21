#pragma once

#include <QStringList>

class CommandLine
{
public:
    // Parses argv-style arguments. Returns true if the GUI should start;
    // false if a flag was handled and the process should exit with exitCode().
    bool process(const QStringList &arguments);
    int exitCode() const { return m_exitCode; }

private:
    int m_exitCode = 0;
};
