// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DailyValuesRepository.h"
#include "../core/Database.h"

#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

// ── Helpers ───────────────────────────────────────────────────────────────────

DailyValuesObject DailyValuesRepository::fromQuery(const QSqlQuery& sqlQuery) const
{
    return DailyValuesObject(
        sqlQuery.value("share_guid").toString(),
        QDate::fromString(sqlQuery.value("date").toString(), Qt::ISODate),
        sqlQuery.value("opening").toDouble(),
        sqlQuery.value("closing").toDouble(),
        sqlQuery.value("top").toDouble(),
        sqlQuery.value("bottom").toDouble(),
        sqlQuery.value("volume").toDouble()
    );
}

// ── Create / Update ───────────────────────────────────────────────────────────

bool DailyValuesRepository::upsert(const DailyValuesObject& dailyValues)
{
    // INSERT OR REPLACE handles both insert and update for the composite PK (share_guid, date).
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        INSERT OR REPLACE INTO daily_values
            (share_guid, date, opening, closing, top, bottom, volume)
        VALUES
            (:share_guid, :date, :opening, :closing, :top, :bottom, :volume)
    )");

    sqlQuery.bindValue(":share_guid", dailyValues.shareGuid());
    sqlQuery.bindValue(":date",       dailyValues.date().toString(Qt::ISODate));
    sqlQuery.bindValue(":opening",    dailyValues.openingPrice());
    sqlQuery.bindValue(":closing",    dailyValues.closingPrice());
    sqlQuery.bindValue(":top",        dailyValues.top());
    sqlQuery.bindValue(":bottom",     dailyValues.bottom());
    sqlQuery.bindValue(":volume",     dailyValues.volume());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[DailyValuesRepository] upsert failed:" << m_lastError.text();
        return false;
    }
    return true;
}

bool DailyValuesRepository::upsertList(const QList<DailyValuesObject>& dailyValuesList)
{
    // Wrap all inserts in a single transaction for performance.
    Database::instance().beginTransaction();
    for (const auto& dailyValues : dailyValuesList) {
        if (!upsert(dailyValues)) {
            Database::instance().rollbackTransaction();
            return false;
        }
    }
    return Database::instance().commitTransaction();
}

// ── Read ──────────────────────────────────────────────────────────────────────

QList<DailyValuesObject> DailyValuesRepository::findByShare(const QString& shareGuid) const
{
    QList<DailyValuesObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM daily_values WHERE share_guid = :sg ORDER BY date ASC");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[DailyValuesRepository] findByShare failed:" << m_lastError.text();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

QList<DailyValuesObject> DailyValuesRepository::findByShareAndDateRange(
    const QString& shareGuid, const QDate& from, const QDate& to) const
{
    QList<DailyValuesObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT * FROM daily_values
        WHERE share_guid = :sg
          AND date BETWEEN :from AND :to
        ORDER BY date ASC
    )");
    sqlQuery.bindValue(":sg",   shareGuid);
    sqlQuery.bindValue(":from", from.toString(Qt::ISODate));
    sqlQuery.bindValue(":to",   to.toString(Qt::ISODate));

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[DailyValuesRepository] findByShareAndDateRange failed:" << m_lastError.text();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

DailyValuesObject DailyValuesRepository::findByShareAndDate(
    const QString& shareGuid, const QDate& date) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM daily_values WHERE share_guid = :sg AND date = :date LIMIT 1");
    sqlQuery.bindValue(":sg",   shareGuid);
    sqlQuery.bindValue(":date", date.toString(Qt::ISODate));

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return DailyValuesObject{};
    }
    return fromQuery(sqlQuery);
}

QDate DailyValuesRepository::latestDate(const QString& shareGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT MAX(date) FROM daily_values WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next() || sqlQuery.value(0).isNull()) {
        m_lastError = sqlQuery.lastError();
        return QDate{};
    }
    return QDate::fromString(sqlQuery.value(0).toString(), Qt::ISODate);
}

int DailyValuesRepository::count(const QString& shareGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT COUNT(*) FROM daily_values WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0;
    }
    return sqlQuery.value(0).toInt();
}

// ── Delete ────────────────────────────────────────────────────────────────────

bool DailyValuesRepository::remove(const QString& shareGuid, const QDate& date)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM daily_values WHERE share_guid = :sg AND date = :date");
    sqlQuery.bindValue(":sg",   shareGuid);
    sqlQuery.bindValue(":date", date.toString(Qt::ISODate));

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[DailyValuesRepository] remove failed:" << m_lastError.text();
        return false;
    }
    return true;
}

int DailyValuesRepository::removeByShare(const QString& shareGuid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM daily_values WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[DailyValuesRepository] removeByShare failed:" << m_lastError.text();
        return -1;
    }
    return sqlQuery.numRowsAffected();
}
