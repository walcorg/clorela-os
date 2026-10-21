// SPDX-License-Identifier: MIT
#include "controlserver.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>

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

    QJsonObject err;
    err[QStringLiteral("ok")] = false;
    err[QStringLiteral("error")] = QStringLiteral("unknown command '%1'").arg(cmd);
    return err;
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
