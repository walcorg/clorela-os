// SPDX-License-Identifier: MIT
// One reading of the 16 light sensors of a node, as returned by data page 0
// of the node-routine firmware (walcorg/node-routine).
#pragma once

#include <QString>
#include <array>
#include <cstdint>

namespace clorela {

constexpr int kSensorCount = 16;      // CD74HC4067 channels
constexpr int kBackscatterFirst = 0;  // SENS_B0..B7 (J1)
constexpr int kTransmissionFirst = 8; // SENS_T0..T7 (J2)

struct Measurement
{
    enum class Kind {
        Dark,   // 'G': all LEDs off (darkness check)
        Led,    // 'H', n: only LED n on; param = n
        Byte,   // 'K', byte: byte pattern on LEDs 0-7; param = byte
        Sample  // 'S': LEDs as they were
    };

    qint64 cycleId = 0;          // groups the readings of one acquisition cycle
    qint64 timestampMs = 0;      // UTC, ms since epoch (host clock)
    int node = 0;                // I2C address of the node
    Kind kind = Kind::Sample;
    int param = -1;
    quint8 flags = 0;            // node status flags at the time of the reading
    std::array<quint16, kSensorCount> values{};

    double meanTransmission() const
    {
        double sum = 0;
        for (int i = kTransmissionFirst; i < kTransmissionFirst + 8; ++i)
            sum += values[i];
        return sum / 8.0;
    }

    static QString kindToString(Kind k)
    {
        switch (k) {
        case Kind::Dark: return QStringLiteral("dark");
        case Kind::Led: return QStringLiteral("led");
        case Kind::Byte: return QStringLiteral("byte");
        case Kind::Sample: return QStringLiteral("sample");
        }
        return QStringLiteral("sample");
    }

    static Kind kindFromString(const QString &s)
    {
        if (s == QLatin1String("dark")) return Kind::Dark;
        if (s == QLatin1String("led")) return Kind::Led;
        if (s == QLatin1String("byte")) return Kind::Byte;
        return Kind::Sample;
    }
};

} // namespace clorela
