// SPDX-License-Identifier: MIT
// Master side of the node-routine I2C protocol (C++ port of
// node-routine/host/node_i2c_example.py).
#pragma once

#include "nodeprotocol.h"
#include "nodetransport.h"

#include <QByteArray>
#include <array>
#include <optional>

namespace clorela {

class NodeClient
{
public:
    using Readings = std::array<quint16, 16>;

    NodeClient(NodeTransport *transport, int address);

    int address() const { return m_address; }
    QString lastError() const { return m_lastError; }

    std::optional<node::Status> status();
    bool command(char letter, const QByteArray &args = QByteArray());
    // Polls the status until the node is IDLE (or in UVC). Fails on timeout
    // or when the node reports an error for the last command.
    std::optional<node::Status> waitIdle(int timeoutMs = 10000);
    std::optional<Readings> page(int number);

    // High-level operations used by the acquisition cycle.
    std::optional<Readings> darkCheck(node::Status *after = nullptr);
    std::optional<Readings> measureLed(int led, node::Status *after = nullptr);
    bool allOff();
    bool startUv(int seconds);
    bool stopUv();
    bool safeMode();

    static node::Status parseStatus(const QByteArray &bytes);
    static Readings parsePage(const QByteArray &bytes);

private:
    std::optional<Readings> runAndRead(char letter, const QByteArray &args, node::Status *after);

    NodeTransport *m_transport;
    int m_address;
    QString m_lastError;
};

} // namespace clorela
