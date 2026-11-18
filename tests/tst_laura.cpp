// SPDX-License-Identifier: MIT
// Laura Encoding: 'X' and 'C' against the simulated node, the signed record
// encoding and the control socket commands.
#include "acquisitionservice.h"
#include "controlserver.h"
#include "lauraencoding.h"
#include "nodeclient.h"
#include "signer.h"
#include "simulatednode.h"

#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>
#include <random>

using namespace clorela;

namespace {

// Server-side model: y = dark + sum_j (level_j / 4095) * (led_j - dark).
std::array<double, 16> predict(const NodeClient::Calibration &c, const NodeClient::Levels &levels)
{
    std::array<double, 16> y{};
    for (int i = 0; i < 16; ++i) {
        y[i] = c.dark[i];
        for (int j = 0; j < node::kVisibleLeds; ++j)
            y[i] += levels[j] / double(node::kMaxLevel) * (double(c.led[j][i]) - c.dark[i]);
    }
    return y;
}

double rmsResidual(const NodeClient::Readings &r, const std::array<double, 16> &pred)
{
    double s = 0;
    for (int i = 0; i < 16; ++i)
        s += (r[i] - pred[i]) * (r[i] - pred[i]);
    return std::sqrt(s / 16);
}

NodeClient::Levels randomLevels(std::mt19937 &rng)
{
    std::uniform_int_distribution<int> d(0, 1500); // stay below sensor saturation
    NodeClient::Levels l{};
    for (auto &v : l)
        v = quint16(d(rng));
    return l;
}

} // namespace

class TestLaura : public QObject
{
    Q_OBJECT

private slots:
    void patternFollowsTheCalibratedMatrix()
    {
        SimulatedNodeBus bus({0x10});
        bus.setGrowth(0x10, false); // keep M fixed for this check
        NodeClient c(&bus, 0x10);
        const auto cal = c.calibrate();
        QVERIFY2(cal, qPrintable(c.lastError()));

        std::mt19937 rng(7);
        for (int k = 0; k < 20; ++k) {
            const auto levels = randomLevels(rng);
            node::Status after;
            const auto r = c.measurePattern(levels, &after);
            QVERIFY2(r, qPrintable(c.lastError()));
            QCOMPARE(after.lastCommand, node::CmdPattern);
            QVERIFY2(rmsResidual(*r, predict(*cal, levels)) < 6.0, "reading does not match M x + b");
        }
    }

    void wrongPatternIsRejected()
    {
        SimulatedNodeBus bus({0x10});
        bus.setGrowth(0x10, false);
        NodeClient c(&bus, 0x10);
        const auto cal = c.calibrate();
        QVERIFY(cal);
        std::mt19937 rng(11);
        const auto asked = randomLevels(rng);
        const auto other = randomLevels(rng);
        const auto r = c.measurePattern(other, nullptr); // answer to a different challenge
        QVERIFY(r);
        QVERIFY(rmsResidual(*r, predict(*cal, asked)) > 20.0);
    }

    void levelsOutOfRangeAreRejected()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        NodeClient::Levels l{};
        l[3] = 4096;
        QVERIFY(!c.measurePattern(l));
        // A malformed frame reaches the node and is refused there.
        QVERIFY(c.command(node::CmdPattern, QByteArray(10, '\0')));
        QCOMPARE(c.status()->error, node::ErrInvalidArgument);
    }

    void everyNodeHasItsOwnFingerprint()
    {
        SimulatedNodeBus bus({0x10, 0x11});
        bus.setGrowth(0x10, false);
        bus.setGrowth(0x11, false);
        NodeClient a(&bus, 0x10), b(&bus, 0x11);
        const auto ca = a.calibrate(), cb = b.calibrate();
        QVERIFY(ca && cb);
        int maxDiff = 0;
        for (int j = 0; j < node::kVisibleLeds; ++j)
            for (int i = 0; i < 16; ++i)
                maxDiff = qMax(maxDiff, std::abs(int(ca->led[j][i]) - int(cb->led[j][i])));
        QVERIFY2(maxDiff > 20, "two nodes produced the same matrix");
    }

    void digestIsCanonicalAndUnsignedWithoutChip()
    {
        laura::ChallengeResponse r;
        r.node = 0x10;
        r.challengeId = QByteArray::fromHex("00112233");
        r.levels.fill(100);
        r.readings.fill(500);
        r.firmware = 2;
        r.timestampMs = 1795000000000;
        NullSigner signer;
        laura::sign(&r, &signer);
        QCOMPARE(r.sig.digest.size(), 32);
        QVERIFY(r.sig.signature.isEmpty());
        QCOMPARE(r.sig.signerStatus, SignerStatus::NotAvailable);

        const QByteArray d1 = r.sig.digest;
        laura::sign(&r, &signer);
        QCOMPARE(r.sig.digest, d1); // deterministic
        r.readings[5] = 501;
        laura::sign(&r, &signer);
        QVERIFY(r.sig.digest != d1); // any change in the data changes the digest

        const QByteArray bytes = laura::canonicalBytes(r);
        QVERIFY(bytes.startsWith(laura::kChallengeDomain));
        QCOMPARE(bytes.size(), int(sizeof(laura::kChallengeDomain) - 1) + 1 + 1 + 4 + 24 + 32 + 1 + 1 + 8);
    }

    void controlSocketCommands()
    {
        QTemporaryDir dir;
        Config cfg;
        cfg.sensor = QStringLiteral("sim");
        cfg.nodeAddresses = {0x10};
        cfg.databasePath = dir.filePath(QStringLiteral("m.db"));
        cfg.requireNetwork = false;
        AcquisitionService svc(cfg, std::make_unique<SimulatedNodeBus>(cfg.nodeAddresses));
        NullSigner signer;
        svc.setSigner(&signer);
        ControlServer control(&svc, cfg);

        auto req = [](const char *json) { return QJsonDocument::fromJson(json).object(); };

        QJsonObject cal = control.handle(req(R"({"cmd":"laura_calibrate","node":16})"));
        QVERIFY2(cal.value("ok").toBool(), qPrintable(cal.value("error").toString()));
        const QJsonObject calRec = cal.value("laura").toObject();
        QCOMPARE(calRec.value("type").toString(), QStringLiteral("calibration"));
        QCOMPARE(calRec.value("leds").toArray().size(), 12);
        QCOMPARE(calRec.value("digest").toString().size(), 64);
        QVERIFY(calRec.value("signature").isNull());

        QJsonObject ch = control.handle(req(
            R"({"cmd":"laura_challenge","node":16,"challenge_id":"a1b2c3d4","levels":[1,2,3,4,5,6,7,8,9,10,11,4095]})"));
        QVERIFY2(ch.value("ok").toBool(), qPrintable(ch.value("error").toString()));
        const QJsonObject rec = ch.value("laura").toObject();
        QCOMPARE(rec.value("readings").toArray().size(), 16);
        QCOMPARE(rec.value("challenge_id").toString(), QStringLiteral("a1b2c3d4"));
        QCOMPARE(rec.value("firmware").toInt(), int(node::kFirmwarePattern));

        QVERIFY(!control.handle(req(R"({"cmd":"laura_challenge","node":17,"challenge_id":"aa","levels":[0,0,0,0,0,0,0,0,0,0,0,0]})"))
                     .value("ok").toBool()); // node not configured
        QVERIFY(!control.handle(req(R"({"cmd":"laura_challenge","node":16,"challenge_id":"xyz","levels":[0,0,0,0,0,0,0,0,0,0,0,0]})"))
                     .value("ok").toBool()); // bad hex
        QVERIFY(!control.handle(req(R"({"cmd":"laura_challenge","node":16,"challenge_id":"aa","levels":[0,0,0]})"))
                     .value("ok").toBool()); // wrong length
        QVERIFY(!control.handle(req(R"({"cmd":"laura_challenge","node":16,"challenge_id":"aa","levels":[0,0,0,0,0,0,0,0,0,0,0,5000]})"))
                     .value("ok").toBool()); // level out of range
    }

    void sterilizingNodeDoesNotAnswer()
    {
        SimulatedNodeBus bus({0x10});
        NodeClient c(&bus, 0x10);
        QVERIFY(c.startUv(60));
        NodeClient::Levels l{};
        QVERIFY(!c.measurePattern(l));
        QVERIFY(!c.calibrate());
    }
};

QTEST_GUILESS_MAIN(TestLaura)
#include "tst_laura.moc"
