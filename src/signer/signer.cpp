// SPDX-License-Identifier: MIT
#include "signer.h"

#include "config.h"

#include <QCryptographicHash>

namespace clorela {

namespace {
// floor(n / 2) for the P-256 group order n.
const QByteArray kP256HalfOrder = QByteArray::fromHex(
    "7fffffff800000007fffffffffffffffde737d56d38bcf4279dce5617e3192a8");
constexpr int kMaxRandomBytes = 256;
} // namespace

QString signerStatusText(SignerStatus status)
{
    switch (status) {
    case SignerStatus::Ok: return QStringLiteral("ok");
    case SignerStatus::NotAvailable: return QStringLiteral("not available");
    case SignerStatus::NotProvisioned: return QStringLiteral("not provisioned");
    case SignerStatus::InvalidInput: return QStringLiteral("invalid input");
    case SignerStatus::HardwareError: return QStringLiteral("hardware error");
    }
    return QStringLiteral("unknown");
}

SignerStatus Signer::anchorPublicKey(PublicKey *out)
{
    if (!out)
        return SignerStatus::InvalidInput;
    const SignerStatus st = doAnchorPublicKey(out);
    if (st == SignerStatus::Ok && !out->isValid()) {
        m_lastError = QStringLiteral("backend returned a malformed public key");
        return SignerStatus::HardwareError;
    }
    return st;
}

SignerStatus Signer::signDigest(const QByteArray &digest32, Signature *out)
{
    if (!out || digest32.size() != 32) {
        m_lastError = QStringLiteral("digest must be exactly 32 bytes");
        return SignerStatus::InvalidInput;
    }
    const SignerStatus st = doSignDigest(digest32, out);
    if (st != SignerStatus::Ok)
        return st;
    // Enforce the contract even if a backend forgets it.
    if (out->r.size() != 32 || out->s.size() != 32 || !isLowS(out->s)) {
        m_lastError = QStringLiteral("backend returned a malformed or high-s signature");
        return SignerStatus::HardwareError;
    }
    return st;
}

SignerStatus Signer::randomBytes(int count, QByteArray *out)
{
    if (!out || count <= 0 || count > kMaxRandomBytes) {
        m_lastError = QStringLiteral("count must be 1-%1").arg(kMaxRandomBytes);
        return SignerStatus::InvalidInput;
    }
    const SignerStatus st = doRandomBytes(count, out);
    if (st == SignerStatus::Ok && out->size() != count) {
        m_lastError = QStringLiteral("backend returned %1 bytes instead of %2").arg(out->size()).arg(count);
        return SignerStatus::HardwareError;
    }
    return st;
}

QByteArray Signer::anchorIdentifier(const PublicKey &key)
{
    if (!key.isValid())
        return QByteArray();
    QByteArray packed(1, char(uint8_t(key.curve)));
    packed += key.x;
    packed += key.y;
    // Keccak-256 as used by Ethereum/Polygon (not FIPS SHA3-256).
    return QCryptographicHash::hash(packed, QCryptographicHash::Keccak_256);
}

bool Signer::isLowS(const QByteArray &s)
{
    if (s.size() != 32)
        return false;
    // Big-endian byte strings of equal length compare like the numbers.
    for (int i = 0; i < 32; ++i) {
        const uint8_t a = uint8_t(s.at(i));
        const uint8_t b = uint8_t(kP256HalfOrder.at(i));
        if (a != b)
            return a < b;
    }
    return true; // equal to n/2
}

namespace {
SignerStatus notAvailable(QString *err)
{
    *err = QStringLiteral("no secure element: the TROPIC01 is not integrated yet");
    return SignerStatus::NotAvailable;
}
} // namespace

SignerStatus NullSigner::doAnchorPublicKey(PublicKey *)
{
    return notAvailable(&m_lastError);
}

SignerStatus NullSigner::doSignDigest(const QByteArray &, Signature *)
{
    return notAvailable(&m_lastError);
}

SignerStatus NullSigner::doRandomBytes(int, QByteArray *)
{
    return notAvailable(&m_lastError);
}

std::unique_ptr<Signer> createSigner(const Config &config)
{
    // Future: if (config.signerBackend == "tropic01") return std::make_unique<Tropic01Signer>(...);
    Q_UNUSED(config);
    return std::make_unique<NullSigner>();
}

} // namespace clorela
