// SPDX-License-Identifier: MIT
#include "controlserver.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>

namespace clorela {

namespace {
constexpr qint64 kMaxRequestBytes = 4096;
}

ControlServer::ControlServer(AcquisitionService *service, const Config &config, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_config(config)
    , m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, &ControlServer::onNewConnection);
}

bool ControlServer::listen(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QLocalServer::removeServer(path); // stale socket from a previous run
    // Owner and group only: the interface runs as the same user.
    m_server->setSocketOptions(QLocalServer::UserAccessOption | QLocalServer::GroupAccessOption);
    if (!m_server->listen(path)) {
        m_error = m_server->errorString();
        return false;
    }
    return true;
}

QJsonObject ControlServer::snapshotToJson(const ServiceSnapshot &s, const Config &config)
{
    QJsonArray nodes;
    for (int n : config.nodeAddresses)
        nodes.append(n);
    QJsonObject o;
    o[QStringLiteral("ok")] = true;
    o[QStringLiteral("state")] = ServiceSnapshot::stateName(s.state);
    o[QStringLiteral("running")] = s.running;
    o[QStringLiteral("online")] = s.online;
    o[QStringLiteral("detail")] = s.detail;
    o[QStringLiteral("cycles")] = double(s.cycles);
    o[QStringLiteral("last_cycle")] = double(s.lastCycleId);
    o[QStringLiteral("last_cycle_ms")] = double(s.lastCycleMs);
    o[QStringLiteral("next_cycle_s")] = s.nextCycleInS;
    o[QStringLiteral("uv_s")] = s.uvSecondsRemaining;
    o[QStringLiteral("sensor")] = config.sensor;
    o[QStringLiteral("signer")] = config.signerBackend;
    o[QStringLiteral("nodes")] = nodes;
    o[QStringLiteral("version")] = QStringLiteral(CLORELA_VERSION);
    return o;
}

QJsonObject ControlServer::handle(const QJsonObject &request)
{
    const QString cmd = request.value(QStringLiteral("cmd")).toString();
    QJsonObject reply = snapshotToJson(m_snapshot, m_config);

    if (cmd == QLatin1String("status")) {
        return reply;
    }
    if (cmd == QLatin1String("start")) {
        QMetaObject::invokeMethod(m_service, &AcquisitionService::start, Qt::QueuedConnection);
        return reply;
    }
    if (cmd == QLatin1String("stop")) {
        QMetaObject::invokeMethod(m_service, &AcquisitionService::stop, Qt::QueuedConnection);
        return reply;
    }
    if (cmd == QLatin1String("sterilize")) {
        const int seconds = request.value(QStringLiteral("seconds")).toInt(300);
        if (seconds <= 0 || seconds > 1800) {
            QJsonObject err;
            err[QStringLiteral("ok")] = false;
            err[QStringLiteral("error")] = QStringLiteral("seconds must be 1-1800");
            return err;
        }
        QMetaObject::invokeMethod(m_service, [svc = m_service, seconds] { svc->sterilize(seconds); },
                                  Qt::QueuedConnection);
        return reply;
    }
    if (cmd == QLatin1String("stop_uv")) {
        QMetaObject::invokeMethod(m_service, &AcquisitionService::stopSterilization, Qt::QueuedConnection);
        return reply;
    }

    if (cmd == QLatin1String("laura_calibrate") || cmd == QLatin1String("laura_challenge"))
        return handleLaura(cmd, request, reply);

    QJsonObject err;
    err[QStringLiteral("ok")] = false;
    err[QStringLiteral("error")] = QStringLiteral("unknown command '%1'").arg(cmd);
    return err;
}

QJsonObject ControlServer::handleLaura(const QString &cmd, const QJsonObject &request, QJsonObject reply)
{
    auto fail = [](const QString &message) {
        QJsonObject e;
        e[QStringLiteral("ok")] = false;
        e[QStringLiteral("error")] = message;
        return e;
    };
    const int address = request.value(QStringLiteral("node")).toInt(-1);
    if (!m_config.nodeAddresses.contains(address))
        return fail(QStringLiteral("node must be one of the configured addresses"));

    std::function<QJsonObject()> job;
    if (cmd == QLatin1String("laura_calibrate")) {
        job = [svc = m_service, address] { return svc->lauraCalibrate(address); };
    } else {
        const QString idHex = request.value(QStringLiteral("challenge_id")).toString();
        const QByteArray id = QByteArray::fromHex(idHex.toLatin1());
        if (idHex.isEmpty() || idHex.size() % 2 || id.size() * 2 != idHex.size()
            || id.size() > 32)
            return fail(QStringLiteral("challenge_id must be 1-32 bytes in hex"));
        const QJsonArray arr = request.value(QStringLiteral("levels")).toArray();
        if (arr.size() != node::kVisibleLeds)
            return fail(QStringLiteral("levels must have %1 values").arg(node::kVisibleLeds));
        NodeClient::Levels levels{};
        for (int i = 0; i < arr.size(); ++i) {
            const double v = arr.at(i).toDouble(-1);
            if (!arr.at(i).isDouble() || v < 0 || v > node::kMaxLevel || v != int(v))
                return fail(QStringLiteral("levels must be integers 0-%1").arg(node::kMaxLevel));
            levels[i] = quint16(v);
        }
        job = [svc = m_service, address, id, levels] { return svc->lauraChallenge(address, id, levels); };
    }

    // The node must answer before we reply, so wait for the service thread.
    QJsonObject result;
    if (m_service->thread() == QThread::currentThread())
        result = job();
    else
        QMetaObject::invokeMethod(m_service, [&] { result = job(); }, Qt::BlockingQueuedConnection);

    if (!result.value(QStringLiteral("ok")).toBool())
        return fail(result.value(QStringLiteral("error")).toString());
    result.remove(QStringLiteral("ok"));
    reply[QStringLiteral("laura")] = result;
    return reply;
}

void ControlServer::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
    }
}

void ControlServer::onReadyRead(QLocalSocket *socket)
{
    while (socket->canReadLine()) {
        const QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty())
            continue;
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        QJsonObject reply;
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            reply[QStringLiteral("ok")] = false;
            reply[QStringLiteral("error")] = QStringLiteral("invalid JSON");
        } else {
            reply = handle(doc.object());
        }
        socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
    }
    if (socket->bytesAvailable() > kMaxRequestBytes)
        socket->abort(); // no newline in sight: drop the client
}

} // namespace clorela
