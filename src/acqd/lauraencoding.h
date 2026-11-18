// SPDX-License-Identifier: MIT
// Laura Encoding (Codificado de Laura): node side.
//
// The culture between the LEDs and the sensors is a physical 16x12 transfer
// matrix M. 'C' measures it and 'X' answers an optical challenge (an intensity
// pattern chosen by the server). Clorela-OS only relays: it runs the command,
// builds a canonical byte string of the result, hashes it with SHA-256 and asks
// the Signer (TROPIC01, when present) to sign the digest. Verification, and the
// stored M, live on the server. Specification: docs/laura-encoding.md.
#pragma once

#include "nodeclient.h"
#include "signer.h"

#include <QByteArray>
#include <QJsonObject>

namespace clorela::laura {

// Domain separation tags of the two signed messages (version 1).
inline constexpr char kChallengeDomain[] = "WALCORG-LAURA-CHALLENGE-v1";
inline constexpr char kCalibrationDomain[] = "WALCORG-LAURA-CALIBRATION-v1";
inline constexpr int kMaxChallengeIdBytes = 32;

struct Signed
{
    QByteArray digest;      // SHA-256 of the canonical bytes
    QByteArray signature;   // r || s (64 bytes), empty if not signed
    SignerStatus signerStatus = SignerStatus::NotAvailable;
    QString signerName;
};

struct ChallengeResponse
{
    int node = 0;
    QByteArray challengeId;  // opaque nonce from the server, 1-32 bytes
    NodeClient::Levels levels{};
    NodeClient::Readings readings{};
    quint8 flags = 0;
    quint8 firmware = 0;
    qint64 timestampMs = 0;  // host clock, UTC
    Signed sig;
};

struct CalibrationRecord
{
    int node = 0;
    NodeClient::Calibration data;
    quint8 flags = 0;
    quint8 firmware = 0;
    qint64 timestampMs = 0;
    Signed sig;
};

// Canonical encodings (all integers little-endian):
//   challenge:   domain | node u8 | len u8 | challengeId | levels 12 x u16 |
//                readings 16 x u16 | flags u8 | firmware u8 | timestamp i64
//   calibration: domain | node u8 | dark 16 x u16 | led 12 x 16 x u16 |
//                flags u8 | firmware u8 | timestamp i64
QByteArray canonicalBytes(const ChallengeResponse &r);
QByteArray canonicalBytes(const CalibrationRecord &c);

// Fills r.sig / c.sig: digest always, signature when the signer can sign.
void sign(ChallengeResponse *r, Signer *signer);
void sign(CalibrationRecord *c, Signer *signer);

QJsonObject toJson(const ChallengeResponse &r);
QJsonObject toJson(const CalibrationRecord &c);

} // namespace clorela::laura
