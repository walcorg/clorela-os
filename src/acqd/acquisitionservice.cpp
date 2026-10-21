// SPDX-License-Identifier: MIT
#include "acquisitionservice.h"

#include "networkstatus.h"
#include "nodeclient.h"

#include <QDateTime>
#include <QTimer>

namespace clorela {

namespace {
constexpr int kTickMs = 1000;
constexpr int kUvPollMs = 5000;

QString hex(int address)
{
    return QStringLiteral("0x%1").arg(address, 2, 16, QLatin1Char('0'));
}
} // namespace

QString ServiceSnapshot::stateName(State s)
{
    switch (s) {
    case State::Idle: return QStringLiteral("idle");
    case State::Waiting: return QStringLiteral("waiting");
    case State::Sampling: return QStringLiteral("sampling");
    case State::WaitingNetwork: return QStringLiteral("waiting_network");
    case State::Sterilizing: return QStringLiteral("sterilizing");
    case State::Error: return QStringLiteral("error");
    }
    return QStringLiteral("idle");
}

AcquisitionService::AcquisitionService(const Config &config, std::unique_ptr<NodeTransport> transport,
                                       QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_transport(std::move(transport))
    , m_networkProbe([] { return NetworkStatus::isOnline(); })
{
    qRegisterMetaType<ServiceSnapshot>();
}

AcquisitionService::~AcquisitionService() = default;

void AcquisitionService::initialize()
{
    if (!m_store.open(m_config.databasePath)) {
        m_storeError = m_store.lastError();
        emit logMessage(QStringLiteral("cannot open %1: %2").arg(m_config.databasePath, m_storeError));
    }
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &AcquisitionService::tick);
    m_timer->start(kTickMs);
    m_snapshot.online = m_networkProbe();
    publish();
    if (m_config.autostart)
        start();
}

void AcquisitionService::start()
{
    if (m_snapshot.running)
        return;
    m_snapshot.running = true;
    m_sinceLastCycle.invalidate(); // first cycle on the next tick
    emit logMessage(QStringLiteral("acquisition started (%1, nodes %2, every %3 s)")
                        .arg(m_transport->name())
                        .arg([this] {
                            QStringList a;
                            for (int n : std::as_const(m_config.nodeAddresses))
                                a << hex(n);
                            return a.join(QLatin1Char(','));
                        }())
                        .arg(m_config.intervalSeconds));
    tick();
}

void AcquisitionService::stop()
{
    if (!m_snapshot.running)
        return;
    m_snapshot.running = false;
    for (int address : std::as_const(m_config.nodeAddresses)) {
        NodeClient client(m_transport.get(), address);
        client.allOff(); // leave the culture in the dark; errors are not fatal here
    }
    emit logMessage(QStringLiteral("acquisition stopped"));
    tick();
}

void AcquisitionService::sterilize(int seconds)
{
    if (m_snapshot.running)
        stop(); // never sample during UVC
    for (int address : std::as_const(m_config.nodeAddresses)) {
        NodeClient client(m_transport.get(), address);
        if (!client.startUv(seconds))
            emit logMessage(QStringLiteral("UVC on %1 failed: %2").arg(hex(address), client.lastError()));
    }
    m_sinceUvPoll.invalidate();
    tick();
}

void AcquisitionService::stopSterilization()
{
    for (int address : std::as_const(m_config.nodeAddresses)) {
        NodeClient client(m_transport.get(), address);
        client.stopUv();
    }
    m_sinceUvPoll.invalidate();
    tick();
}

void AcquisitionService::shutdown()
{
    if (m_timer)
        m_timer->stop();
    if (m_snapshot.running)
        stop();
    m_store.close();
}

bool AcquisitionService::pollUv()
{
    int remaining = 0;
    bool any = false;
    for (int address : std::as_const(m_config.nodeAddresses)) {
        NodeClient client(m_transport.get(), address);
        const auto s = client.status();
        if (s && (s->state == node::StateUv || s->has(node::FlagUvActive))) {
            any = true;
            remaining = qMax(remaining, int(s->uvSecondsRemaining));
        }
    }
    m_snapshot.uvSecondsRemaining = remaining;
    return any;
}

void AcquisitionService::tick()
{
    m_snapshot.online = m_networkProbe();

    if (!m_sinceUvPoll.isValid() || m_sinceUvPoll.elapsed() >= kUvPollMs) {
        m_sinceUvPoll.start();
        if (pollUv()) {
            m_snapshot.state = ServiceSnapshot::State::Sterilizing;
            m_snapshot.detail = QStringLiteral("UV %1 s").arg(m_snapshot.uvSecondsRemaining);
            m_snapshot.nextCycleInS = -1;
            publish();
            return;
        }
    } else if (m_snapshot.state == ServiceSnapshot::State::Sterilizing) {
        return; // keep showing it until the next poll
    }

    if (!m_snapshot.running) {
        m_snapshot.state = ServiceSnapshot::State::Idle;
        m_snapshot.detail.clear();
        m_snapshot.nextCycleInS = -1;
        publish();
        return;
    }

    if (m_config.requireNetwork && !m_snapshot.online) {
        m_snapshot.state = ServiceSnapshot::State::WaitingNetwork;
        m_snapshot.detail = QStringLiteral("no Wi-Fi");
        m_snapshot.nextCycleInS = -1;
        publish();
        return;
    }

    const qint64 intervalMs = qint64(m_config.intervalSeconds) * 1000;
    if (!m_sinceLastCycle.isValid() || m_sinceLastCycle.elapsed() >= intervalMs) {
        m_sinceLastCycle.start();
        runCycle();
        return;
    }

    if (m_snapshot.state != ServiceSnapshot::State::Error) {
        m_snapshot.state = ServiceSnapshot::State::Waiting;
        m_snapshot.detail = m_snapshot.lastCycleId ? QStringLiteral("cycle %1").arg(m_snapshot.lastCycleId)
                                                   : QString();
    }
    m_snapshot.nextCycleInS = int((intervalMs - m_sinceLastCycle.elapsed() + 999) / 1000);
    publish();
}

QList<Measurement> AcquisitionService::runCycle()
{
    QList<Measurement> batch;
    const qint64 cycleId = m_store.isOpen() ? m_store.nextCycleId() : m_snapshot.lastCycleId + 1;

    m_snapshot.state = ServiceSnapshot::State::Sampling;
    m_snapshot.detail = QStringLiteral("cycle %1").arg(cycleId);
    m_snapshot.nextCycleInS = -1;
    publish();

    QStringList failures;
    int nodesOk = 0;
    for (int address : std::as_const(m_config.nodeAddresses)) {
        NodeClient client(m_transport.get(), address);
        const auto s = client.status();
        if (!s) {
            failures << QStringLiteral("%1: %2").arg(hex(address), client.lastError());
            continue;
        }
        if (s->state == node::StateUv) {
            failures << QStringLiteral("%1: sterilizing, skipped").arg(hex(address));
            continue;
        }

        auto record = [&](Measurement::Kind kind, int param, const NodeClient::Readings &r, const node::Status &st) {
            Measurement m;
            m.cycleId = cycleId;
            m.timestampMs = QDateTime::currentMSecsSinceEpoch();
            m.node = address;
            m.kind = kind;
            m.param = param;
            m.flags = st.flags;
            for (int i = 0; i < kSensorCount; ++i)
                m.values[i] = r[i];
            batch.append(m);
        };

        bool ok = true;
        node::Status after;
        if (const auto dark = client.darkCheck(&after)) {
            record(Measurement::Kind::Dark, -1, *dark, after);
            if (after.has(node::FlagDarkAlert))
                emit logMessage(QStringLiteral("%1: DARK_ALERT, light is leaking into the node").arg(hex(address)));
        } else {
            ok = false;
        }
        for (int led : std::as_const(m_config.leds)) {
            if (!ok)
                break;
            if (const auto r = client.measureLed(led, &after))
                record(Measurement::Kind::Led, led, *r, after);
            else
                ok = false;
        }
        if (ok)
            ++nodesOk;
        else
            failures << QStringLiteral("%1: %2").arg(hex(address), client.lastError());
    }

    if (!batch.isEmpty() && m_store.isOpen() && !m_store.insertBatch(batch))
        emit logMessage(QStringLiteral("cannot store cycle %1: %2").arg(cycleId).arg(m_store.lastError()));

    for (const QString &f : std::as_const(failures))
        emit logMessage(f);

    if (nodesOk == 0) {
        m_snapshot.state = ServiceSnapshot::State::Error;
        m_snapshot.detail = failures.isEmpty() ? QStringLiteral("no nodes configured") : failures.first();
    } else {
        ++m_snapshot.cycles;
        m_snapshot.lastCycleId = cycleId;
        m_snapshot.lastCycleMs = QDateTime::currentMSecsSinceEpoch();
        m_snapshot.state = ServiceSnapshot::State::Waiting;
        m_snapshot.detail = QStringLiteral("cycle %1").arg(cycleId);
        m_snapshot.nextCycleInS = m_config.intervalSeconds;
    }
    publish();
    return batch;
}

void AcquisitionService::publish()
{
    emit snapshotChanged(m_snapshot);
}

} // namespace clorela
