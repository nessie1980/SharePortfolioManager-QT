// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DividendRepository.h"
#include "../core/Database.h"

#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

// ── Helpers ───────────────────────────────────────────────────────────────────

DividendObject DividendRepository::fromQuery(const QSqlQuery& sqlQuery) const
{
    return DividendObject(
        sqlQuery.value("guid").toString(),
        sqlQuery.value("share_guid").toString(),
        sqlQuery.value("datetime").toString(),
        sqlQuery.value("rate").toDouble(),
        sqlQuery.value("volume").toDouble(),
        sqlQuery.value("tax_at_source").toDouble(),
        sqlQuery.value("capital_gains_tax").toDouble(),
        sqlQuery.value("solidarity_tax").toDouble(),
        sqlQuery.value("price_at_payday").toDouble(),
        sqlQuery.value("enable_fc").toBool(),
        sqlQuery.value("exchange_ratio").toDouble(),
        sqlQuery.value("currency").toString(),
        sqlQuery.value("document").toString(),
        sqlQuery.value("ex_date").toString(),
        sqlQuery.value("depot_number").toString()
    );
}

// ── Create ────────────────────────────────────────────────────────────────────

bool DividendRepository::insert(const DividendObject& dividend)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        INSERT INTO dividends
            (guid, share_guid, datetime, rate, volume,
             tax_at_source, capital_gains_tax, solidarity_tax,
             price_at_payday, enable_fc, exchange_ratio, currency, document,
             ex_date, depot_number)
        VALUES
            (:guid, :share_guid, :datetime, :rate, :volume,
             :tax_at_source, :capital_gains_tax, :solidarity_tax,
             :price_at_payday, :enable_fc, :exchange_ratio, :currency, :document,
             :ex_date, :depot_number)
    )");

    sqlQuery.bindValue(":guid",             dividend.guid());
    sqlQuery.bindValue(":share_guid",       dividend.shareGuid());
    sqlQuery.bindValue(":datetime",         dividend.dateTime());
    sqlQuery.bindValue(":rate",             dividend.rate());
    sqlQuery.bindValue(":volume",           dividend.volume());
    sqlQuery.bindValue(":tax_at_source",    dividend.taxAtSource());
    sqlQuery.bindValue(":capital_gains_tax",dividend.capitalGainsTax());
    sqlQuery.bindValue(":solidarity_tax",   dividend.solidarityTax());
    sqlQuery.bindValue(":price_at_payday",  dividend.priceAtPayday());
    sqlQuery.bindValue(":enable_fc",        dividend.enableForeignCurrency());
    sqlQuery.bindValue(":exchange_ratio",   dividend.exchangeRatio());
    sqlQuery.bindValue(":currency",         dividend.currency());
    sqlQuery.bindValue(":document",         dividend.document());
    sqlQuery.bindValue(":ex_date",          dividend.exDate());
    sqlQuery.bindValue(":depot_number",     dividend.depotNumber());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[DividendRepository] insert failed:" << m_lastError.text();
        return false;
    }
    return true;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QList<DividendObject> DividendRepository::findByShare(const QString& shareGuid) const
{
    QList<DividendObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM dividends WHERE share_guid = :sg ORDER BY datetime ASC");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

DividendObject DividendRepository::findByGuid(const QString& guid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM dividends WHERE guid = :guid LIMIT 1");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return DividendObject{};
    }
    return fromQuery(sqlQuery);
}

QList<DividendObject> DividendRepository::findByShareAndYear(const QString& shareGuid,
                                                              int year) const
{
    QList<DividendObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT * FROM dividends
        WHERE share_guid = :sg
          AND strftime('%Y', datetime) = :year
        ORDER BY datetime ASC
    )");
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

// ── Update ────────────────────────────────────────────────────────────────────

bool DividendRepository::update(const DividendObject& dividend)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        UPDATE dividends SET
            datetime          = :datetime,
            rate              = :rate,
            volume            = :volume,
            tax_at_source     = :tax_at_source,
            capital_gains_tax = :capital_gains_tax,
            solidarity_tax    = :solidarity_tax,
            price_at_payday   = :price_at_payday,
            enable_fc         = :enable_fc,
            exchange_ratio    = :exchange_ratio,
            currency          = :currency,
            document          = :document,
            ex_date           = :ex_date,
            depot_number      = :depot_number
        WHERE guid = :guid
    )");

    sqlQuery.bindValue(":guid",             dividend.guid());
    sqlQuery.bindValue(":datetime",         dividend.dateTime());
    sqlQuery.bindValue(":rate",             dividend.rate());
    sqlQuery.bindValue(":volume",           dividend.volume());
    sqlQuery.bindValue(":tax_at_source",    dividend.taxAtSource());
    sqlQuery.bindValue(":capital_gains_tax",dividend.capitalGainsTax());
    sqlQuery.bindValue(":solidarity_tax",   dividend.solidarityTax());
    sqlQuery.bindValue(":price_at_payday",  dividend.priceAtPayday());
    sqlQuery.bindValue(":enable_fc",        dividend.enableForeignCurrency());
    sqlQuery.bindValue(":exchange_ratio",   dividend.exchangeRatio());
    sqlQuery.bindValue(":currency",         dividend.currency());
    sqlQuery.bindValue(":document",         dividend.document());
    sqlQuery.bindValue(":ex_date",          dividend.exDate());
    sqlQuery.bindValue(":depot_number",     dividend.depotNumber());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[DividendRepository] update failed:" << m_lastError.text();
        return false;
    }
    return true;
}

bool DividendRepository::updateDocument(const QString& guid, const QString& document)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("UPDATE dividends SET document = :doc WHERE guid = :guid");
    sqlQuery.bindValue(":doc",  document);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return true;
}

// ── Delete ────────────────────────────────────────────────────────────────────

bool DividendRepository::remove(const QString& guid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM dividends WHERE guid = :guid");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return true;
}

int DividendRepository::removeByShare(const QString& shareGuid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM dividends WHERE share_guid = :sg");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return -1;
    }
    return sqlQuery.numRowsAffected();
}

// ── Aggregates ────────────────────────────────────────────────────────────────

double DividendRepository::totalPayoutWithTaxes(const QString& shareGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT COALESCE(SUM(
            CASE WHEN enable_fc = 1 AND exchange_ratio != 0
                THEN ROUND(ROUND(rate * volume, 2) / exchange_ratio, 2)
                ELSE ROUND(rate * volume, 2)
            END
            - (tax_at_source + capital_gains_tax + solidarity_tax)
        ), 0)
        FROM dividends WHERE share_guid = :sg
    )");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}

double DividendRepository::totalPayout(const QString& shareGuid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        SELECT COALESCE(SUM(
            CASE WHEN enable_fc = 1 AND exchange_ratio != 0
                THEN ROUND(ROUND(rate * volume, 2) / exchange_ratio, 2)
                ELSE ROUND(rate * volume, 2)
            END
        ), 0)
        FROM dividends WHERE share_guid = :sg
    )");
    sqlQuery.bindValue(":sg", shareGuid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return 0.0;
    }
    return sqlQuery.value(0).toDouble();
}
