// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "BuyRepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

// ── Helpers ───────────────────────────────────────────────────────────────────

BuyObject BuyRepository::fromQuery(const QSqlQuery& sqlQuery) const
{
    return BuyObject(
        sqlQuery.value("guid").toString(),
        sqlQuery.value("share_guid").toString(),
        sqlQuery.value("depot_number").toString(),
        sqlQuery.value("order_number").toString(),
        sqlQuery.value("datetime").toString(),
        sqlQuery.value("volume").toDouble(),
        sqlQuery.value("volume_sold").toDouble(),
        sqlQuery.value("price").toDouble(),
        sqlQuery.value("brokerage_guid").toString(),
        sqlQuery.value("document").toString()
    );
}

// ── Create ────────────────────────────────────────────────────────────────────

bool BuyRepository::insert(const BuyObject& buy)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        INSERT INTO buys
            (guid, share_guid, depot_number, order_number, datetime,
             volume, volume_sold, price, brokerage_guid, document)
        VALUES
            (:guid, :share_guid, :depot_number, :order_number, :datetime,
             :volume, :volume_sold, :price, :brokerage_guid, :document)
    )");

    sqlQuery.bindValue(":guid",          buy.guid());
    sqlQuery.bindValue(":share_guid",    buy.shareGuid());
    sqlQuery.bindValue(":depot_number",  buy.depotNumber());
    sqlQuery.bindValue(":order_number",  buy.orderNumber());
    sqlQuery.bindValue(":datetime",      buy.dateTime());
    sqlQuery.bindValue(":volume",        buy.volume());
    sqlQuery.bindValue(":volume_sold",   buy.volumeSold());
    sqlQuery.bindValue(":price",         buy.price());
    sqlQuery.bindValue(":brokerage_guid",buy.brokerageGuid());
    sqlQuery.bindValue(":document",      buy.document());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] insert failed:" << m_lastError.text();
        return false;
    }
    return true;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QList<BuyObject> BuyRepository::findByShare(const QString& shareGuid) const
{
    QList<BuyObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM buys WHERE share_guid = :sg ORDER BY datetime ASC");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] findByShare failed:" << m_lastError.text();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

BuyObject BuyRepository::findByGuid(const QString& guid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM buys WHERE guid = :guid LIMIT 1");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return BuyObject{};
    }
    return fromQuery(sqlQuery);
}

QList<BuyObject> BuyRepository::findByShareAndYear(const QString& shareGuid, int year) const
{
    QList<BuyObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT * FROM buys
        WHERE share_guid = :sg
          AND strftime('%Y', datetime) = :year
        ORDER BY datetime ASC
    )");
    sqlQuery.bindValue(":sg",   shareGuid);
    sqlQuery.bindValue(":year", QString::number(year));

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] findByShareAndYear failed:" << m_lastError.text();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

bool BuyRepository::orderNumberExists(const QString& shareGuid,
                                       const QString& orderNumber) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT COUNT(*) FROM buys WHERE share_guid = :sg AND order_number = :on");
    sqlQuery.bindValue(":sg", shareGuid);
    sqlQuery.bindValue(":on", orderNumber);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return sqlQuery.value(0).toInt() > 0;
}

// ── Update ────────────────────────────────────────────────────────────────────

bool BuyRepository::update(const BuyObject& buy)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        UPDATE buys SET
            depot_number   = :depot_number,
            order_number   = :order_number,
            datetime       = :datetime,
            volume         = :volume,
            volume_sold    = :volume_sold,
            price          = :price,
            brokerage_guid = :brokerage_guid,
            document       = :document
        WHERE guid = :guid
    )");

    sqlQuery.bindValue(":guid",          buy.guid());
    sqlQuery.bindValue(":depot_number",  buy.depotNumber());
    sqlQuery.bindValue(":order_number",  buy.orderNumber());
    sqlQuery.bindValue(":datetime",      buy.dateTime());
    sqlQuery.bindValue(":volume",        buy.volume());
    sqlQuery.bindValue(":volume_sold",   buy.volumeSold());
    sqlQuery.bindValue(":price",         buy.price());
    sqlQuery.bindValue(":brokerage_guid",buy.brokerageGuid());
    sqlQuery.bindValue(":document",      buy.document());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] update failed:" << m_lastError.text();
        return false;
    }
    return true;
}

bool BuyRepository::updateVolumeSold(const QString& guid, double volumeSold)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("UPDATE buys SET volume_sold = :vs WHERE guid = :guid");
    sqlQuery.bindValue(":vs",   volumeSold);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] updateVolumeSold failed:" << m_lastError.text();
        return false;
    }
    return true;
}

bool BuyRepository::updateDocument(const QString& guid, const QString& document)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("UPDATE buys SET document = :doc WHERE guid = :guid");
    sqlQuery.bindValue(":doc",  document);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] updateDocument failed:" << m_lastError.text();
        return false;
    }
    return true;
}

bool BuyRepository::updateBrokerageGuid(const QString& guid, const QString& brokerageGuid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("UPDATE buys SET brokerage_guid = :bg WHERE guid = :guid");
    sqlQuery.bindValue(":bg",   brokerageGuid);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] updateBrokerageGuid failed:" << m_lastError.text();
        return false;
    }
    return true;
}

// ── Delete ────────────────────────────────────────────────────────────────────

bool BuyRepository::remove(const QString& guid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM buys WHERE guid = :guid");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] remove failed:" << m_lastError.text();
        return false;
    }
    return true;
}

int BuyRepository::removeByShare(const QString& shareGuid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM buys WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[BuyRepository] removeByShare failed:" << m_lastError.text();
        return -1;
    }
    return sqlQuery.numRowsAffected();
}

// ── Aggregates ────────────────────────────────────────────────────────────────

double BuyRepository::totalVolume(const QString& shareGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT COALESCE(SUM(volume), 0) FROM buys WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}

double BuyRepository::totalBuyValueBrokerageReduction(const QString& shareGuid) const
{
    // BuyValueBrokerageReduction = volume*price + brokerage - reduction
    // Brokerage data lives in the brokerage table — JOIN on brokerage_guid.
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT COALESCE(
            SUM(b.volume * b.price
                + COALESCE(br.provision,   0)
                + COALESCE(br.broker_fee,  0)
                + COALESCE(br.trader_fee,  0)
                - COALESCE(br.reduction,   0)),
            0)
        FROM buys b
        LEFT JOIN brokerage br ON br.guid = b.brokerage_guid
        WHERE b.share_guid = :sg
    )");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}
