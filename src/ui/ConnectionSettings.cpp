#include "ui/ConnectionSettings.h"

#include "midi/X50Link.h"

#include <QDir>
#include <QSettings>

ConnectionSettings ConnectionSettings::load(const QString &configDir)
{
    ConnectionSettings s;
    if (configDir.isEmpty())
        return s;
    const QSettings in(QDir(configDir).filePath(fileName()), QSettings::IniFormat);
    s.outputPort = in.value(QStringLiteral("midi/output")).toString();
    s.inputPort = in.value(QStringLiteral("midi/input")).toString();
    bool ok = false;
    const int ch = in.value(QStringLiteral("manual/channel")).toInt(&ok);
    if (ok && ch >= 1 && ch <= 16)
        s.channel = ch;
    const QString map = in.value(QStringLiteral("manual/bankMap")).toString();
    if (map == QLatin1String("korg"))
        s.bankMap = BankMap::Korg;
    return s;
}

bool ConnectionSettings::save(const QString &configDir) const
{
    if (configDir.isEmpty())
        return true;
    QDir().mkpath(configDir);
    QSettings out(QDir(configDir).filePath(fileName()), QSettings::IniFormat);
    out.setValue(QStringLiteral("midi/output"), outputPort);
    out.setValue(QStringLiteral("midi/input"), inputPort);
    out.setValue(QStringLiteral("manual/channel"), channel);
    out.setValue(QStringLiteral("manual/bankMap"), bankMap == BankMap::Korg ? QStringLiteral("korg") : QStringLiteral("gm"));
    out.sync();
    return out.status() == QSettings::NoError;
}

void ConnectionSettings::applyTo(X50Link &link) const
{
    if (!outputPort.isEmpty())
        link.setPortNames(outputPort, inputPort);
    link.setManualChannel(channel);
    link.setManualBankMap(bankMap);
}
