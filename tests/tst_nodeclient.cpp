// SPDX-License-Identifier: MIT
// NodeClient against the simulated node: checks that both speak the
// node-routine protocol byte for byte.
#include "nodeclient.h"
#include "simulatednode.h"

#include <QTest>
#include <QThread>

using namespace clorela;

class TstNodeClient : public QObject
{
    Q_OBJECT

private slots:
    void statusBytes()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        const auto s = c.status();
        QVERIFY(s);
        QCOMPARE(s->state, node::StateIdle);
        QCOMPARE(s->firmwareVersion, uint8_t(1));
        QCOMPARE(s->address, uint8_t(0x10));
        QCOMPARE(s->error, node::ErrNone);
    }

    void parseStatusLayout()
    {
        const QByteArray raw("\x02\x09\x45\x00\x01\x2C\x01\x11", 8);
        const node::Status s = NodeClient::parseStatus(raw);
        QCOMPARE(s.state, node::StateUv);
        QVERIFY(s.has(node::FlagDataReady));
        QVERIFY(s.has(node::FlagUvActive));
        QCOMPARE(s.lastCommand, 'E');
        QCOMPARE(s.uvSecondsRemaining, uint16_t(300));
        QCOMPARE(s.address, uint8_t(0x11));
    }

    void parsePageIsLittleEndian()
    {
        QByteArray raw(32, '\0');
        raw[0] = char(0xFF);
        raw[1] = char(0x03); // 1023
        raw[30] = char(0x34);
        raw[31] = char(0x12);
        const auto r = NodeClient::parsePage(raw);
        QCOMPARE(r[0], quint16(1023));
        QCOMPARE(r[15], quint16(0x1234));
    }

    void darkCheckAndLeds()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        node::Status after;
        const auto dark = c.darkCheck(&after);
        QVERIFY2(dark, qPrintable(c.lastError()));
        QVERIFY(!after.has(node::FlagDarkAlert));
        for (quint16 v : *dark)
            QVERIFY(v < 20);

        const auto led3 = c.measureLed(3, &after);
        QVERIFY(led3);
        QVERIFY(after.has(node::FlagDataReady));
        // The sensor facing LED 3 on the transmission wall (T3 = channel 11)
        // sees the most light.
        for (int ch = 8; ch < 16; ++ch)
            if (ch != 11)
                QVERIFY((*led3)[11] > (*led3)[ch]);
        QVERIFY((*led3)[11] > 100);
    }

    void lightLeakRaisesDarkAlert()
    {
        SimulatedNodeBus bus({0x10});
        bus.setLightLeak(0x10, true);
        NodeClient c(&bus, 0x10);
        node::Status after;
        QVERIFY(c.darkCheck(&after));
        QVERIFY(after.has(node::FlagDarkAlert));
    }

    void invalidArguments()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        QVERIFY(!c.measureLed(12)); // rejected by the client
        QVERIFY(c.command(node::CmdSingleLed, QByteArray(1, char(12))));
        const auto s = c.status();
        QCOMPARE(s->error, node::ErrInvalidArgument);
        QVERIFY(c.command('Z'));
        QCOMPARE(c.status()->error, node::ErrUnknownCommand);
        QVERIFY(!c.startUv(0));
        QVERIFY(!c.startUv(1801));
    }

    void uvBlocksOtherCommands()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        QVERIFY(c.startUv(60));
        auto s = c.status();
        QCOMPARE(s->state, node::StateUv);
        QVERIFY(s->has(node::FlagUvActive));
        QVERIFY(s->uvSecondsRemaining > 58);

        QVERIFY(!c.darkCheck());
        QVERIFY(c.lastError().contains(QStringLiteral("blocked by active UVC"))
                || c.lastError().contains(QStringLiteral("sterilizing")));

        QVERIFY(c.stopUv());
        s = c.status();
        QCOMPARE(s->state, node::StateIdle);
        QCOMPARE(s->error, node::ErrAborted);
        QVERIFY(c.darkCheck());
    }

    void uvExpires()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        QVERIFY(c.startUv(1));
        QCOMPARE(c.status()->state, node::StateUv);
        QThread::msleep(1100);
        QCOMPARE(c.status()->state, node::StateIdle);
    }

    void safeModeStopsUv()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        QVERIFY(c.startUv(30));
        QVERIFY(c.safeMode());
        QCOMPARE(c.status()->state, node::StateIdle);
    }

    void unknownAddressNacks()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x13);
        QVERIFY(!c.status());
        QVERIFY(c.lastError().contains(QStringLiteral("0x13")));
    }

    void cultureDensityTrend()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        const auto first = c.measureLed(0);
        for (int i = 0; i < 400; ++i)
            c.measureLed(0);
        const auto later = c.measureLed(0);
        // Denser culture -> less light through it.
        QVERIFY((*later)[8] < (*first)[8]);
        QVERIFY(bus.opticalDensity(0x10) > 0.35);
    }
};

QTEST_GUILESS_MAIN(TstNodeClient)
#include "tst_nodeclient.moc"
