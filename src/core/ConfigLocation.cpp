#include "core/ConfigLocation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

QString ConfigLocation::portableMarkerName()
{
    return QStringLiteral("portable");
}

QString ConfigLocation::resolve(const QString &cliDir, const QString &exeDir, const QString &appConfigDir)
{
    if (!cliDir.isEmpty())
        return QDir(cliDir).absolutePath();
    if (!exeDir.isEmpty() && QFileInfo(QDir(exeDir).filePath(portableMarkerName())).isFile())
        return QDir(exeDir).absolutePath();
    return appConfigDir;
}

QString ConfigLocation::resolveForApp(const QString &cliDir)
{
    return resolve(cliDir, QCoreApplication::applicationDirPath(),
                   QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
}
