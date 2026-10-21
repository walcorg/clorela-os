// SPDX-License-Identifier: MIT
#include "simulatednode.h"

#include <QtMath>
#include <algorithm>

namespace clorela {

using namespace node;

SimulatedNodeBus::SimulatedNodeBus(const QList<int> &addresses, unsigned seed)
    : m_rng(seed)
{
    for (int a : addresses) {
        Node n;
        n.address = a;
        m_nodes.insert(a, n);
    }
}

SimulatedNodeBus::Node *SimulatedNodeBus::find(int address)
{
    auto it = m_nodes.find(address);
    if (it == m_nodes.end()) {
        m_lastError = QStringLiteral("no ACK from 0x%1 (simulated bus)").arg(address, 2, 16, QLatin1Char('0'));
        return nullptr;
    }
    return &it.value();
}

void SimulatedNodeBus::setLightLeak(int address, bool leak)
{
    if (Node *n = find(address))
        n->lightLeak = leak;
}

double SimulatedNodeBus::opticalDensity(int address) const
{
    return m_nodes.value(address).od;
}

int SimulatedNodeBus::commandsReceived(int address) const
{
    return m_nodes.value(address).commands;
}

bool SimulatedNodeBus::write(int address, const QByteArray &data)
{
    Node *n = find(address);
    if (!n)
        return false;
    if (data.isEmpty() || data.size() > kMaxWrite) {
        m_lastError = QStringLiteral("invalid write length %1").arg(data.size());
        return false;
    }
    execute(*n, data);
    return true;
}

bool SimulatedNodeBus::writeRead(int address, const QByteArray &data, int readLength, QByteArray *out)
{
    if (!write(address, data))
        return false;
    Node &n = *find(address);
    QByteArray bytes = n.readPage ? pageBytes(n) : statusBytes(n);
    bytes = bytes.left(readLength);
    bytes.append(QByteArray(readLength - bytes.size(), char(0xFF))); // bus idles high
    *out = bytes;
    return true;
}

void SimulatedNodeBus::updateUv(Node &n)
{
    if (n.state != StateUv)
        return;
    if (n.uvTimer.elapsed() >= qint64(n.uvSeconds) * 1000) {
        n.state = StateIdle;
        n.flags &= ~FlagUvActive;
    }
}

std::array<uint16_t, 16> SimulatedNodeBus::measure(Node &n)
{
    std::normal_distribution<double> noise(0.0, 2.0);
    const double gain = (n.flags & FlagSensitiveRange) ? 3.0 : 1.0;
    const double intensity = n.params[ParamVisibleIntensity] / 4095.0;
    std::array<uint16_t, 16> v{};
    for (int ch = 0; ch < 16; ++ch) {
        double value = 8.0 + noise(m_rng) + (n.lightLeak ? 60.0 : 0.0);
        const bool transmission = ch >= 8;
        const int position = ch % 8;
        for (int led = 0; led < kVisibleLeds; ++led) {
            if (!(n.ledMask & (1 << led)))
                continue;
            // LED k faces sensor k on the opposite wall; neighbours get less.
            const int distance = std::abs((led % 8) - position);
            const double geometry = distance == 0 ? 1.0 : 0.35 / distance;
            const double base = (led >= 8 ? 700.0 : 900.0) * intensity * geometry;
            if (transmission)
                value += base * qExp(-n.od * 2.0);           // Beer-Lambert through 1 L cube
            else
                value += base * 0.25 * (1.0 - qExp(-n.od));  // backscatter grows with biomass
        }
        v[ch] = uint16_t(std::clamp(value * gain, 0.0, 1023.0));
    }
    n.od = std::min(3.0, n.od + 0.0005); // slow growth so trends are visible
    n.flags |= FlagDataReady;
    return v;
}

void SimulatedNodeBus::execute(Node &n, const QByteArray &data)
{
    updateUv(n);
    ++n.commands;
    const char cmd = data.at(0);
    auto arg = [&](int i) -> int { return i < data.size() ? uint8_t(data.at(i)) : -1; };
    auto arg16 = [&](int i) -> int {
        return (i + 1 < data.size()) ? (uint8_t(data.at(i)) | (uint8_t(data.at(i + 1)) << 8)) : -1;
    };

    if (cmd == CmdReadPage) {
        const int page = arg(1);
        if (page < 0 || page > PageConfiguration) {
            n.error = ErrInvalidArgument;
            n.readPage = false;
            return;
        }
        n.readPage = true;
        n.page = uint8_t(page);
        return;
    }
    n.readPage = false;
    if (cmd == CmdStatus)
        return;

    n.lastCommand = cmd;
    n.error = ErrNone;

    // While UVC is active only 'F' and 'D' are served.
    if (n.state == StateUv && cmd != CmdUvStop && cmd != CmdSafeMode) {
        n.error = ErrBlockedByUv;
        return;
    }

    switch (cmd) {
    case CmdDarkCheck: {
        n.ledMask = 0;
        n.pages[PageLastMeasurement] = measure(n);
        const auto &m = n.pages[PageLastMeasurement];
        const bool alert = std::any_of(m.begin(), m.end(), [&](uint16_t x) { return x > n.params[ParamDarkThreshold]; });
        if (alert)
            n.flags |= FlagDarkAlert;
        else
            n.flags &= ~FlagDarkAlert;
        break;
    }
    case CmdSingleLed: {
        const int led = arg(1);
        if (led < 0 || led >= kVisibleLeds) {
            n.error = ErrInvalidArgument;
            break;
        }
        n.ledMask = uint16_t(1 << led);
        n.pages[PageLastMeasurement] = measure(n);
        n.ledMask = 0;
        break;
    }
    case CmdByte: {
        const int b = arg(1);
        if (b < 0) {
            n.error = ErrInvalidArgument;
            break;
        }
        n.ledMask = uint16_t(b);
        n.pages[PageLastMeasurement] = measure(n);
        break;
    }
    case CmdLedMask: {
        const int mask = arg16(1);
        if (mask < 0 || mask >= (1 << kVisibleLeds)) {
            n.error = ErrInvalidArgument;
            break;
        }
        n.ledMask = uint16_t(mask);
        break;
    }
    case CmdSample:
        n.pages[PageLastMeasurement] = measure(n);
        break;
    case CmdAllOff:
        n.ledMask = 0;
        break;
    case CmdPlayText:
        if (data.size() < 4)
            n.error = ErrInvalidArgument;
        break; // completes instantly in the simulation
    case CmdCalibrate:
        n.ledMask = 0;
        n.pages[PageDarkBaseline] = measure(n);
        for (int led = 0; led < kVisibleLeds; ++led) {
            n.ledMask = uint16_t(1 << led);
            n.pages[PageFirstLed + led] = measure(n);
        }
        n.ledMask = 0;
        n.pages[PageLastMeasurement] = n.pages[PageFirstLed + kVisibleLeds - 1];
        break;
    case CmdUvStart: {
        int seconds = data.size() >= 3 ? arg16(1) : n.params[ParamUvDefaultSeconds];
        if (seconds <= 0 || seconds > kUvHardMaxSeconds) {
            n.error = ErrInvalidArgument;
            break;
        }
        n.ledMask = 0; // UVC switches the visible LEDs off
        n.uvSeconds = seconds;
        n.uvTimer.start();
        n.state = StateUv;
        n.flags |= FlagUvActive;
        break;
    }
    case CmdUvStop:
        if (n.state == StateUv)
            n.error = ErrAborted;
        n.state = StateIdle;
        n.flags &= ~FlagUvActive;
        break;
    case CmdSafeMode:
        if (n.state == StateUv)
            n.error = ErrAborted;
        n.state = StateIdle;
        n.ledMask = 0;
        n.flags &= ~FlagUvActive;
        break;
    case CmdRange: {
        const int r = arg(1);
        if (r == 0)
            n.flags &= ~FlagSensitiveRange;
        else if (r == 1)
            n.flags |= FlagSensitiveRange;
        else
            n.error = ErrInvalidArgument;
        break;
    }
    case CmdParameter: {
        const int id = arg(1);
        const int value = arg16(2);
        if (id < 0 || id > ParamUvDefaultSeconds || value < 0) {
            n.error = ErrInvalidArgument;
            break;
        }
        n.params[id] = uint16_t(value);
        for (int i = 0; i < 6; ++i)
            n.pages[PageConfiguration][i] = n.params[i];
        break;
    }
    default:
        n.error = ErrUnknownCommand;
        break;
    }
}

QByteArray SimulatedNodeBus::statusBytes(const Node &n) const
{
    Node copy = n;
    updateUv(copy);
    int remaining = 0;
    if (copy.state == StateUv)
        remaining = qMax(0, copy.uvSeconds - int(copy.uvTimer.elapsed() / 1000));
    QByteArray b(kStatusLength, '\0');
    b[0] = char(copy.state);
    b[1] = char(copy.flags);
    b[2] = copy.lastCommand;
    b[3] = char(copy.error);
    b[4] = char(0x01); // firmware version of node-routine v0.1
    b[5] = char(remaining & 0xFF);
    b[6] = char(remaining >> 8);
    b[7] = char(copy.address);
    return b;
}

QByteArray SimulatedNodeBus::pageBytes(const Node &n) const
{
    QByteArray b(kPageLength, '\0');
    const auto &page = n.pages[n.page];
    for (int i = 0; i < 16; ++i) {
        b[2 * i] = char(page[i] & 0xFF);
        b[2 * i + 1] = char(page[i] >> 8);
    }
    return b;
}

} // namespace clorela
