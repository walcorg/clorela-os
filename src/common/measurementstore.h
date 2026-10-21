// SPDX-License-Identifier: MIT
// Local measurement storage (SQLite, WAL mode).
//
// clorela-acqd is the only writer; clorela-ui only reads. Each instance owns
// its own QSqlDatabase connection, so one instance per thread.
#pragma once

#include "measurement.h"

#include <QList>
#include <QString>

namespace clorela {

class MeasurementStore
{
public:
    static constexpr int kSchemaVersion = 1;

    MeasurementStore();
    ~MeasurementStore();
    MeasurementStore(const MeasurementStore &) = delete;
    MeasurementStore &operator=(const MeasurementStore &) = delete;

    // Creates the parent directory and the schema if needed.
    bool open(const QString &path);
    void close();
    bool isOpen() const;
    QString lastError() const { return m_lastError; }

    // Returns a new cycle id (max + 1).
    qint64 nextCycleId();
    bool insert(const Measurement &m);
    bool insertBatch(const QList<Measurement> &batch);

    // Most recent cycle (all its readings), optionally for one node only (-1 = any).
    QList<Measurement> latestCycle(int node = -1);
    // Readings of the last `cycles` cycles, oldest first.
    QList<Measurement> recentCycles(int cycles, int node = -1);
    qint64 count();

private:
    bool exec(const QString &sql);
    QList<Measurement> select(const QString &where, const QVariantList &binds);

    QString m_connection;
    QString m_lastError;
};

} // namespace clorela
