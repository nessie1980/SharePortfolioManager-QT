// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "BrokerageRepository.h"
#include "../core/Database.h"

#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

// ── Helpers ───────────────────────────────────────────────────────────────────

BrokerageObject BrokerageRepository::fromQuery(const QSqlQuery& sqlQuery) const
{
    return BrokerageObject(
        sqlQuery.value("guid").toString(),
        sqlQuery.value("share_guid").toString(),
        sqlQuery.value("buy_guid").toString(),
        sqlQuery.value("sale_guid").toString(),
        sqlQuery.value("datetime").toString(),
        sqlQuery.value("provision").toDouble(),
        sqlQuery.value("broker_fee").toDouble(),
        sqlQuery.value("trader_fee").toDouble(),
        sqlQuery.value("reduction").toDouble(),
        sqlQuery.value("document").toString()
    );
}

// ── Create ────────────────────────────────────────────────────────────────────

bool BrokerageRepository::insert(const BrokerageObject& brokerage)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        INSERT INTO brokerage
            (guid, share_guid, buy_guid, sale_guid, datetime,
             provision, broker_fee, trader_fee, reduction, document)
        VALUES
            (:guid, :share_guid, :buy_guid, :sale_guid, :datetime,
             :provision, :broker_fee, :trader_fee, :reduction, :document)
    )");

    sqlQuery.bindValue(":guid",       brokerage.guid());
    sqlQuery.bindValue(":share_guid", brokerage.shareGuid());
    sqlQuery.bindValue(":buy_guid",   brokerage.buyGuid().isEmpty()  ? QVariant() : brokerage.buyGuid());
    sqlQuery.bindValue(":sale_guid",  brokerage.saleGuid().isEmpty() ? QVariant() : brokerage.saleGuid());
    sqlQuery.bindValue(":datetime",   brokerage.dateTime());
    sqlQuery.bindValue(":provision",  brokerage.provision());
    sqlQuery.bindValue(":broker_fee", brokerage.brokerFee());
    sqlQuery.bindValue(":trader_fee", brokerage.traderFee());
    sqlQuery.bindValue(":reduction",  brokerage.reduction());
    sqlQuery.bindValue(":document",   brokerage.document());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BrokerageRepository] insert failed:" << m_lastError.text();
        return false;
    }
    return true;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QList<BrokerageObject> BrokerageRepository::findByShare(const QString& shareGuid) const
{
    QList<BrokerageObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM brokerage WHERE share_guid = :sg ORDER BY datetime ASC");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BrokerageRepository] findByShare failed:" << m_lastError.text();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

BrokerageObject BrokerageRepository::findByGuid(const QString& guid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM brokerage WHERE guid = :guid LIMIT 1");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return BrokerageObject{};
    }
    return fromQuery(sqlQuery);
}

BrokerageObject BrokerageRepository::findByBuyGuid(const QString& buyGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM brokerage WHERE buy_guid = :bg LIMIT 1");
    sqlQuery.bindValue(":bg", buyGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return BrokerageObject{};
    }
    return fromQuery(sqlQuery);
}

BrokerageObject BrokerageRepository::findBySaleGuid(const QString& saleGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM brokerage WHERE sale_guid = :sg LIMIT 1");
    sqlQuery.bindValue(":sg", saleGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return BrokerageObject{};
    }
    return fromQuery(sqlQuery);
}

QList<BrokerageObject> BrokerageRepository::findByShareAndYear(const QString& shareGuid,
                                                                int year) const
{
    QList<BrokerageObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT * FROM brokerage
        WHERE share_guid = :sg
          AND strftime('%Y', datetime) = :year
        ORDER BY datetime ASC
    )");
    sqlQuery.bindValue(":sg",   shareGuid);
    sqlQuery.bindValue(":year", QString::number(year));

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BrokerageRepository] findByShareAndYear failed:" << m_lastError.text();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

// ── Update ────────────────────────────────────────────────────────────────────

bool BrokerageRepository::update(const BrokerageObject& brokerage)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        UPDATE brokerage SET
            buy_guid   = :buy_guid,
            sale_guid  = :sale_guid,
            datetime   = :datetime,
            provision  = :provision,
            broker_fee = :broker_fee,
            trader_fee = :trader_fee,
            reduction  = :reduction,
            document   = :document
        WHERE guid = :guid
    )");

    sqlQuery.bindValue(":guid",       brokerage.guid());
    sqlQuery.bindValue(":buy_guid",   brokerage.buyGuid().isEmpty()  ? QVariant() : brokerage.buyGuid());
    sqlQuery.bindValue(":sale_guid",  brokerage.saleGuid().isEmpty() ? QVariant() : brokerage.saleGuid());
    sqlQuery.bindValue(":datetime",   brokerage.dateTime());
    sqlQuery.bindValue(":provision",  brokerage.provision());
    sqlQuery.bindValue(":broker_fee", brokerage.brokerFee());
    sqlQuery.bindValue(":trader_fee", brokerage.traderFee());
    sqlQuery.bindValue(":reduction",  brokerage.reduction());
    sqlQuery.bindValue(":document",   brokerage.document());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BrokerageRepository] update failed:" << m_lastError.text();
        return false;
    }
    return true;
}

bool BrokerageRepository::updateDocument(const QString& guid, const QString& document)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("UPDATE brokerage SET document = :doc WHERE guid = :guid");
    sqlQuery.bindValue(":doc",  document);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BrokerageRepository] updateDocument failed:" << m_lastError.text();
        return false;
    }
    return true;
}

// ── Delete ────────────────────────────────────────────────────────────────────

bool BrokerageRepository::remove(const QString& guid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM brokerage WHERE guid = :guid");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BrokerageRepository] remove failed:" << m_lastError.text();
        return false;
    }
    return true;
}

int BrokerageRepository::removeByShare(const QString& shareGuid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM brokerage WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BrokerageRepository] removeByShare failed:" << m_lastError.text();
        return -1;
    }
    return sqlQuery.numRowsAffected();
}

// ── Aggregates ────────────────────────────────────────────────────────────────

double BrokerageRepository::totalBrokerage(const QString& shareGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT COALESCE(SUM(provision + broker_fee + trader_fee), 0)
        FROM brokerage WHERE share_guid = :sg
    )");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}

double BrokerageRepository::totalBrokerageReduction(const QString& shareGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT COALESCE(SUM(provision + broker_fee + trader_fee - reduction), 0)
        FROM brokerage WHERE share_guid = :sg
    )");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}
