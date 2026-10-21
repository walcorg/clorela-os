// SPDX-License-Identifier: MIT
#include "acquisitionservice.h"
#include "controlserver.h"
#include "measurementstore.h"
#include "simulatednode.h"

#include <QJsonDocument>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

using namespace clorela;
using State = ServiceSnapshot::State;

class TstAcquisition : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    Config config(const QList<int> &nodes = {0x10})
    {
        Config c;
        c.sensor = QStringLiteral("sim");
        c.nodeAddresses = nodes;
        c.databasePath = m_dir.filePath(QStringLiteral("m-%1.db").arg(QTest::currentTestFunction()));
        c.controlSocket = m_dir.filePath(QStringLiteral("acqd-%1.sock").arg(QTest::currentTestFunction()));
        c.intervalSeconds = 5;
        c.autostart = false;
        return c;
    }

private slots:
    void oneCycleStoresDarkAndTwelveLeds()
    {
        const Config c = config();
        AcquisitionService svc(c, std::make_unique<SimulatedNodeBus>(c.nodeAddresses));
        svc.setNetworkProbe([] { return true; });
        svc.initialize();
        const QList<Measurement> cycle = svc.runCycle();
        QCOMPARE(cycle.size(), 13);
        QCOMPARE(cycle.first().kind, Measurement::Kind::Dark);
        QCOMPARE(cycle.at(12).kind, Measurement::Kind::Led);
        QCOMPARE(cycle.at(12).param, 11);
        QCOMPARE(svc.snapshot().lastCycleId, 1);
        QCOMPARE(svc.snapshot().state, State::Waiting);
        svc.runCycle();
        QCOMPARE(svc.snapshot().lastCycleId, 2);
        svc.shutdown();

        MeasurementStore store;
        QVERIFY(store.open(c.databasePath));
        QCOMPARE(store.count(), 26);
        QCOMPARE(store.latestCycle().size(), 13);
    }

    void twoNodes()
    {
        const Config c = config({0x10, 0x11});
        AcquisitionService svc(c, std::make_unique<SimulatedNodeBus>(c.nodeAddresses));
        svc.setNetworkProbe([] { return true; });
        svc.initialize();
        QCOMPARE(svc.runCycle().size(), 26);
    }

    void missingNodeIsReportedButOthersContinue()
    {
        Config c = config({0x10, 0x12});
        // The bus only has 0x10.
        AcquisitionService svc(c, std::make_unique<SimulatedNodeBus>(QList<int>{0x10}));
        svc.setNetworkProbe([] { return true; });
        QSignalSpy logSpy(&svc, &AcquisitionService::logMessage);
        svc.initialize();
        QCOMPARE(svc.runCycle().size(), 13);
        bool mentioned = false;
        for (const auto &args : logSpy)
            mentioned |= args.at(0).toString().contains(QStringLiteral("0x12"));
        QVERIFY(mentioned);
        QCOMPARE(svc.snapshot().state, State::Waiting);
    }

    void noNodesIsAnError()
    {
        Config c = config({0x13});
        AcquisitionService svc(c, std::make_unique<SimulatedNodeBus>(QList<int>{0x10}));
        svc.setNetworkProbe([] { return true; });
        svc.initialize();
        QVERIFY(svc.runCycle().isEmpty());
        QCOMPARE(svc.snapshot().state, State::Error);
    }

    void requiresNetwork()
    {
        Config c = config();
        c.requireNetwork = true;
        AcquisitionService svc(c, std::make_unique<SimulatedNodeBus>(c.nodeAddresses));
        bool online = false;
        svc.setNetworkProbe([&online] { return online; });
        svc.initialize();
        svc.start();
        QCOMPARE(svc.snapshot().state, State::WaitingNetwork);
        QCOMPARE(svc.snapshot().lastCycleId, 0);

        online = true;
        QTRY_COMPARE_WITH_TIMEOUT(svc.snapshot().lastCycleId, 1, 3000);
        svc.stop();
        QCOMPARE(svc.snapshot().state, State::Idle);
    }

    void networkNotRequired()
    {
        Config c = config();
        c.requireNetwork = false;
        AcquisitionService svc(c, std::make_unique<SimulatedNodeBus>(c.nodeAddresses));
        svc.setNetworkProbe([] { return false; });
        svc.initialize();
        svc.start();
        QCOMPARE(svc.snapshot().lastCycleId, 1); // first cycle runs immediately
        QVERIFY(!svc.snapshot().online);
    }

    void sterilizingStopsSampling()
    {
        const Config c = config();
        AcquisitionService svc(c, std::make_unique<SimulatedNodeBus>(c.nodeAddresses));
        svc.setNetworkProbe([] { return true; });
        svc.initialize();
        svc.start();
        QVERIFY(svc.snapshot().running);
        svc.sterilize(60);
        QVERIFY(!svc.snapshot().running);
        QCOMPARE(svc.snapshot().state, State::Sterilizing);
        QVERIFY(svc.snapshot().uvSecondsRemaining > 55);
        QVERIFY(svc.runCycle().isEmpty()); // node in UVC is skipped
        svc.stopSterilization();
        QCOMPARE(svc.snapshot().state, State::Idle);
    }

    void controlSocketEndToEnd()
    {
        const Config c = config();
        QThread worker;
        auto *svc = new AcquisitionService(c, std::make_unique<SimulatedNodeBus>(c.nodeAddresses));
        svc->setNetworkProbe([] { return true; });
        svc->moveToThread(&worker);
        connect(&worker, &QThread::finished, svc, &QObject::deleteLater);

        ControlServer server(svc, c);
        connect(svc, &AcquisitionService::snapshotChanged, &server, &ControlServer::setSnapshot);
        QVERIFY2(server.listen(c.controlSocket), qPrintable(server.errorString()));
        worker.start();
        QMetaObject::invokeMethod(svc, &AcquisitionService::initialize, Qt::QueuedConnection);

        QLocalSocket client;
        client.connectToServer(c.controlSocket);
        QVERIFY(client.waitForConnected(2000));

        // The server lives in this thread, so wait with the event loop running
        // (blocking waitForReadyRead would starve it).
        auto ask = [&client](const QByteArray &line) {
            client.write(line + '\n');
            client.flush();
            if (!QTest::qWaitFor([&client] { return client.canReadLine(); }, 3000))
                return QJsonObject();
            return QJsonDocument::fromJson(client.readLine()).object();
        };

        QTRY_COMPARE(ask(R"({"cmd":"status"})").value(QStringLiteral("state")).toString(), QStringLiteral("idle"));
        QVERIFY(ask(R"({"cmd":"start"})").value(QStringLiteral("ok")).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(ask(R"({"cmd":"status"})").value(QStringLiteral("last_cycle")).toInt(), 1, 5000);
        const QJsonObject st = ask(R"({"cmd":"status"})");
        QCOMPARE(st.value(QStringLiteral("sensor")).toString(), QStringLiteral("sim"));
        QVERIFY(st.value(QStringLiteral("running")).toBool());

        QVERIFY(!ask(R"({"cmd":"fly"})").value(QStringLiteral("ok")).toBool());
        QVERIFY(!ask("not json").value(QStringLiteral("ok")).toBool());
        QVERIFY(!ask(R"({"cmd":"sterilize","seconds":5000})").value(QStringLiteral("ok")).toBool());

        QVERIFY(ask(R"({"cmd":"stop"})").value(QStringLiteral("ok")).toBool());
        QTRY_COMPARE(ask(R"({"cmd":"status"})").value(QStringLiteral("state")).toString(), QStringLiteral("idle"));

        QMetaObject::invokeMethod(svc, &AcquisitionService::shutdown, Qt::BlockingQueuedConnection);
        worker.quit();
        worker.wait();
    }
};

QTEST_GUILESS_MAIN(TstAcquisition)
#include "tst_acquisition.moc"
