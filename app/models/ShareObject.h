// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QDateTime>
#include <QLocale>

/**
 * @brief Share type classification.
 */
enum class ShareType {
    Share, ///< Regular stock
    Fond,  ///< Investment fund
    Etf    ///< Exchange-traded fund
};

/**
 * @brief Internet update strategy for a share.
 */
enum class ShareUpdateType {
    None,        ///< No automatic update
    MarketPrice, ///< Update current price only
    DailyValues, ///< Update daily OHLCV history only
    Both         ///< Update both price and history
};

/**
 * @brief Parsing strategy for market data retrieval.
 */
enum class ShareParsingType {
    Regex,      ///< Regex-based web scraping
    ApiOnVista, ///< OnVista REST API (JSON)
    ApiYahoo    ///< Yahoo Finance REST API (JSON)
};

/**
 * @brief Represents the master data of a single share (stock, fund or ETF).
 *
 * Stores all general information about a share: identification, current price,
 * internet update configuration and display settings. Transaction data
 * (buys, sales, dividends) is managed separately via the repository layer.
 *
 * ### Fields overview
 * | Group | Fields |
 * |-------|--------|
 * | Identity | guid, wkn, isin, name, shareType |
 * | Pricing | curPrice, prevDayPrice, currency |
 * | Update config | updateType, parsingType, marketPriceUrl, dailyValuesUrl, websiteEncoding |
 * | Dates | addDateTime, lastInternetUpdate, lastPriceUpdate |
 * | Display | imagePath, detailsWebSiteUrl |
 *
 * ### Example
 * @code
 * ShareObject share(
 *     QUuid::createUuid().toString(QUuid::WithoutBraces),
 *     "BASF11", "DE000BASF111", "BASF SE",
 *     ShareType::Share, "EUR"
 * );
 * share.setCurPrice(42.50);
 * share.setPrevDayPrice(41.80);
 * @endcode
 */
class ShareObject
{
public:
    ShareObject() = default;

    /**
     * @brief Minimal constructor for creating a new share.
     * @param guid      Unique identifier (UUID string)
     * @param wkn       German securities identification number
     * @param isin      International Securities Identification Number
     * @param name      Display name of the share
     * @param type      Share type (Share, Fond, ETF)
     * @param currency  Currency code (e.g. "EUR")
     */
    ShareObject(const QString& guid,
                const QString& wkn,
                const QString& isin,
                const QString& name,
                ShareType      type     = ShareType::Share,
                const QString& currency = QStringLiteral("EUR"));

    /**
     * @brief Full constructor — all fields.
     */
    ShareObject(const QString& guid,
                const QString& wkn,
                const QString& isin,
                const QString& name,
                ShareType      shareType,
                const QString& currency,
                const QString& addDateTime,
                double         curPrice,
                double         prevDayPrice,
                const QString& lastInternetUpdate,
                const QString& lastPriceUpdate,
                ShareUpdateType  updateType,
                ShareParsingType marketPriceParsingType,
                const QString& marketPriceUrl,
                const QString& marketPriceEncoding,
                ShareParsingType dailyValuesParsingType,
                const QString& dailyValuesUrl,
                const QString& dailyValuesEncoding,
                const QString& detailsWebSiteUrl,
                const QString& imagePath);

    // ── Identity ──────────────────────────────────────────────────────────
    QString guid()      const { return m_guid; }      ///< Unique identifier (UUID)
    QString wkn()       const { return m_wkn; }       ///< WKN (German securities ID)
    QString isin()      const { return m_isin; }      ///< ISIN
    QString name()      const { return m_name; }      ///< Display name
    ShareType shareType() const { return m_shareType; } ///< Share type

    void setWkn(const QString& value)  { m_wkn  = value; }
    void setIsin(const QString& value) { m_isin = value; }
    void setName(const QString& value) { m_name = value; }
    void setShareType(ShareType value) { m_shareType = value; }

    // ── Pricing ───────────────────────────────────────────────────────────
    double  curPrice()     const { return m_curPrice; }     ///< Current market price
    double  prevDayPrice() const { return m_prevDayPrice; } ///< Previous day closing price
    QString currency()     const { return m_currency; }     ///< Currency code

    void setCurPrice(double value)        { m_curPrice     = value; }
    void setPrevDayPrice(double value)    { m_prevDayPrice = value; }
    void setCurrency(const QString& value){ m_currency     = value; }

    /// Price change from previous day (curPrice - prevDayPrice).
    double priceDifference() const { return m_curPrice - m_prevDayPrice; }

    /// Percentage change from previous day (priceDifference / prevDayPrice * 100).
    double pricePerformance() const {
        return (m_prevDayPrice > 0) ? (priceDifference() / m_prevDayPrice * 100.0) : 0.0;
    }

    // ── Dates ─────────────────────────────────────────────────────────────
    QString addDateTime()         const { return m_addDateTime; }         ///< When share was added
    QString lastInternetUpdate()  const { return m_lastInternetUpdate; }  ///< Last web update
    QString lastPriceUpdate()     const { return m_lastPriceUpdate; }     ///< Last price update

    void setAddDateTime(const QString& value)        { m_addDateTime        = value; }
    void setLastInternetUpdate(const QString& value) { m_lastInternetUpdate = value; }
    void setLastPriceUpdate(const QString& value)    { m_lastPriceUpdate    = value; }

    // ── Internet update configuration ─────────────────────────────────────
    ShareUpdateType  updateType()              const { return m_updateType; }
    ShareParsingType marketPriceParsingType()  const { return m_marketPriceParsingType; }
    QString          marketPriceUrl()          const { return m_marketPriceUrl; }
    QString          marketPriceEncoding()     const { return m_marketPriceEncoding; }
    ShareParsingType dailyValuesParsingType()  const { return m_dailyValuesParsingType; }
    QString          dailyValuesUrl()          const { return m_dailyValuesUrl; }
    QString          dailyValuesEncoding()     const { return m_dailyValuesEncoding; }

    void setUpdateType(ShareUpdateType value)              { m_updateType              = value; }
    void setMarketPriceParsingType(ShareParsingType value) { m_marketPriceParsingType  = value; }
    void setMarketPriceUrl(const QString& value)           { m_marketPriceUrl          = value ; }
    void setMarketPriceEncoding(const QString& value)      { m_marketPriceEncoding     = value; }
    void setDailyValuesParsingType(ShareParsingType value) { m_dailyValuesParsingType  = value; }
    void setDailyValuesUrl(const QString& value)           { m_dailyValuesUrl          = value; }
    void setDailyValuesEncoding(const QString& value)      { m_dailyValuesEncoding     = value; }

    // ── Display ───────────────────────────────────────────────────────────
    QString detailsWebSiteUrl() const { return m_detailsWebSiteUrl; } ///< URL for share details
    QString imagePath()         const { return m_imagePath; }         ///< Path to share logo

    void setDetailsWebSiteUrl(const QString& value) { m_detailsWebSiteUrl = value; }
    void setImagePath(const QString& value)         { m_imagePath         = value; }

    // ── Validity ──────────────────────────────────────────────────────────
    /// Returns true if guid, wkn and name are set.
    bool isValid() const { return !m_guid.isEmpty() && !m_wkn.isEmpty() && !m_name.isEmpty(); }

private:
    // Identity
    QString   m_guid;
    QString   m_wkn;
    QString   m_isin;
    QString   m_name;
    ShareType m_shareType = ShareType::Share;

    // Pricing
    double  m_curPrice     = 0.0;
    double  m_prevDayPrice = 0.0;
    QString m_currency     = QStringLiteral("EUR");

    // Dates
    QString m_addDateTime;
    QString m_lastInternetUpdate;
    QString m_lastPriceUpdate;

    // Internet update config
    ShareUpdateType  m_updateType             = ShareUpdateType::Both;
    ShareParsingType m_marketPriceParsingType = ShareParsingType::Regex;
    QString          m_marketPriceUrl;
    QString          m_marketPriceEncoding    = QStringLiteral("UTF-8");
    ShareParsingType m_dailyValuesParsingType = ShareParsingType::Regex;
    QString          m_dailyValuesUrl;
    QString          m_dailyValuesEncoding    = QStringLiteral("UTF-8");

    // Display
    QString m_detailsWebSiteUrl;
    QString m_imagePath;
};
