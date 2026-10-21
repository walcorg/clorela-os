// SPDX-License-Identifier: MIT
// Software emulation of one or more nodes running node-routine, so the daemon,
// the interface and the tests can run on a PC without hardware.
//
// It answers the same bytes as the firmware (status, data pages, errors, UVC
// lockout). Commands complete instantly. The optical model is deliberately
// simple: light from each LED crosses the culture with Beer-Lambert
// attenuation; the optical density grows slowly with every measurement so the
// Graph view shows a trend. It is not a model of a real Chlorella culture.
#pragma once

#include "nodeprotocol.h"
#include "nodetransport.h"

#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <array>
#include <random>

namespace clorela {

class SimulatedNodeBus : public NodeTransport
{
public:
    explicit SimulatedNodeBus(const QList<int> &addresses, unsigned seed = 2026);

    QString name() const override { return QStringLiteral("sim"); }
    bool write(int address, const QByteArray &data) override;
    bool writeRead(int address, const QByteArray &data, int readLength, QByteArray *out) override;

    // Test hooks.
    void setLightLeak(int address, bool leak);
    double opticalDensity(int address) const;
    int commandsReceived(int address) const;

private:
    struct Node
    {
        int address = 0;
        uint8_t state = node::StateIdle;
        uint8_t flags = 0;
        char lastCommand = 0;
        uint8_t error = node::ErrNone;
        uint16_t ledMask = 0;           // visible LEDs 0-11 currently on
        bool readPage = false;          // next read returns a data page
        uint8_t page = 0;
        std::array<std::array<uint16_t, 16>, 15> pages{};
        std::array<uint16_t, 6> params{30, 20, 4095, 4095, 4, 300};
        QElapsedTimer uvTimer;
        int uvSeconds = 0;
        double od = 0.35;               // optical density of the culture
        bool lightLeak = false;
        int commands = 0;
    };

    void execute(Node &n, const QByteArray &data);
    static void updateUv(Node &n);
    std::array<uint16_t, 16> measure(Node &n);
    QByteArray statusBytes(const Node &n) const;
    QByteArray pageBytes(const Node &n) const;
    Node *find(int address);

    QMap<int, Node> m_nodes;
    std::mt19937 m_rng;
};

} // namespace clorela
