#pragma once

#include <QString>

// Application name and version, shared by the window title and the command line.
class Version
{
public:
    static QString appName();
    static QString appVersion();
};
