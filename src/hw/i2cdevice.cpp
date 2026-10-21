// SPDX-License-Identifier: MIT
#include "i2cdevice.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace clorela {

I2cDevice::I2cDevice(int bus)
    : m_bus(bus)
{
}

I2cDevice::~I2cDevice()
{
    close();
}

bool I2cDevice::open()
{
    if (m_fd >= 0)
        return true;
    const QByteArray path = QStringLiteral("/dev/i2c-%1").arg(m_bus).toLocal8Bit();
    m_fd = ::open(path.constData(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0) {
        m_lastError = QStringLiteral("%1: %2").arg(QString::fromLocal8Bit(path), QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    return true;
}

void I2cDevice::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool I2cDevice::write(int address, const QByteArray &data)
{
    if (!open())
        return false;
    i2c_msg msg{};
    msg.addr = quint16(address);
    msg.flags = 0;
    msg.len = quint16(data.size());
    msg.buf = reinterpret_cast<quint8 *>(const_cast<char *>(data.constData()));
    i2c_rdwr_ioctl_data xfer{&msg, 1};
    if (::ioctl(m_fd, I2C_RDWR, &xfer) < 0) {
        m_lastError = QStringLiteral("write to 0x%1 on bus %2: %3")
                          .arg(address, 2, 16, QLatin1Char('0'))
                          .arg(m_bus)
                          .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    return true;
}

bool I2cDevice::writeRead(int address, const QByteArray &data, int readLength, QByteArray *out)
{
    if (!open())
        return false;
    QByteArray rx(readLength, '\0');
    i2c_msg msgs[2]{};
    msgs[0].addr = quint16(address);
    msgs[0].flags = 0;
    msgs[0].len = quint16(data.size());
    msgs[0].buf = reinterpret_cast<quint8 *>(const_cast<char *>(data.constData()));
    msgs[1].addr = quint16(address);
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = quint16(readLength);
    msgs[1].buf = reinterpret_cast<quint8 *>(rx.data());
    i2c_rdwr_ioctl_data xfer{msgs, 2};
    if (::ioctl(m_fd, I2C_RDWR, &xfer) < 0) {
        m_lastError = QStringLiteral("write/read 0x%1 on bus %2: %3")
                          .arg(address, 2, 16, QLatin1Char('0'))
                          .arg(m_bus)
                          .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    *out = rx;
    return true;
}

} // namespace clorela
