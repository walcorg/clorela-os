// SPDX-License-Identifier: MIT
// Runtime configuration shared by clorela-ui and clorela-acqd.
//
// Read from an INI file. Lookup order:
//   1. path given on the command line (--config)
//   2. $CLORELA_CONFIG
//   3. /etc/clorela/clorela.conf
// Missing keys fall back to the defaults below. Relative paths are resolved
// against the working directory. See data/clorela.conf.
#pragma once

#include <QList>
#include <QString>

namespace clorela {

struct Config
{
    // [storage]
    QString databasePath = QStringLiteral("/var/lib/clorela/measurements.db");

    // [acquisition]
    QString sensor = QStringLiteral("i2c");          // "i2c" or "sim"
    int i2cBus = 1;                                    // [TO VERIFY] on shield v4.1
    QList<int> nodeAddresses = {0x10};                 // 0x10..0x13, node-routine jumpers
    int intervalSeconds = 60;
    QList<int> leds = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    bool requireNetwork = true;                        // acquisition needs Wi-Fi
    bool autostart = false;                            // start sampling when the daemon starts
    QString controlSocket = QStringLiteral("/run/clorela/acqd.sock");

    // [display] status display on the shield
    QString displayBackend = QStringLiteral("ssd1306"); // "ssd1306", "console" or "none"
    int displayI2cBus = 1;                              // [TO VERIFY]
    int displayAddress = 0x3C;                          // [TO VERIFY] 0x3C or 0x3D

    // [signer] secure element used to sign capture receipts (Anchor Protocol).
    // Only "none" exists today; see docs/signer-tropic01.md.
    QString signerBackend = QStringLiteral("none");

    // [ui]
    bool fullscreen = true;
    QString language = QStringLiteral("es");            // "es", "en" or "fr"
    QString uiStatePath = QStringLiteral("/var/lib/clorela/ui.conf");

    // Path of the file this configuration was read from (empty = defaults only).
    QString sourcePath;

    static Config load(const QString &explicitPath = QString());
    static QString resolvePath(const QString &explicitPath);
};

// Parses "0x10, 0x11" or "16,17" or "0-11". Invalid entries are skipped.
QList<int> parseIntList(const QString &text);

} // namespace clorela
