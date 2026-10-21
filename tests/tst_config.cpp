// SPDX-License-Identifier: MIT
#include "config.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using clorela::Config;

class TstConfig : public QObject
{
    Q_OBJECT

private slots:
    void defaultsWhenFileMissing()
    {
        const Config c = Config::load(QStringLiteral("/nonexistent/clorela.conf"));
        QVERIFY(c.sourcePath.isEmpty());
        QCOMPARE(c.sensor, QStringLiteral("i2c"));
        QCOMPARE(c.nodeAddresses, QList<int>{0x10});
        QCOMPARE(c.leds.size(), 12);
        QVERIFY(c.requireNetwork);
    }

    void parseIntList()
    {
        QCOMPARE(clorela::parseIntList(QStringLiteral("0x10, 0x11")), (QList<int>{16, 17}));
        QCOMPARE(clorela::parseIntList(QStringLiteral("0-3,7")), (QList<int>{0, 1, 2, 3, 7}));
        QCOMPARE(clorela::parseIntList(QStringLiteral("x, 5")), QList<int>{5});
    }

    void readsShippedConfig()
    {
        // The file installed to /etc must parse to the documented values.
        const Config c = Config::load(QStringLiteral(CLORELA_SOURCE_DIR "/data/clorela.conf"));
        QVERIFY(!c.sourcePath.isEmpty());
        QCOMPARE(c.sensor, QStringLiteral("i2c"));
        QCOMPARE(c.nodeAddresses, QList<int>{0x10});
        QCOMPARE(c.leds, (QList<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}));
        QCOMPARE(c.intervalSeconds, 60);
        QCOMPARE(c.displayBackend, QStringLiteral("ssd1306"));
        QCOMPARE(c.displayAddress, 0x3C);
        QCOMPARE(c.language, QStringLiteral("es"));
        QVERIFY(c.requireNetwork);
        QVERIFY(c.fullscreen);
    }

    void unquotedListAndHex()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("c.conf"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[acquisition]\nnode_addresses=0x10,0x12\nsensor=SIM\ninterval_s=1\n"
                "[display]\naddress=0x3D\n");
        f.close();
        const Config c = Config::load(path);
        QCOMPARE(c.nodeAddresses, (QList<int>{0x10, 0x12}));
        QCOMPARE(c.sensor, QStringLiteral("sim"));
        QCOMPARE(c.intervalSeconds, 5); // clamped to the minimum
        QCOMPARE(c.displayAddress, 0x3D);
    }
};

QTEST_GUILESS_MAIN(TstConfig)
#include "tst_config.moc"
