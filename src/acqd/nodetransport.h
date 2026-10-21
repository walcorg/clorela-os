// SPDX-License-Identifier: MIT
// How clorela-acqd reaches the nodes. Two implementations:
//   I2cNodeTransport   real hardware through the shield (/dev/i2c-N)
//   SimulatedNodeBus   emulation of the node-routine firmware, no hardware
// NodeClient speaks the protocol on top of either one.
#pragma once

#include <QByteArray>
#include <QString>

namespace clorela {

class NodeTransport
{
public:
    virtual ~NodeTransport() = default;
    virtual QString name() const = 0;
    virtual bool write(int address, const QByteArray &data) = 0;
    virtual bool writeRead(int address, const QByteArray &data, int readLength, QByteArray *out) = 0;
    QString lastError() const { return m_lastError; }

protected:
    QString m_lastError;
};

} // namespace clorela
