// SPDX-License-Identifier: MIT
// Client of the clorela-acqd control socket (see src/acqd/controlserver.h).
// Asynchronous: one request at a time, replies arrive through replyReceived().
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QQueue>

class QLocalSocket;

class AcqdClient : public QObject
{
    Q_OBJECT

public:
    explicit AcqdClient(const QString &socketPath, QObject *parent = nullptr);

    void request(const QJsonObject &message);
    void request(const QString &cmd) { request(QJsonObject{{QStringLiteral("cmd"), cmd}}); }

    bool isReachable() const { return m_reachable; }
    QJsonObject lastStatus() const { return m_lastStatus; }

signals:
    void replyReceived(const QJsonObject &request, const QJsonObject &reply);
    void reachableChanged(bool reachable);

private:
    void sendNext();
    void onReadyRead();
    void onError();
    void setReachable(bool reachable);

    QString m_path;
    QLocalSocket *m_socket;
    QQueue<QJsonObject> m_queue;
    QJsonObject m_inFlight;
    bool m_busy = false;
    bool m_reachable = false;
    QJsonObject m_lastStatus;
};
