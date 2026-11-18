// SPDX-License-Identifier: MIT
#include "lauraencoding.h"

#include <QCryptographicHash>
#include <QJsonArray>

namespace clorela::laura {

namespace {

void putU8(QByteArray &b, quint8 v) { b.append(char(v)); }

void putU16(QByteArray &b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char(v >> 8));
}

void putI64(QByteArray &b, qint64 v)
{
    const quint64 u = quint64(v);
    for (int i = 0; i < 8; ++i)
        b.append(char((u >> (8 * i)) & 0xFF));
}

template <typename Array>
QJsonArray toArray(const Array &a)
{
    QJsonArray out;
    for (auto v : a)
        out.append(int(v));
    return out;
}

Signed signDigest(const QByteArray &canonical, Signer *signer)
{
    Signed s;
    s.digest = QCryptographicHash::hash(canonical, QCryptographicHash::Sha256);
    if (!signer) {
        s.signerStatus = SignerStatus::NotAvailable;
        s.signerName = QStringLiteral("none");
        return s;
    }
    s.signerName = signer->name();
    Signature sig;
    s.signerStatus = signer->signDigest(s.digest, &sig);
    if (s.signerStatus == SignerStatus::Ok)
        s.signature = sig.toRS();
    return s;
}

void putSigned(QJsonObject &o, const Signed &s)
{
    o[QStringLiteral("digest")] = QString::fromLatin1(s.digest.toHex());
    o[QStringLiteral("signature")] = s.signature.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                           : QJsonValue(QString::fromLatin1(s.signature.toHex()));
    o[QStringLiteral("signer")] = s.signerName;
    o[QStringLiteral("signer_status")] = signerStatusText(s.signerStatus);
}

} // namespace

QByteArray canonicalBytes(const ChallengeResponse &r)
{
    QByteArray b(kChallengeDomain);
    putU8(b, quint8(r.node));
    putU8(b, quint8(r.challengeId.size()));
    b.append(r.challengeId);
    for (quint16 v : r.levels)
        putU16(b, v);
    for (quint16 v : r.readings)
        putU16(b, v);
    putU8(b, r.flags);
    putU8(b, r.firmware);
    putI64(b, r.timestampMs);
    return b;
}

QByteArray canonicalBytes(const CalibrationRecord &c)
{
    QByteArray b(kCalibrationDomain);
    putU8(b, quint8(c.node));
    for (quint16 v : c.data.dark)
        putU16(b, v);
    for (const auto &page : c.data.led)
        for (quint16 v : page)
            putU16(b, v);
    putU8(b, c.flags);
    putU8(b, c.firmware);
    putI64(b, c.timestampMs);
    return b;
}

void sign(ChallengeResponse *r, Signer *signer)
{
    r->sig = signDigest(canonicalBytes(*r), signer);
}

void sign(CalibrationRecord *c, Signer *signer)
{
    c->sig = signDigest(canonicalBytes(*c), signer);
}

QJsonObject toJson(const ChallengeResponse &r)
{
    QJsonObject o;
    o[QStringLiteral("type")] = QStringLiteral("challenge");
    o[QStringLiteral("node")] = r.node;
    o[QStringLiteral("challenge_id")] = QString::fromLatin1(r.challengeId.toHex());
    o[QStringLiteral("levels")] = toArray(r.levels);
    o[QStringLiteral("readings")] = toArray(r.readings);
    o[QStringLiteral("flags")] = int(r.flags);
    o[QStringLiteral("firmware")] = int(r.firmware);
    o[QStringLiteral("ts_ms")] = double(r.timestampMs);
    putSigned(o, r.sig);
    return o;
}

QJsonObject toJson(const CalibrationRecord &c)
{
    QJsonObject o;
    o[QStringLiteral("type")] = QStringLiteral("calibration");
    o[QStringLiteral("node")] = c.node;
    o[QStringLiteral("dark")] = toArray(c.data.dark);
    QJsonArray leds;
    for (const auto &page : c.data.led)
        leds.append(toArray(page));
    o[QStringLiteral("leds")] = leds;
    o[QStringLiteral("flags")] = int(c.flags);
    o[QStringLiteral("firmware")] = int(c.firmware);
    o[QStringLiteral("ts_ms")] = double(c.timestampMs);
    putSigned(o, c.sig);
    return o;
}

} // namespace clorela::laura
