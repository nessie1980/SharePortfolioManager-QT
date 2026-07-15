// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SaleRepository.h"
#include "../core/Database.h"

#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

// ── Helpers ───────────────────────────────────────────────────────────────────

SaleObject SaleRepository::fromQuery(const QSqlQuery& sqlQuery) const
{
    const QString saleGuid = sqlQuery.value("guid").toString();
    const auto    details  = loadBuyDetails(saleGuid);

    // Brokerage values are joined from the brokerage table in the SELECT queries
    return SaleObject(
        saleGuid,
        sqlQuery.value("share_guid").toString(),
        sqlQuery.value("depot_number").toString(),
        sqlQuery.value("order_number").toString(),
        sqlQuery.value("datetime").toString(),
        sqlQuery.value("volume").toDouble(),
        sqlQuery.value("sale_price").toDouble(),
        details,
        sqlQuery.value("tax_at_source").toDouble(),
        sqlQuery.value("capital_gains_tax").toDouble(),
        sqlQuery.value("solidarity_tax").toDouble(),
        sqlQuery.value("brokerage_guid").toString(),
        sqlQuery.value("provision").toDouble(),
        sqlQuery.value("broker_fee").toDouble(),
        sqlQuery.value("trader_fee").toDouble(),
        sqlQuery.value("reduction").toDouble(),
        sqlQuery.value("document").toString()
    );
}

QList<SaleBuyDetail> SaleRepository::loadBuyDetails(const QString& saleGuid) const
{
    QList<SaleBuyDetail> list;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM sale_buy_details WHERE sale_guid = :sg ORDER BY datetime ASC");
    sqlQuery.bindValue(":sg", saleGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return list;
    }
    while (sqlQuery.next()) {
        list.append(SaleBuyDetail(
            sqlQuery.value("buy_guid").toString(),
            sqlQuery.value("datetime").toString(),
            sqlQuery.value("volume").toDouble(),
            sqlQuery.value("buy_price").toDouble(),
            sqlQuery.value("reduction_part").toDouble(),
            sqlQuery.value("brokerage_part").toDouble()
        ));
    }
    return list;
}

bool SaleRepository::insertBuyDetails(const QString& saleGuid,
                                       const QList<SaleBuyDetail>& details) const
{
    for (const auto& detail : details) {
        QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
        sqlQuery.prepare(R"(
            INSERT INTO sale_buy_details
                (sale_guid, buy_guid, datetime, volume, buy_price,
                 reduction_part, brokerage_part)
            VALUES
                (:sale_guid, :buy_guid, :datetime, :volume, :buy_price,
                 :reduction_part, :brokerage_part)
        )");
        sqlQuery.bindValue(":sale_guid",      saleGuid);
        sqlQuery.bindValue(":buy_guid",       detail.buyGuid());
        sqlQuery.bindValue(":datetime",       detail.dateTime());
        sqlQuery.bindValue(":volume",         detail.volume());
        sqlQuery.bindValue(":buy_price",      detail.buyPrice());
        sqlQuery.bindValue(":reduction_part", detail.reductionPart());
        sqlQuery.bindValue(":brokerage_part", detail.brokeragePart());

        if (!sqlQuery.exec()) {
            m_lastError = sqlQuery.lastError();
            qWarning() << "[SaleRepository] insertBuyDetails failed:" << m_lastError.text();
            return false;
        }
    }
    return true;
}

bool SaleRepository::deleteBuyDetails(const QString& saleGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM sale_buy_details WHERE sale_guid = :sg");
    sqlQuery.bindValue(":sg", saleGuid);
    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return true;
}

// ── SELECT helper (JOIN on brokerage) ─────────────────────────────────────────

static const char* kSelectWithBrokerage = R"(
    SELECT s.*,
           COALESCE(br.provision,   0) AS provision,
           COALESCE(br.broker_fee,  0) AS broker_fee,
           COALESCE(br.trader_fee,  0) AS trader_fee,
           COALESCE(br.reduction,   0) AS reduction
    FROM sales s
    LEFT JOIN brokerage br ON br.guid = s.brokerage_guid
)";

// ── Create ────────────────────────────────────────────────────────────────────

bool SaleRepository::insert(const SaleObject& sale)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        INSERT INTO sales
            (guid, share_guid, depot_number, order_number, datetime,
             volume, sale_price,
             tax_at_source, capital_gains_tax, solidarity_tax,
             brokerage_guid, document)
        VALUES
            (:guid, :share_guid, :depot_number, :order_number, :datetime,
             :volume, :sale_price,
             :tax_at_source, :capital_gains_tax, :solidarity_tax,
             :brokerage_guid, :document)
    )");

    sqlQuery.bindValue(":guid",             sale.guid());
    sqlQuery.bindValue(":share_guid",       sale.shareGuid());
    sqlQuery.bindValue(":depot_number",     sale.depotNumber());
    sqlQuery.bindValue(":order_number",     sale.orderNumber());
    sqlQuery.bindValue(":datetime",         sale.dateTime());
    sqlQuery.bindValue(":volume",           sale.volume());
    sqlQuery.bindValue(":sale_price",       sale.salePrice());
    sqlQuery.bindValue(":tax_at_source",    sale.taxAtSource());
    sqlQuery.bindValue(":capital_gains_tax",sale.capitalGainsTax());
    sqlQuery.bindValue(":solidarity_tax",   sale.solidarityTax());
    sqlQuery.bindValue(":brokerage_guid",   sale.brokerageGuid());
    sqlQuery.bindValue(":document",         sale.document());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[SaleRepository] insert failed:" << m_lastError.text();
        return false;
    }
    return insertBuyDetails(sale.guid(), sale.saleBuyDetails());
}

// ── Read ──────────────────────────────────────────────────────────────────────

QList<SaleObject> SaleRepository::findByShare(const QString& shareGuid) const
{
    QList<SaleObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    const QString sql = QString("%1 WHERE s.share_guid = :sg ORDER BY s.datetime ASC")
                            .arg(kSelectWithBrokerage);
    sqlQuery.prepare(sql);
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

SaleObject SaleRepository::findByGuid(const QString& guid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    const QString sql = QString("%1 WHERE s.guid = :guid LIMIT 1")
                            .arg(kSelectWithBrokerage);
    sqlQuery.prepare(sql);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return SaleObject{};
    }
    return fromQuery(sqlQuery);
}

QList<SaleObject> SaleRepository::findByShareAndYear(const QString& shareGuid, int year) const
{
    QList<SaleObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    const QString sql = QString(R"(%1
        WHERE s.share_guid = :sg
          AND strftime('%Y', s.datetime) = :year
        ORDER BY s.datetime ASC)").arg(kSelectWithBrokerage);
    sqlQuery.prepare(sql);
    sqlQuery.bindValue(":sg",   shareGuid);
    sqlQuery.bindValue(":year", QString::number(year));

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

bool SaleRepository::orderNumberExists(const QString& shareGuid,
                                        const QString& orderNumber) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT COUNT(*) FROM sales WHERE share_guid = :sg AND order_number = :on");
    sqlQuery.bindValue(":sg", shareGuid);
    sqlQuery.bindValue(":on", orderNumber);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return sqlQuery.value(0).toInt() > 0;
}

// ── Update ────────────────────────────────────────────────────────────────────

bool SaleRepository::update(const SaleObject& sale)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        UPDATE sales SET
            depot_number      = :depot_number,
            order_number      = :order_number,
            datetime          = :datetime,
            volume            = :volume,
            sale_price        = :sale_price,
            tax_at_source     = :tax_at_source,
            capital_gains_tax = :capital_gains_tax,
            solidarity_tax    = :solidarity_tax,
            brokerage_guid    = :brokerage_guid,
            document          = :document
        WHERE guid = :guid
    )");

    sqlQuery.bindValue(":guid",             sale.guid());
    sqlQuery.bindValue(":depot_number",     sale.depotNumber());
    sqlQuery.bindValue(":order_number",     sale.orderNumber());
    sqlQuery.bindValue(":datetime",         sale.dateTime());
    sqlQuery.bindValue(":volume",           sale.volume());
    sqlQuery.bindValue(":sale_price",       sale.salePrice());
    sqlQuery.bindValue(":tax_at_source",    sale.taxAtSource());
    sqlQuery.bindValue(":capital_gains_tax",sale.capitalGainsTax());
    sqlQuery.bindValue(":solidarity_tax",   sale.solidarityTax());
    sqlQuery.bindValue(":brokerage_guid",   sale.brokerageGuid());
    sqlQuery.bindValue(":document",         sale.document());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[SaleRepository] update failed:" << m_lastError.text();
        return false;
    }

    if (!deleteBuyDetails(sale.guid()))
        return false;
    return insertBuyDetails(sale.guid(), sale.saleBuyDetails());
}

bool SaleRepository::updateDocument(const QString& guid, const QString& document)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("UPDATE sales SET document = :doc WHERE guid = :guid");
    sqlQuery.bindValue(":doc",  document);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return true;
}

bool SaleRepository::updateBrokerageGuid(const QString& guid, const QString& brokerageGuid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("UPDATE sales SET brokerage_guid = :bg WHERE guid = :guid");
    sqlQuery.bindValue(":bg",   brokerageGuid);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return true;
}

// ── Delete ────────────────────────────────────────────────────────────────────

bool SaleRepository::remove(const QString& guid)
{
    if (!deleteBuyDetails(guid))
        return false;

    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM sales WHERE guid = :guid");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return true;
}

int SaleRepository::removeByShare(const QString& shareGuid)
{
    QSqlQuery detailsQuery(QSqlDatabase::database("spm_main"));
    detailsQuery.prepare(R"(
        DELETE FROM sale_buy_details
        WHERE sale_guid IN (SELECT guid FROM sales WHERE share_guid = :sg)
    )");
    detailsQuery.bindValue(":sg", shareGuid);
    if (!detailsQuery.exec()) {
        m_lastError = detailsQuery.lastError();
        return -1;
    }

    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM sales WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return -1;
    }
    return sqlQuery.numRowsAffected();
}

// ── Aggregates ────────────────────────────────────────────────────────────────

double SaleRepository::totalVolume(const QString& shareGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT COALESCE(SUM(volume), 0) FROM sales WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}

double SaleRepository::totalPayoutBrokerageReduction(const QString& shareGuid) const
{
    // payout = volume*salePrice - brokerage + reduction - taxSum
    // Brokerage lives in brokerage table — JOIN on brokerage_guid
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT COALESCE(SUM(
            s.volume * s.sale_price
            - (COALESCE(br.provision,  0) + COALESCE(br.broker_fee, 0) + COALESCE(br.trader_fee, 0))
            + COALESCE(br.reduction,   0)
            - (s.tax_at_source + s.capital_gains_tax + s.solidarity_tax)
        ), 0)
        FROM sales s
        LEFT JOIN brokerage br ON br.guid = s.brokerage_guid
        WHERE s.share_guid = :sg
    )");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}

double SaleRepository::totalProfitLossBrokerageReduction(const QString& shareGuid) const
{
    // profitLoss = (saleValue - brokerage + reduction) - buyValueBrokerageReduction - taxSum
    // Computed inline via JOIN — no stored column needed.
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT COALESCE(SUM(
            (s.volume * s.sale_price
             - (COALESCE(br.provision,  0) + COALESCE(br.broker_fee, 0) + COALESCE(br.trader_fee, 0))
             + COALESCE(br.reduction,   0))
            - (
                SELECT COALESCE(SUM(
                    sbd.volume * sbd.buy_price
                    + sbd.brokerage_part - sbd.reduction_part
                ), 0)
                FROM sale_buy_details sbd WHERE sbd.sale_guid = s.guid
              )
            - (s.tax_at_source + s.capital_gains_tax + s.solidarity_tax)
        ), 0)
        FROM sales s
        LEFT JOIN brokerage br ON br.guid = s.brokerage_guid
        WHERE s.share_guid = :sg
    )");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}
