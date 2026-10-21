// SPDX-License-Identifier: MIT
// Thin wrapper over Linux i2c-dev (/dev/i2c-N), shared by the node transport
// and the status display. Requires the i2c-dev kernel module.
#pragma once

#include <QByteArray>
#include <QString>

namespace clorela {

class I2cDevice
{
public:
    explicit I2cDevice(int bus);
    ~I2cDevice();
    I2cDevice(const I2cDevice &) = delete;
    I2cDevice &operator=(const I2cDevice &) = delete;

    int bus() const { return m_bus; }
    bool open();
    bool isOpen() const { return m_fd >= 0; }
    void close();

    // One write transaction.
    bool write(int address, const QByteArray &data);
    // Write followed by a read with a repeated start (I2C_RDWR), as the
    // node-routine firmware expects for '?' and 'R'.
    bool writeRead(int address, const QByteArray &data, int readLength, QByteArray *out);

    QString lastError() const { return m_lastError; }

private:
    int m_bus;
    int m_fd = -1;
    QString m_lastError;
};

} // namespace clorela
