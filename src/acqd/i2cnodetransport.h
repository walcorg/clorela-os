// SPDX-License-Identifier: MIT
#pragma once

#include "i2cdevice.h"
#include "nodetransport.h"

namespace clorela {

// Nodes on a Linux I2C bus. The bus number is acquisition/i2c_bus [TO VERIFY]:
// node-routine recommends the software bus (dtoverlay=i2c-gpio) because the
// BCM2835 hardware controller does not handle clock stretching.
class I2cNodeTransport : public NodeTransport
{
public:
    explicit I2cNodeTransport(int bus) : m_dev(bus) {}
    QString name() const override { return QStringLiteral("i2c-%1").arg(m_dev.bus()); }

    bool write(int address, const QByteArray &data) override
    {
        if (m_dev.write(address, data))
            return true;
        m_lastError = m_dev.lastError();
        return false;
    }

    bool writeRead(int address, const QByteArray &data, int readLength, QByteArray *out) override
    {
        if (m_dev.writeRead(address, data, readLength, out))
            return true;
        m_lastError = m_dev.lastError();
        return false;
    }

private:
    I2cDevice m_dev;
};

} // namespace clorela
