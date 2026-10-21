// SPDX-License-Identifier: MIT
#include "networkstatus.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>

namespace clorela {

bool NetworkStatus::hasDefaultRoute(const QString &procNetRoute)
{
    // Format: Iface Destination Gateway Flags RefCnt Use Metric Mask ...
    // Flags is hex; 0x0001 = RTF_UP.
    const QStringList lines = procNetRoute.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (int i = 1; i < lines.size(); ++i) {
        const QStringList f = lines.at(i).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (f.size() < 8)
            continue;
        if (f.at(0) == QLatin1String("lo"))
            continue;
        bool ok = false;
        const uint flags = f.at(3).toUInt(&ok, 16);
        if (f.at(1) == QLatin1String("00000000") && f.at(7) == QLatin1String("00000000") && ok && (flags & 0x1))
            return true;
    }
    return false;
}

bool NetworkStatus::isOnline()
{
    const QString forced = qEnvironmentVariable("CLORELA_NETWORK").toLower();
    if (forced == QLatin1String("online"))
        return true;
    if (forced == QLatin1String("offline"))
        return false;

    QFile f(QStringLiteral("/proc/net/route"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    return hasDefaultRoute(QString::fromLatin1(f.readAll()));
}

} // namespace clorela
