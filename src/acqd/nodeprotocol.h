// SPDX-License-Identifier: MIT
// I2C protocol of the node firmware, mirrored from walcorg/node-routine
// (firmware/walcorg_node_v45/walcorg_node_v45.ino, version 0x01).
// Keep this file in sync with the sketch header.
#pragma once

#include <QString>
#include <cstdint>

namespace clorela::node {

constexpr int kFirstAddress = 0x10;      // 0x10 + ADDR1*2 + ADDR0
constexpr int kLastAddress = 0x13;
constexpr int kStatusLength = 8;         // bytes returned after any command
constexpr int kPageLength = 32;          // 16 little-endian uint16
constexpr int kMaxWrite = 32;            // Wire buffer of the ATmega328P
constexpr int kVisibleLeds = 12;         // TLC59401 channels 0-11
constexpr int kUvHardMaxSeconds = 1800;  // enforced by the firmware

// Command letters.
enum Command : char {
    CmdStatus = '?',
    CmdReadPage = 'R',
    CmdDarkCheck = 'G',
    CmdSingleLed = 'H',
    CmdByte = 'K',
    CmdLedMask = 'M',
    CmdSample = 'S',
    CmdAllOff = 'O',
    CmdPlayText = 'B',
    CmdCalibrate = 'C',
    CmdUvStart = 'E',
    CmdUvStop = 'F',
    CmdSafeMode = 'D',
    CmdRange = 'L',
    CmdParameter = 'P',
};

// Data pages for 'R'.
enum Page : uint8_t {
    PageLastMeasurement = 0,
    PageDarkBaseline = 1,
    PageFirstLed = 2,   // 2..13 = LED 0..11 alone
    PageConfiguration = 14,
};

enum State : uint8_t { StateIdle = 0, StateBusy = 1, StateUv = 2, StatePlaying = 3 };

enum Flag : uint8_t {
    FlagDataReady = 1 << 0,
    FlagDarkAlert = 1 << 1,
    FlagXerr = 1 << 2,
    FlagUvActive = 1 << 3,
    FlagSensitiveRange = 1 << 4,
    FlagIntExp = 1 << 5,
    FlagWatchdogReset = 1 << 6,
};

enum Error : uint8_t {
    ErrNone = 0,
    ErrUnknownCommand = 1,
    ErrInvalidArgument = 2,
    ErrBusy = 3,
    ErrBlockedByUv = 4,
    ErrAborted = 5,
};

enum Parameter : uint8_t {
    ParamSettleMs = 0,
    ParamDarkThreshold = 1,
    ParamVisibleIntensity = 2,
    ParamUvIntensity = 3,
    ParamSamples = 4,
    ParamUvDefaultSeconds = 5,
};

struct Status
{
    uint8_t state = StateIdle;
    uint8_t flags = 0;
    char lastCommand = 0;
    uint8_t error = ErrNone;
    uint8_t firmwareVersion = 0;
    uint16_t uvSecondsRemaining = 0;
    uint8_t address = 0;

    bool has(Flag f) const { return flags & f; }
};

inline QString errorText(uint8_t e)
{
    switch (e) {
    case ErrNone: return QStringLiteral("none");
    case ErrUnknownCommand: return QStringLiteral("unknown command");
    case ErrInvalidArgument: return QStringLiteral("invalid argument");
    case ErrBusy: return QStringLiteral("busy");
    case ErrBlockedByUv: return QStringLiteral("blocked by active UVC");
    case ErrAborted: return QStringLiteral("aborted");
    }
    return QStringLiteral("error %1").arg(e);
}

} // namespace clorela::node
