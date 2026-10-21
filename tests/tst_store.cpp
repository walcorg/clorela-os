// SPDX-License-Identifier: MIT
#include "measurementstore.h"

#include <QTemporaryDir>
#include <QTest>

using clorela::Measurement;
using clorela::MeasurementStore;

namespace {
Measurement make(qint64 cycle, int node, Measurement::Kind kind, int param, quint16 base)
{
    Measurement m;
    m.cycleId = cycle;
    m.timestampMs = 1'760'000'000'000 + cycle * 1000;
    m.node = node;
    m.kind = kind;
    m.param = param;
    m.flags = 0x01;
    for (int i = 0; i < clorela::kSensorCount; ++i)
        m.values[i] = quint16(base + i);
    return m;
}
} // namespace

class TstStore : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip()
    {
        QTemporaryDir dir;
        MeasurementStore store;
        QVERIFY2(store.open(dir.filePath(QStringLiteral("sub/m.db"))), qPrintable(store.lastError()));
        QCOMPARE(store.nextCycleId(), 1);

        QList<Measurement> batch{make(1, 0x10, Measurement::Kind::Dark, -1, 5),
                                 make(1, 0x10, Measurement::Kind::Led, 3, 500)};
        QVERIFY2(store.insertBatch(batch), qPrintable(store.lastError()));
        QCOMPARE(store.count(), 2);
        QCOMPARE(store.nextCycleId(), 2);

        const QList<Measurement> back = store.latestCycle();
        QCOMPARE(back.size(), 2);
        QCOMPARE(back.at(1).kind, Measurement::Kind::Led);
        QCOMPARE(back.at(1).param, 3);
        QCOMPARE(back.at(1).values[15], quint16(515));
        QCOMPARE(back.at(0).flags, quint8(1));
    }

    void recentCyclesAndNodeFilter()
    {
        QTemporaryDir dir;
        MeasurementStore store;
        QVERIFY(store.open(dir.filePath(QStringLiteral("m.db"))));
        for (int c = 1; c <= 5; ++c) {
            QVERIFY(store.insert(make(c, 0x10, Measurement::Kind::Led, 0, quint16(c))));
            QVERIFY(store.insert(make(c, 0x11, Measurement::Kind::Led, 0, quint16(100 + c))));
        }
        QCOMPARE(store.recentCycles(2).size(), 4);
        QCOMPARE(store.recentCycles(2).first().cycleId, 4);
        QCOMPARE(store.latestCycle(0x11).size(), 1);
        QCOMPARE(store.latestCycle(0x11).first().values[0], quint16(105));
    }

    void twoConnectionsSameFile()
    {
        // Writer (daemon) and reader (interface) open the same file.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("m.db"));
        MeasurementStore writer, reader;
        QVERIFY(writer.open(path));
        QVERIFY(reader.open(path));
        QVERIFY(writer.insert(make(1, 0x10, Measurement::Kind::Dark, -1, 1)));
        QCOMPARE(reader.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TstStore)
#include "tst_store.moc"
