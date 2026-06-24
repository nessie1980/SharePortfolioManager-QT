// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ShareRepository.h"
#include "../core/Database.h"

#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

// ── Helpers ───────────────────────────────────────────────────────────────────

ShareObject ShareRepository::fromQuery(const QSqlQuery& sqlQuery) const
{
    return ShareObject(
        sqlQuery.value("guid").toString(),
        sqlQuery.value("wkn").toString(),
        sqlQuery.value("isin").toString(),
        sqlQuery.value("name").toString(),
        static_cast<ShareType>(sqlQuery.value("share_type").toInt()),
        sqlQuery.value("currency").toString(),
        sqlQuery.value("add_datetime").toString(),
        sqlQuery.value("cur_price").toDouble(),
        sqlQuery.value("prev_day_price").toDouble(),
        sqlQuery.value("last_internet_update").toString(),
        sqlQuery.value("last_price_update").toString(),
        static_cast<ShareUpdateType>(sqlQuery.value("update_type").toInt()),
        static_cast<ShareParsingType>(sqlQuery.value("market_price_parsing_type").toInt()),
        sqlQuery.value("market_price_url").toString(),
        sqlQuery.value("market_price_encoding").toString(),
        static_cast<ShareParsingType>(sqlQuery.value("daily_values_parsing_type").toInt()),
        sqlQuery.value("daily_values_url").toString(),
        sqlQuery.value("daily_values_encoding").toString(),
        sqlQuery.value("details_website_url").toString(),
        sqlQuery.value("image_path").toString()
    );
}

// ── Create ────────────────────────────────────────────────────────────────────

bool ShareRepository::insert(const ShareObject& share)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        INSERT INTO shares
            (guid, wkn, isin, name, share_type, currency,
             add_datetime, cur_price, prev_day_price,
             last_internet_update, last_price_update,
             update_type,
             market_price_parsing_type, market_price_url, market_price_encoding,
             daily_values_parsing_type, daily_values_url, daily_values_encoding,
             details_website_url, image_path)
        VALUES
            (:guid, :wkn, :isin, :name, :share_type, :currency,
             :add_datetime, :cur_price, :prev_day_price,
             :last_internet_update, :last_price_update,
             :update_type,
             :market_price_parsing_type, :market_price_url, :market_price_encoding,
             :daily_values_parsing_type, :daily_values_url, :daily_values_encoding,
             :details_website_url, :image_path)
    )");

    sqlQuery.bindValue(":guid",                      share.guid());
    sqlQuery.bindValue(":wkn",                       share.wkn());
    sqlQuery.bindValue(":isin",                      share.isin());
    sqlQuery.bindValue(":name",                      share.name());
    sqlQuery.bindValue(":share_type",                static_cast<int>(share.shareType()));
    sqlQuery.bindValue(":currency",                  share.currency());
    sqlQuery.bindValue(":add_datetime",              share.addDateTime());
    sqlQuery.bindValue(":cur_price",                 share.curPrice());
    sqlQuery.bindValue(":prev_day_price",            share.prevDayPrice());
    sqlQuery.bindValue(":last_internet_update",      share.lastInternetUpdate());
    sqlQuery.bindValue(":last_price_update",         share.lastPriceUpdate());
    sqlQuery.bindValue(":update_type",               static_cast<int>(share.updateType()));
    sqlQuery.bindValue(":market_price_parsing_type", static_cast<int>(share.marketPriceParsingType()));
    sqlQuery.bindValue(":market_price_url",          share.marketPriceUrl());
    sqlQuery.bindValue(":market_price_encoding",     share.marketPriceEncoding());
    sqlQuery.bindValue(":daily_values_parsing_type", static_cast<int>(share.dailyValuesParsingType()));
    sqlQuery.bindValue(":daily_values_url",          share.dailyValuesUrl());
    sqlQuery.bindValue(":daily_values_encoding",     share.dailyValuesEncoding());
    sqlQuery.bindValue(":details_website_url",       share.detailsWebSiteUrl());
    sqlQuery.bindValue(":image_path",                share.imagePath());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareRepository] insert failed:" << m_lastError.text();
        return false;
    }
    return true;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QList<ShareObject> ShareRepository::findAll() const
{
    QList<ShareObject> result;
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM shares ORDER BY name ASC");

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return result;
    }
    while (sqlQuery.next())
        result.append(fromQuery(sqlQuery));
    return result;
}

ShareObject ShareRepository::findByGuid(const QString& guid) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM shares WHERE guid = :guid LIMIT 1");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return ShareObject{};
    }
    return fromQuery(sqlQuery);
}

ShareObject ShareRepository::findByWkn(const QString& wkn) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM shares WHERE wkn = :wkn LIMIT 1");
    sqlQuery.bindValue(":wkn", wkn);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return ShareObject{};
    }
    return fromQuery(sqlQuery);
}

ShareObject ShareRepository::findByIsin(const QString& isin) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT * FROM shares WHERE isin = :isin LIMIT 1");
    sqlQuery.bindValue(":isin", isin);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return ShareObject{};
    }
    return fromQuery(sqlQuery);
}

bool ShareRepository::wknExists(const QString& wkn) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT COUNT(*) FROM shares WHERE wkn = :wkn");
    sqlQuery.bindValue(":wkn", wkn);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return sqlQuery.value(0).toInt() > 0;
}

bool ShareRepository::isinExists(const QString& isin) const
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("SELECT COUNT(*) FROM shares WHERE isin = :isin");
    sqlQuery.bindValue(":isin", isin);

    if (!sqlQuery.exec() || !sqlQuery.next()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return sqlQuery.value(0).toInt() > 0;
}

// ── Update ────────────────────────────────────────────────────────────────────

bool ShareRepository::update(const ShareObject& share)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        UPDATE shares SET
            wkn                       = :wkn,
            isin                      = :isin,
            name                      = :name,
            share_type                = :share_type,
            currency                  = :currency,
            cur_price                 = :cur_price,
            prev_day_price            = :prev_day_price,
            last_internet_update      = :last_internet_update,
            last_price_update         = :last_price_update,
            update_type               = :update_type,
            market_price_parsing_type = :market_price_parsing_type,
            market_price_url          = :market_price_url,
            market_price_encoding     = :market_price_encoding,
            daily_values_parsing_type = :daily_values_parsing_type,
            daily_values_url          = :daily_values_url,
            daily_values_encoding     = :daily_values_encoding,
            details_website_url       = :details_website_url,
            image_path                = :image_path
        WHERE guid = :guid
    )");

    sqlQuery.bindValue(":guid",                      share.guid());
    sqlQuery.bindValue(":wkn",                       share.wkn());
    sqlQuery.bindValue(":isin",                      share.isin());
    sqlQuery.bindValue(":name",                      share.name());
    sqlQuery.bindValue(":share_type",                static_cast<int>(share.shareType()));
    sqlQuery.bindValue(":currency",                  share.currency());
    sqlQuery.bindValue(":cur_price",                 share.curPrice());
    sqlQuery.bindValue(":prev_day_price",            share.prevDayPrice());
    sqlQuery.bindValue(":last_internet_update",      share.lastInternetUpdate());
    sqlQuery.bindValue(":last_price_update",         share.lastPriceUpdate());
    sqlQuery.bindValue(":update_type",               static_cast<int>(share.updateType()));
    sqlQuery.bindValue(":market_price_parsing_type", static_cast<int>(share.marketPriceParsingType()));
    sqlQuery.bindValue(":market_price_url",          share.marketPriceUrl());
    sqlQuery.bindValue(":market_price_encoding",     share.marketPriceEncoding());
    sqlQuery.bindValue(":daily_values_parsing_type", static_cast<int>(share.dailyValuesParsingType()));
    sqlQuery.bindValue(":daily_values_url",          share.dailyValuesUrl());
    sqlQuery.bindValue(":daily_values_encoding",     share.dailyValuesEncoding());
    sqlQuery.bindValue(":details_website_url",       share.detailsWebSiteUrl());
    sqlQuery.bindValue(":image_path",                share.imagePath());

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareRepository] update failed:" << m_lastError.text();
        return false;
    }
    return true;
}

bool ShareRepository::updatePrice(const QString& guid, double curPrice,
                                   double prevDayPrice, const QString& lastPriceUpdate)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare(R"(
        UPDATE shares SET
            cur_price         = :cur_price,
            prev_day_price    = :prev_day_price,
            last_price_update = :last_price_update
        WHERE guid = :guid
    )");
    sqlQuery.bindValue(":guid",             guid);
    sqlQuery.bindValue(":cur_price",        curPrice);
    sqlQuery.bindValue(":prev_day_price",   prevDayPrice);
    sqlQuery.bindValue(":last_price_update",lastPriceUpdate);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return true;
}

bool ShareRepository::updateLastInternetUpdate(const QString& guid, const QString& lastUpdate)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("UPDATE shares SET last_internet_update = :lu WHERE guid = :guid");
    sqlQuery.bindValue(":lu",   lastUpdate);
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        return false;
    }
    return true;
}

// ── Delete ────────────────────────────────────────────────────────────────────

bool ShareRepository::remove(const QString& guid)
{
    QSqlQuery sqlQuery(QSqlDatabase::database("spm_main"));
    sqlQuery.prepare("DELETE FROM shares WHERE guid = :guid");
    sqlQuery.bindValue(":guid", guid);

    if (!sqlQuery.exec()) {
        m_lastError = sqlQuery.lastError();
        qWarning() << "[ShareRepository] remove failed:" << m_lastError.text();
        return false;
    }
    return true;
}
