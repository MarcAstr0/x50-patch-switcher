#include "core/Version.h"

QString Version::appName()
{
    return QStringLiteral("X50 Patch Switcher");
}

QString Version::appVersion()
{
    return QStringLiteral(X50_VERSION);
}
