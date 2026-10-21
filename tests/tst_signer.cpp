// SPDX-License-Identifier: MIT
// Contract of the signing extension point. A future Tropic01Signer must pass
// the same checks (see FakeSigner below for what "passing" means).
#include "config.h"
#include "signer.h"

#include <QCryptographicHash>
#include <QTest>

using namespace clorela;

namespace {

// Test double that lets the base-class checks be exercised.
class FakeSigner : public Signer
{
public:
    QByteArray rToReturn = QByteArray(32, '\x01');
    QByteArray sToReturn = QByteArray(32, '\x02');
    int randomLength = -1;
    int backendCalls = 0;

    QString name() const override { return QStringLiteral("fake"); }
    bool isAvailable() const override { return true; }

protected:
    SignerStatus doAnchorPublicKey(PublicKey *out) override
    {
        ++backendCalls;
        out->curve = Curve::P256;
        out->x = QByteArray(32, '\xAA');
        out->y = QByteArray(32, '\xBB');
        return SignerStatus::Ok;
    }
    SignerStatus doSignDigest(const QByteArray &, Signature *out) override
    {
        ++backendCalls;
        out->r = rToReturn;
        out->s = sToReturn;
        return SignerStatus::Ok;
    }
    SignerStatus doRandomBytes(int count, QByteArray *out) override
    {
        ++backendCalls;
        *out = QByteArray(randomLength < 0 ? count : randomLength, '\x05');
        return SignerStatus::Ok;
    }
};

} // namespace

class TstSigner : public QObject
{
    Q_OBJECT

private slots:
    void nullSignerIsNotAvailable()
    {
        auto signer = createSigner(Config{});
        QCOMPARE(signer->name(), QStringLiteral("none"));
        QVERIFY(!signer->isAvailable());

        PublicKey key;
        QCOMPARE(signer->anchorPublicKey(&key), SignerStatus::NotAvailable);
        Signature sig;
        QCOMPARE(signer->signDigest(QByteArray(32, '\0'), &sig), SignerStatus::NotAvailable);
        QByteArray rnd;
        QCOMPARE(signer->randomBytes(16, &rnd), SignerStatus::NotAvailable);
        QVERIFY(signer->lastError().contains(QStringLiteral("TROPIC01")));
    }

    void inputIsValidatedBeforeTheBackend()
    {
        FakeSigner f;
        Signature sig;
        QCOMPARE(f.signDigest(QByteArray(31, '\0'), &sig), SignerStatus::InvalidInput);
        QCOMPARE(f.signDigest(QByteArray(33, '\0'), &sig), SignerStatus::InvalidInput);
        QCOMPARE(f.signDigest(QByteArray(32, '\0'), nullptr), SignerStatus::InvalidInput);
        QByteArray rnd;
        QCOMPARE(f.randomBytes(0, &rnd), SignerStatus::InvalidInput);
        QCOMPARE(f.randomBytes(1000, &rnd), SignerStatus::InvalidInput);
        QCOMPARE(f.backendCalls, 0);
    }

    void validSignaturePasses()
    {
        FakeSigner f;
        Signature sig;
        QCOMPARE(f.signDigest(QByteArray(32, '\x11'), &sig), SignerStatus::Ok);
        QCOMPARE(sig.toRS().size(), 64);
    }

    void highSIsRejected()
    {
        FakeSigner f;
        f.sToReturn = QByteArray(32, '\xFF');
        Signature sig;
        QCOMPARE(f.signDigest(QByteArray(32, '\x11'), &sig), SignerStatus::HardwareError);
    }

    void malformedBackendOutputIsRejected()
    {
        FakeSigner f;
        f.rToReturn = QByteArray(31, '\x01');
        Signature sig;
        QCOMPARE(f.signDigest(QByteArray(32, '\x11'), &sig), SignerStatus::HardwareError);
        f.randomLength = 3;
        QByteArray rnd;
        QCOMPARE(f.randomBytes(16, &rnd), SignerStatus::HardwareError);
    }

    void lowSBoundary()
    {
        const QByteArray half = QByteArray::fromHex("7fffffff800000007fffffffffffffffde737d56d38bcf4279dce5617e3192a8");
        QVERIFY(Signer::isLowS(half));
        QByteArray above = half;
        above[31] = char(0xA9);
        QVERIFY(!Signer::isLowS(above));
        QVERIFY(Signer::isLowS(QByteArray(32, '\0')));
        QVERIFY(!Signer::isLowS(QByteArray(31, '\0')));
    }

    void keccakIsEthereumKeccak()
    {
        // keccak256("") as used on Ethereum/Polygon; differs from SHA3-256("").
        QCOMPARE(QCryptographicHash::hash(QByteArray(), QCryptographicHash::Keccak_256).toHex(),
                 QByteArray("c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"));
    }

    void anchorIdentifier()
    {
        FakeSigner f;
        PublicKey key;
        QCOMPARE(f.anchorPublicKey(&key), SignerStatus::Ok);
        const QByteArray id = Signer::anchorIdentifier(key);
        QCOMPARE(id.size(), 32);
        QCOMPARE(id, QCryptographicHash::hash(QByteArray(1, '\x01') + key.x + key.y, QCryptographicHash::Keccak_256));
        key.curve = Curve::Secp256k1;
        QVERIFY(Signer::anchorIdentifier(key) != id); // curve is part of the identity
        QVERIFY(Signer::anchorIdentifier(PublicKey{}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstSigner)
#include "tst_signer.moc"
