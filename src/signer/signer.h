// SPDX-License-Identifier: MIT
// Signing extension point for the TROPIC01 secure element on the shield.
//
// NOT IMPLEMENTED YET. The shield v4.1 has no TROPIC01 and Clorela-OS ships
// only NullSigner, which reports NotAvailable. This interface fixes what the
// rest of Clorela-OS may rely on, so the Anchor Protocol
// (github.com/walcorg/anchor-protocol, draft v0.2) can be built against it
// before the chip is on the board. See docs/signer-tropic01.md.
//
// Contract for every implementation:
//   - The private key never leaves the chip; only the public key is exported.
//   - signDigest() signs the 32-byte digest exactly as given (the EIP-712
//     digest of a CaptureReceipt or Bind) and returns r || s, 32 bytes each,
//     big-endian, with s normalised to the lower half of the curve order.
//   - Input is validated here (Signer), before any backend code runs.
#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <memory>

namespace clorela {

struct Config;

// Curve identifiers as in the Anchor Protocol `curve` field.
enum class Curve : uint8_t {
    P256 = 1,      // secp256r1, verified on Polygon with P256VERIFY
    Secp256k1 = 2, // reserved until TROPIC01 firmware supports it
};

struct PublicKey
{
    Curve curve = Curve::P256;
    QByteArray x; // 32 bytes, big-endian
    QByteArray y; // 32 bytes, big-endian
    bool isValid() const { return x.size() == 32 && y.size() == 32; }
};

struct Signature
{
    QByteArray r; // 32 bytes, big-endian
    QByteArray s; // 32 bytes, big-endian, low-s
    QByteArray toRS() const { return r + s; }
};

enum class SignerStatus {
    Ok,
    NotAvailable,   // no secure element (NullSigner, or chip not detected)
    NotProvisioned, // chip present but no anchor key in its slot yet
    InvalidInput,   // e.g. digest is not 32 bytes
    HardwareError,  // SPI or secure-session failure; see lastError()
};

QString signerStatusText(SignerStatus status);

class Signer
{
public:
    virtual ~Signer() = default;

    virtual QString name() const = 0;
    // True if signatures can be produced right now.
    virtual bool isAvailable() const = 0;

    SignerStatus anchorPublicKey(PublicKey *out);
    SignerStatus signDigest(const QByteArray &digest32, Signature *out);
    // Random bytes from the chip's TRNG (CaptureReceipt.nonce is 16 bytes).
    SignerStatus randomBytes(int count, QByteArray *out);

    QString lastError() const { return m_lastError; }

    // shieldId / ciwiId = keccak256(curve || pubX || pubY), Anchor spec section 2.
    static QByteArray anchorIdentifier(const PublicKey &key);
    // True if s <= n/2 for P-256 (s is 32 bytes, big-endian).
    static bool isLowS(const QByteArray &s);

protected:
    virtual SignerStatus doAnchorPublicKey(PublicKey *out) = 0;
    virtual SignerStatus doSignDigest(const QByteArray &digest32, Signature *out) = 0;
    virtual SignerStatus doRandomBytes(int count, QByteArray *out) = 0;

    QString m_lastError;
};

// Default: there is no secure element. Every operation returns NotAvailable.
class NullSigner : public Signer
{
public:
    QString name() const override { return QStringLiteral("none"); }
    bool isAvailable() const override { return false; }

protected:
    SignerStatus doAnchorPublicKey(PublicKey *) override;
    SignerStatus doSignDigest(const QByteArray &, Signature *) override;
    SignerStatus doRandomBytes(int, QByteArray *) override;
};

// Returns the signer selected by [signer] backend in the configuration.
// Only "none" exists today; "tropic01" will be added with the hardware.
std::unique_ptr<Signer> createSigner(const Config &config);

} // namespace clorela
