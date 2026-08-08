// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ShareSplitRepository.h"
#include "../core/Database.h"

#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

// ── Helpers ───────────────────────────────────────────────────────────────────

ShareSplitObject ShareSplitRepository::fromQuery(const QSqlQuery& sqlQuery) const
{
    return ShareSplitObject(
        sqlQuery.value("guid").toString(),
        sqlQuery.value("share_guid").toString(),
        QDate::fromString(sqlQuery.value("date").toString(), Qt::ISODate),
        sqlQuery.value("ratio_new").toDouble(),
        sqlQuery.value("ratio_old").toDouble(),
        sqlQuery.value("prices_adjusted").toBool(),
        sqlQuery.value("comment").toString(),
        sqlQuery.value("document").toString()
    );
}

// ── Create ────────────────────────────────────────────────────────────────────

bool ShareSplitRepository::insert(const ShareSplitObject& split)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        INSERT INTO share_splits
            (guid, share_guid, date, ratio_new, ratio_old, prices_adjusted, comment, document)
        VALUES
            (:guid, :share_guid, :date, :ratio_new, :ratio_old, :prices_adjusted, :comment, :document)
    )");

    sqlQuery.bindValue(":guid",            split.guid());
    sqlQuery.bindValue(":share_guid",      split.shareGuid());
    sqlQuery.bindValue(":date",            split.date().toString(Qt::ISODate));
    sqlQuery.bindValue(":ratio_new",       split.ratioNew());
    sqlQuery.bindValue(":ratio_old",       split.ratioOld());
    sqlQuery.bindValue(":prices_adjusted", split.pricesAdjusted());
    sqlQuery.bindValue(":comment",         split.comment());
    sqlQuery.bindValue(":document",        split.document());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareSplitRepository] insert failed:" << m_lastError.text();
        return false;
    }
    return true;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QList<ShareSplitObject> ShareSplitRepository::findByShare(const QString& shareGuid) const
{
    QList<ShareSplitObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM share_splits WHERE share_guid = :sg ORDER BY date ASC");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareSplitRepository] findByShare failed:" << m_lastError.text();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

ShareSplitObject ShareSplitRepository::findByGuid(const QString& guid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM share_splits WHERE guid = :guid LIMIT 1");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return ShareSplitObject{};
    }
    return fromQuery(sqlQuery);
}

bool ShareSplitRepository::existsForDate(const QString& shareGuid, const QDate& date) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT COUNT(*) FROM share_splits WHERE share_guid = :sg AND date = :date");
    sqlQuery.bindValue(":sg",   shareGuid);
    sqlQuery.bindValue(":date", date.toString(Qt::ISODate));

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return sqlQuery.value(0).toInt() > 0;
}

// ── Update ────────────────────────────────────────────────────────────────────

bool ShareSplitRepository::update(const ShareSplitObject& split)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        UPDATE share_splits SET
            date            = :date,
            ratio_new       = :ratio_new,
            ratio_old       = :ratio_old,
            prices_adjusted = :prices_adjusted,
            comment         = :comment,
            document        = :document
        WHERE guid = :guid
    )");

    sqlQuery.bindValue(":guid",            split.guid());
    sqlQuery.bindValue(":date",            split.date().toString(Qt::ISODate));
    sqlQuery.bindValue(":ratio_new",       split.ratioNew());
    sqlQuery.bindValue(":ratio_old",       split.ratioOld());
    sqlQuery.bindValue(":prices_adjusted", split.pricesAdjusted());
    sqlQuery.bindValue(":comment",         split.comment());
    sqlQuery.bindValue(":document",        split.document());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareSplitRepository] update failed:" << m_lastError.text();
        return false;
    }
    return true;
}

bool ShareSplitRepository::updateDocument(const QString& guid, const QString& document)
{
    QSqlQuery sqlQuery(QSqlDatabase::database(Database::connectionName()));
    sqlQuery.prepare("UPDATE share_splits SET document = :doc WHERE guid = :guid");
    sqlQuery.bindValue(":doc",  document);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareSplitRepository] updateDocument failed:" << m_lastError.text();
        return false;
    }
    return true;
}

// ── Delete ────────────────────────────────────────────────────────────────────

bool ShareSplitRepository::remove(const QString& guid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM share_splits WHERE guid = :guid");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareSplitRepository] remove failed:" << m_lastError.text();
        return false;
    }
    return true;
}

int ShareSplitRepository::removeByShare(const QString& shareGuid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM share_splits WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareSplitRepository] removeByShare failed:" << m_lastError.text();
        return -1;
    }
    return sqlQuery.numRowsAffected();
}
