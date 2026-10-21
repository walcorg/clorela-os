// SPDX-License-Identifier: MIT
#include "measurementstore.h"

#include <QAtomicInt>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace clorela {

namespace {
QAtomicInt g_connectionCounter;

QString channelColumns()
{
    QStringList cols;
    for (int i = 0; i < kSensorCount; ++i)
        cols << QStringLiteral("ch%1").arg(i);
    return cols.join(QLatin1Char(','));
}
} // namespace

MeasurementStore::MeasurementStore()
    : m_connection(QStringLiteral("clorela-store-%1").arg(g_connectionCounter.fetchAndAddRelaxed(1)))
{
}

MeasurementStore::~MeasurementStore()
{
    close();
}

bool MeasurementStore::open(const QString &path)
{
    close();
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        m_lastError = QStringLiteral("cannot create directory %1").arg(info.absolutePath());
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!db.open()) {
        m_lastError = db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connection);
        return false;
    }

    QStringList chDefs;
    for (int i = 0; i < kSensorCount; ++i)
        chDefs << QStringLiteral("ch%1 INTEGER NOT NULL").arg(i);

    const bool ok =
        exec(QStringLiteral("PRAGMA journal_mode=WAL"))
        && exec(QStringLiteral("PRAGMA synchronous=NORMAL"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS measurement ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "cycle INTEGER NOT NULL,"
                               "ts_ms INTEGER NOT NULL,"
                               "node INTEGER NOT NULL,"
                               "kind TEXT NOT NULL,"
                               "param INTEGER NOT NULL,"
                               "flags INTEGER NOT NULL,%1)")
                    .arg(chDefs.join(QLatin1Char(','))))
        && exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_measurement_cycle ON measurement(cycle)"))
        && exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_measurement_ts ON measurement(ts_ms)"))
        && exec(QStringLiteral("INSERT OR IGNORE INTO meta(key, value) VALUES ('schema_version', '%1')")
                    .arg(kSchemaVersion));
    if (!ok)
        close();
    return ok;
}

void MeasurementStore::close()
{
    if (QSqlDatabase::contains(m_connection)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connection, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connection);
    }
}

bool MeasurementStore::isOpen() const
{
    return QSqlDatabase::contains(m_connection)
           && QSqlDatabase::database(m_connection, false).isOpen();
}

bool MeasurementStore::exec(const QString &sql)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

qint64 MeasurementStore::nextCycleId()
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (q.exec(QStringLiteral("SELECT COALESCE(MAX(cycle), 0) + 1 FROM measurement")) && q.next())
        return q.value(0).toLongLong();
    return 1;
}

bool MeasurementStore::insert(const Measurement &m)
{
    return insertBatch({m});
}

bool MeasurementStore::insertBatch(const QList<Measurement> &batch)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("store not open");
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }

    QStringList placeholders;
    for (int i = 0; i < 6 + kSensorCount; ++i)
        placeholders << QStringLiteral("?");
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO measurement (cycle, ts_ms, node, kind, param, flags, %1) VALUES (%2)")
                  .arg(channelColumns(), placeholders.join(QLatin1Char(','))));

    for (const Measurement &m : batch) {
        q.addBindValue(m.cycleId);
        q.addBindValue(m.timestampMs);
        q.addBindValue(m.node);
        q.addBindValue(Measurement::kindToString(m.kind));
        q.addBindValue(m.param);
        q.addBindValue(int(m.flags));
        for (quint16 v : m.values)
            q.addBindValue(int(v));
        if (!q.exec()) {
            m_lastError = q.lastError().text();
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

QList<Measurement> MeasurementStore::select(const QString &where, const QVariantList &binds)
{
    QList<Measurement> out;
    if (!isOpen())
        return out;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("SELECT cycle, ts_ms, node, kind, param, flags, %1 FROM measurement %2 ORDER BY id")
                  .arg(channelColumns(), where));
    for (const QVariant &b : binds)
        q.addBindValue(b);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return out;
    }
    while (q.next()) {
        Measurement m;
        m.cycleId = q.value(0).toLongLong();
        m.timestampMs = q.value(1).toLongLong();
        m.node = q.value(2).toInt();
        m.kind = Measurement::kindFromString(q.value(3).toString());
        m.param = q.value(4).toInt();
        m.flags = quint8(q.value(5).toInt());
        for (int i = 0; i < kSensorCount; ++i)
            m.values[i] = quint16(q.value(6 + i).toInt());
        out.append(m);
    }
    return out;
}

QList<Measurement> MeasurementStore::latestCycle(int node)
{
    if (node < 0)
        return select(QStringLiteral("WHERE cycle = (SELECT MAX(cycle) FROM measurement)"), {});
    return select(QStringLiteral("WHERE node = ? AND cycle = (SELECT MAX(cycle) FROM measurement WHERE node = ?)"),
                  {node, node});
}

QList<Measurement> MeasurementStore::recentCycles(int cycles, int node)
{
    if (node < 0)
        return select(QStringLiteral("WHERE cycle > (SELECT COALESCE(MAX(cycle), 0) - ? FROM measurement)"),
                      {cycles});
    return select(QStringLiteral("WHERE node = ? AND cycle > (SELECT COALESCE(MAX(cycle), 0) - ? FROM measurement WHERE node = ?)"),
                  {node, cycles, node});
}

qint64 MeasurementStore::count()
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM measurement")) && q.next())
        return q.value(0).toLongLong();
    return 0;
}

} // namespace clorela
