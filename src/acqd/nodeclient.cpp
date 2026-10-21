// SPDX-License-Identifier: MIT
#include "nodeclient.h"

#include <QElapsedTimer>
#include <QThread>

namespace clorela {

using namespace node;

namespace {
constexpr int kPollIntervalMs = 20;
}

NodeClient::NodeClient(NodeTransport *transport, int address)
    : m_transport(transport)
    , m_address(address)
{
}

Status NodeClient::parseStatus(const QByteArray &b)
{
    Status s;
    if (b.size() < kStatusLength)
        return s;
    s.state = uint8_t(b.at(0));
    s.flags = uint8_t(b.at(1));
    s.lastCommand = b.at(2);
    s.error = uint8_t(b.at(3));
    s.firmwareVersion = uint8_t(b.at(4));
    s.uvSecondsRemaining = uint16_t(uint8_t(b.at(5)) | (uint8_t(b.at(6)) << 8));
    s.address = uint8_t(b.at(7));
    return s;
}

NodeClient::Readings NodeClient::parsePage(const QByteArray &b)
{
    Readings r{};
    for (int i = 0; i < 16 && 2 * i + 1 < b.size(); ++i)
        r[i] = quint16(uint8_t(b.at(2 * i)) | (uint8_t(b.at(2 * i + 1)) << 8));
    return r;
}

std::optional<Status> NodeClient::status()
{
    QByteArray rx;
    if (!m_transport->writeRead(m_address, QByteArray(1, CmdStatus), kStatusLength, &rx)) {
        m_lastError = m_transport->lastError();
        return std::nullopt;
    }
    return parseStatus(rx);
}

bool NodeClient::command(char letter, const QByteArray &args)
{
    QByteArray frame(1, letter);
    frame += args;
    if (frame.size() > kMaxWrite) {
        m_lastError = QStringLiteral("command too long (%1 bytes)").arg(frame.size());
        return false;
    }
    if (!m_transport->write(m_address, frame)) {
        m_lastError = m_transport->lastError();
        return false;
    }
    return true;
}

std::optional<Status> NodeClient::waitIdle(int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (true) {
        const auto s = status();
        if (!s)
            return std::nullopt;
        if (s->state == StateIdle || s->state == StateUv) {
            if (s->error != ErrNone) {
                m_lastError = QStringLiteral("node 0x%1: %2")
                                  .arg(m_address, 2, 16, QLatin1Char('0'))
                                  .arg(errorText(s->error));
                return std::nullopt;
            }
            return s;
        }
        if (t.elapsed() > timeoutMs) {
            m_lastError = QStringLiteral("node 0x%1 did not finish within %2 ms")
                              .arg(m_address, 2, 16, QLatin1Char('0'))
                              .arg(timeoutMs);
            return std::nullopt;
        }
        QThread::msleep(kPollIntervalMs);
    }
}

std::optional<NodeClient::Readings> NodeClient::page(int number)
{
    QByteArray rx;
    const QByteArray frame{QByteArray(1, CmdReadPage) + QByteArray(1, char(number))};
    if (!m_transport->writeRead(m_address, frame, kPageLength, &rx)) {
        m_lastError = m_transport->lastError();
        return std::nullopt;
    }
    return parsePage(rx);
}

std::optional<NodeClient::Readings> NodeClient::runAndRead(char letter, const QByteArray &args, Status *after)
{
    if (!command(letter, args))
        return std::nullopt;
    const auto s = waitIdle();
    if (!s)
        return std::nullopt;
    if (s->state == StateUv) {
        m_lastError = QStringLiteral("node 0x%1 is sterilizing (UVC)").arg(m_address, 2, 16, QLatin1Char('0'));
        return std::nullopt;
    }
    if (after)
        *after = *s;
    return page(PageLastMeasurement);
}

std::optional<NodeClient::Readings> NodeClient::darkCheck(Status *after)
{
    return runAndRead(CmdDarkCheck, QByteArray(), after);
}

std::optional<NodeClient::Readings> NodeClient::measureLed(int led, Status *after)
{
    if (led < 0 || led >= kVisibleLeds) {
        m_lastError = QStringLiteral("LED %1 out of range 0-11").arg(led);
        return std::nullopt;
    }
    return runAndRead(CmdSingleLed, QByteArray(1, char(led)), after);
}

bool NodeClient::allOff()
{
    return command(CmdAllOff);
}

bool NodeClient::startUv(int seconds)
{
    if (seconds <= 0 || seconds > kUvHardMaxSeconds) {
        m_lastError = QStringLiteral("UVC duration must be 1-%1 s").arg(kUvHardMaxSeconds);
        return false;
    }
    QByteArray args;
    args.append(char(seconds & 0xFF));
    args.append(char(seconds >> 8));
    return command(CmdUvStart, args);
}

bool NodeClient::stopUv()
{
    return command(CmdUvStop);
}

bool NodeClient::safeMode()
{
    return command(CmdSafeMode);
}

} // namespace clorela
