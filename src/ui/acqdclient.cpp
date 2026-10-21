// SPDX-License-Identifier: MIT
#include "acqdclient.h"

#include <QJsonDocument>
#include <QLocalSocket>

namespace {
constexpr int kMaxQueue = 8;
}

AcqdClient::AcqdClient(const QString &socketPath, QObject *parent)
    : QObject(parent)
    , m_path(socketPath)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::connected, this, [this] {
        setReachable(true);
        const QByteArray line = QJsonDocument(m_inFlight).toJson(QJsonDocument::Compact) + '\n';
        m_socket->write(line);
    });
    connect(m_socket, &QLocalSocket::readyRead, this, &AcqdClient::onReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &AcqdClient::onError);
}

void AcqdClient::request(const QJsonObject &message)
{
    if (m_queue.size() >= kMaxQueue)
        m_queue.dequeue(); // the daemon is not answering; keep only recent requests
    m_queue.enqueue(message);
    if (!m_busy)
        sendNext();
}

void AcqdClient::sendNext()
{
    if (m_queue.isEmpty()) {
        m_busy = false;
        return;
    }
    m_busy = true;
    m_inFlight = m_queue.dequeue();
    if (m_socket->state() == QLocalSocket::ConnectedState) {
        m_socket->write(QJsonDocument(m_inFlight).toJson(QJsonDocument::Compact) + '\n');
    } else {
        m_socket->abort();
        m_socket->connectToServer(m_path);
    }
}

void AcqdClient::onReadyRead()
{
    while (m_socket->canReadLine()) {
        const QJsonObject reply = QJsonDocument::fromJson(m_socket->readLine()).object();
        if (reply.value(QStringLiteral("ok")).toBool() && reply.contains(QStringLiteral("state")))
            m_lastStatus = reply;
        const QJsonObject req = m_inFlight;
        emit replyReceived(req, reply);
        sendNext();
    }
}

void AcqdClient::onError()
{
    setReachable(false);
    if (!m_busy) {
        m_socket->abort(); // daemon went away between requests
        return;
    }
    const QJsonObject req = m_inFlight;
    QJsonObject reply{{QStringLiteral("ok"), false}, {QStringLiteral("error"), m_socket->errorString()}};
    m_socket->abort();
    emit replyReceived(req, reply);
    sendNext();
}

void AcqdClient::setReachable(bool reachable)
{
    if (reachable == m_reachable)
        return;
    m_reachable = reachable;
    emit reachableChanged(reachable);
}
