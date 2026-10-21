// SPDX-License-Identifier: MIT
// Local control socket of clorela-acqd (default /run/clorela/acqd.sock).
//
// Protocol: one JSON object per line in each direction.
//   -> {"cmd":"status"}                     <- {"ok":true,"state":"waiting",...}
//   -> {"cmd":"start"} / {"cmd":"stop"}     <- {"ok":true,...}
//   -> {"cmd":"sterilize","seconds":300}    <- {"ok":true,...}
//   -> {"cmd":"stop_uv"}                    <- {"ok":true,...}
// Errors: {"ok":false,"error":"..."}. Every reply carries the current snapshot.
// Commands are queued to the service thread; the reply reflects the state
// before the command took effect.
#pragma once

#include "acquisitionservice.h"

#include <QJsonObject>
#include <QObject>

class QLocalServer;
class QLocalSocket;

namespace clorela {

class ControlServer : public QObject
{
    Q_OBJECT

public:
    ControlServer(AcquisitionService *service, const Config &config, QObject *parent = nullptr);
    bool listen(const QString &path);
    QString errorString() const { return m_error; }

    static QJsonObject snapshotToJson(const ServiceSnapshot &s, const Config &config);
    // Handles one request; exposed for tests.
    QJsonObject handle(const QJsonObject &request);

public slots:
    void setSnapshot(const clorela::ServiceSnapshot &snapshot) { m_snapshot = snapshot; }

private slots:
    void onNewConnection();

private:
    void onReadyRead(QLocalSocket *socket);

    AcquisitionService *m_service;
    Config m_config;
    QLocalServer *m_server;
    ServiceSnapshot m_snapshot;
    QString m_error;
};

} // namespace clorela
