// SPDX-License-Identifier: MIT
#include "config.h"

#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace clorela {

namespace {

int parseInt(const QString &text, bool *ok)
{
    const QString t = text.trimmed();
    if (t.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        return t.mid(2).toInt(ok, 16);
    return t.toInt(ok, 10);
}

int readInt(QSettings &s, const QString &key, int fallback)
{
    if (!s.contains(key))
        return fallback;
    bool ok = false;
    const int v = parseInt(s.value(key).toString(), &ok);
    return ok ? v : fallback;
}

bool readBool(QSettings &s, const QString &key, bool fallback)
{
    if (!s.contains(key))
        return fallback;
    const QString v = s.value(key).toString().trimmed().toLower();
    if (v == QLatin1String("true") || v == QLatin1String("1") || v == QLatin1String("yes"))
        return true;
    if (v == QLatin1String("false") || v == QLatin1String("0") || v == QLatin1String("no"))
        return false;
    return fallback;
}

QString readString(QSettings &s, const QString &key, const QString &fallback)
{
    const QString v = s.value(key, fallback).toString().trimmed();
    return v.isEmpty() ? fallback : v;
}

QList<int> readList(QSettings &s, const QString &key, const QList<int> &fallback)
{
    if (!s.contains(key))
        return fallback;
    // QSettings splits unquoted commas into a QStringList; join it back.
    const QVariant raw = s.value(key);
    const QString text = raw.typeId() == QMetaType::QStringList
                             ? raw.toStringList().join(QLatin1Char(','))
                             : raw.toString();
    const QList<int> parsed = parseIntList(text);
    return parsed.isEmpty() ? fallback : parsed;
}

} // namespace

QList<int> parseIntList(const QString &text)
{
    QList<int> out;
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString p = part.trimmed();
        const int dash = p.indexOf(QLatin1Char('-'), 1);
        bool ok1 = false;
        if (dash > 0) {
            bool ok2 = false;
            const int a = parseInt(p.left(dash), &ok1);
            const int b = parseInt(p.mid(dash + 1), &ok2);
            if (ok1 && ok2 && a <= b) {
                for (int v = a; v <= b; ++v)
                    out.append(v);
            }
            continue;
        }
        const int v = parseInt(p, &ok1);
        if (ok1)
            out.append(v);
    }
    return out;
}

QString Config::resolvePath(const QString &explicitPath)
{
    if (!explicitPath.isEmpty())
        return explicitPath;
    const QString env = qEnvironmentVariable("CLORELA_CONFIG");
    if (!env.isEmpty())
        return env;
    return QStringLiteral("/etc/clorela/clorela.conf");
}

Config Config::load(const QString &explicitPath)
{
    Config c;
    const QString path = resolvePath(explicitPath);
    if (!QFileInfo::exists(path))
        return c;

    QSettings s(path, QSettings::IniFormat);
    c.sourcePath = path;

    c.databasePath = readString(s, QStringLiteral("storage/database"), c.databasePath);

    c.sensor = readString(s, QStringLiteral("acquisition/sensor"), c.sensor).toLower();
    c.i2cBus = readInt(s, QStringLiteral("acquisition/i2c_bus"), c.i2cBus);
    c.nodeAddresses = readList(s, QStringLiteral("acquisition/node_addresses"), c.nodeAddresses);
    c.intervalSeconds = qMax(5, readInt(s, QStringLiteral("acquisition/interval_s"), c.intervalSeconds));
    c.leds = readList(s, QStringLiteral("acquisition/leds"), c.leds);
    c.requireNetwork = readBool(s, QStringLiteral("acquisition/require_network"), c.requireNetwork);
    c.autostart = readBool(s, QStringLiteral("acquisition/autostart"), c.autostart);
    c.controlSocket = readString(s, QStringLiteral("acquisition/control_socket"), c.controlSocket);

    c.displayBackend = readString(s, QStringLiteral("display/backend"), c.displayBackend).toLower();
    c.displayI2cBus = readInt(s, QStringLiteral("display/i2c_bus"), c.displayI2cBus);
    c.displayAddress = readInt(s, QStringLiteral("display/address"), c.displayAddress);

    c.fullscreen = readBool(s, QStringLiteral("ui/fullscreen"), c.fullscreen);
    c.language = readString(s, QStringLiteral("ui/language"), c.language).toLower();
    c.uiStatePath = readString(s, QStringLiteral("ui/state"), c.uiStatePath);

    // Relative paths (used by data/dev.conf) are relative to the working
    // directory. QLocalServer would otherwise put a relative socket name in /tmp.
    for (QString *path : {&c.databasePath, &c.controlSocket, &c.uiStatePath})
        *path = QFileInfo(*path).absoluteFilePath();
    return c;
}

} // namespace clorela
