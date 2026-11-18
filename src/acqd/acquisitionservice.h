// SPDX-License-Identifier: MIT
// Acquisition loop of clorela-acqd. Lives in its own thread so I2C waits never
// block the control socket.
//
// Each cycle, for every configured node:
//   1. 'G' darkness check            -> one "dark" record
//   2. 'H', n for each LED in `leds`  -> one "led" record per LED
// Nodes in UVC disinfection are skipped. Without network (and with
// require_network=true) no cycle runs. [TO VERIFY] This default routine is a
// starting point; the scientific protocol is not defined yet.
#pragma once

#include "config.h"
#include "nodeclient.h"
#include "measurement.h"
#include "measurementstore.h"
#include "nodetransport.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <functional>
#include <memory>

class QTimer;

namespace clorela {

class Signer;

struct ServiceSnapshot
{
    enum class State {
        Idle,           // not acquiring (stopped)
        Waiting,        // acquiring, between cycles
        Sampling,       // a cycle is running
        WaitingNetwork, // acquiring but blocked: no Wi-Fi
        Sterilizing,    // at least one node is in UVC disinfection
        Error,          // no node answered in the last cycle
    };

    State state = State::Idle;
    bool running = false;
    bool online = false;
    QString detail;
    qint64 cycles = 0;          // cycles completed since the daemon started
    qint64 lastCycleId = 0;
    qint64 lastCycleMs = 0;     // UTC ms of the last completed cycle
    int nextCycleInS = -1;
    int uvSecondsRemaining = 0;

    static QString stateName(State s);
};

class AcquisitionService : public QObject
{
    Q_OBJECT

public:
    using NetworkProbe = std::function<bool()>;

    AcquisitionService(const Config &config, std::unique_ptr<NodeTransport> transport,
                       QObject *parent = nullptr);
    ~AcquisitionService() override;

    // For tests: replace the routing-table check.
    void setNetworkProbe(NetworkProbe probe) { m_networkProbe = std::move(probe); }
    // Signer used for Laura Encoding records (not owned). Null = unsigned.
    void setSigner(Signer *signer) { m_signer = signer; }
    ServiceSnapshot snapshot() const { return m_snapshot; }
    QString storeError() const { return m_storeError; }

public slots:
    // Opens the database and starts the housekeeping timer. Call from the
    // service thread (queued) once it is running.
    void initialize();
    void start();
    void stop();
    void sterilize(int seconds);
    void stopSterilization();
    // Runs one cycle now (ignores the interval). Returns the stored readings.
    QList<clorela::Measurement> runCycle();
    // Laura Encoding (docs/laura-encoding.md). Run in the service thread, between
    // cycles; return {"ok":true,...record} or {"ok":false,"error":...}.
    QJsonObject lauraCalibrate(int address);
    QJsonObject lauraChallenge(int address, const QByteArray &challengeId,
                               const clorela::NodeClient::Levels &levels);
    void shutdown();

signals:
    void snapshotChanged(const clorela::ServiceSnapshot &snapshot);
    void logMessage(const QString &message);

private slots:
    void tick();

private:
    void publish();
    bool pollUv();

    Config m_config;
    std::unique_ptr<NodeTransport> m_transport;
    MeasurementStore m_store;
    QString m_storeError;
    QTimer *m_timer = nullptr;
    QElapsedTimer m_sinceLastCycle;
    QElapsedTimer m_sinceUvPoll;
    NetworkProbe m_networkProbe;
    Signer *m_signer = nullptr;
    ServiceSnapshot m_snapshot;
};

} // namespace clorela

Q_DECLARE_METATYPE(clorela::ServiceSnapshot)
