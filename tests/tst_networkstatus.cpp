// SPDX-License-Identifier: MIT
#include "networkstatus.h"

#include <QTest>

using clorela::NetworkStatus;

static const char *kHeader =
    "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n";

class TstNetworkStatus : public QObject
{
    Q_OBJECT

private slots:
    void defaultRouteOnWlan()
    {
        const QString t = QString::fromLatin1(kHeader)
            + QStringLiteral("wlan0\t00000000\t0101A8C0\t0003\t0\t0\t600\t00000000\t0\t0\t0\n"
                             "wlan0\t0001A8C0\t00000000\t0001\t0\t0\t600\t00FFFFFF\t0\t0\t0\n");
        QVERIFY(NetworkStatus::hasDefaultRoute(t));
    }

    void onlyLocalSubnet()
    {
        const QString t = QString::fromLatin1(kHeader)
            + QStringLiteral("wlan0\t0001A8C0\t00000000\t0001\t0\t0\t600\t00FFFFFF\t0\t0\t0\n");
        QVERIFY(!NetworkStatus::hasDefaultRoute(t));
    }

    void emptyTable()
    {
        QVERIFY(!NetworkStatus::hasDefaultRoute(QString::fromLatin1(kHeader)));
        QVERIFY(!NetworkStatus::hasDefaultRoute(QString()));
    }

    void forcedByEnvironment()
    {
        qputenv("CLORELA_NETWORK", "offline");
        QVERIFY(!NetworkStatus::isOnline());
        qputenv("CLORELA_NETWORK", "online");
        QVERIFY(NetworkStatus::isOnline());
        qunsetenv("CLORELA_NETWORK");
    }
};

QTEST_GUILESS_MAIN(TstNetworkStatus)
#include "tst_networkstatus.moc"
