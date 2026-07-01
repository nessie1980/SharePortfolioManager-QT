// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QList>
#include <QXmlStreamReader>

/**
 * @brief Raw data structures mirroring the C# predecessor's XML export format.
 *
 * These structs are a 1:1 reflection of the XML attributes/elements found in
 * the legacy Portfolio.xml file. No conversion or validation happens here —
 * numeric and date fields stay as raw strings (German decimal comma / German
 * date format). Converting and persisting them is the job of
 * PortfolioImporter; this keeps the parser a pure, side-effect-free
 * XML -> struct mapper that is easy to unit test independently.
 */

/// <ForeignCurrency Flag="Checked" ExchangeRatio="1,11638" FCName="en-US" />
struct RawForeignCurrency
{
    bool    enabled = false;   ///< Flag == "Checked"
    QString exchangeRatio;     ///< German decimal string, e.g. "1,11638"
    QString currency;          ///< FCName, e.g. "en-US"
};

/// <Dividend Guid="..." Date="..." Rate="..." Volume="..." .../>
struct RawDividend
{
    QString guid;
    QString date;               ///< dd.MM.yyyy
    QString rate;
    QString volume;
    QString taxAtSource;
    QString capitalGainsTax;
    QString solidarityTax;
    QString price;               ///< price at payday
    QString doc;
    RawForeignCurrency fc;
    bool    hasForeignCurrency = false;
};

/// <UsedBuy BuyDate="..." BuyVolume="..." BuyPrice="..." Reduction="..." Brokerage="..." BuyGuid="..." />
struct RawUsedBuy
{
    QString buyDate;
    QString buyGuid;
    QString buyVolume;
    QString buyPrice;
    QString reduction;
    QString brokerage;
};

/// <Sale Guid="..." Date="..." DepotNumber="..." OrderNumber="..." ...>
///   <UsedBuys>...</UsedBuys>
/// </Sale>
struct RawSale
{
    QString guid;
    QString date;
    QString depotNumber;
    QString orderNumber;
    QString volume;
    QString salePrice;
    QString taxAtSource;
    QString capitalGainsTax;
    QString solidarityTax;
    QString reduction;     ///< legacy top-level field — the authoritative reduction
                           ///< lives in the linked Brokerage record (see Brokerage@Reduction)
    QString brokerageGuid;
    QString doc;
    QList<RawUsedBuy> usedBuys;
};

/// <Buy Guid="..." DepotNumber="..." OrderNumber="..." Date="..." .../>
struct RawBuy
{
    QString guid;
    QString depotNumber;
    QString orderNumber;
    QString date;
    QString volume;
    QString volumeSold;
    QString price;
    QString brokerageGuid;
    QString doc;
};

/// <Brokerage Guid="..." BuyPart="True/False" SalePart="True/False" GuidBuySale="..." .../>
struct RawBrokerage
{
    QString guid;
    bool    buyPart  = false;
    bool    salePart = false;
    QString guidBuySale;   ///< GUID of the linked Buy or Sale
    QString date;
    QString provision;
    QString brokerFee;
    QString traderFee;
    QString reduction;
    QString doc;
};

/// <Entry D="..." C="..." O="..." T="..." B="..." V="..." />
struct RawDailyValue
{
    QString date;     ///< D
    QString close;    ///< C
    QString open;     ///< O
    QString top;      ///< T
    QString bottom;   ///< B
    QString volume;   ///< V
};

/// <Share WKN="..." ISIN="..." Name="..." Update="...">...</Share>
struct RawShare
{
    QString wkn;
    QString isin;
    QString name;
    QString updateStr; ///< "None" | "MarketPrice" | "DailyValues" | "Both"

    QString detailsWebSite;
    QString stockMarketLaunchDate; ///< dd.MM.yyyy
    QString lastUpdateInternet;    ///< dd.MM.yyyy HH:mm
    QString lastUpdateShareDate;   ///< dd.MM.yyyy HH:mm
    QString sharePrice;
    QString sharePriceBefore;
    QString culture;       ///< no corresponding column in the current schema — logged as ignored
    QString shareTypeStr;  ///< "0" | "1" | "2"

    QString marketValueWebSite;
    QString marketValueParsing;   ///< "ApiYahoo" | "ApiOnvista" | "Regex" | (empty)

    QString dailyValuesWebSite;
    QString dailyValuesParsing;

    QList<RawDailyValue> dailyValues;
    QList<RawBrokerage>  brokerages;
    QList<RawBuy>        buys;
    QList<RawSale>       sales;
    QList<RawDividend>   dividends;
};

struct RawPortfolio
{
    QList<RawShare> shares;
};

/**
 * @brief Streaming reader for the legacy C# Portfolio.xml export format.
 *
 * Pure parsing only — no DB access, no business logic, no logging beyond
 * fatal XML structure errors (returned via the error out-parameter).
 */
class XmlPortfolioParser
{
public:
    /**
     * @brief Parse a Portfolio.xml file into a RawPortfolio structure.
     * @param filePath      Full path to the XML file.
     * @param outPortfolio  Receives the parsed data on success.
     * @param errorMessage  Receives a human-readable error on failure.
     * @return true on success, false on any XML or I/O error.
     */
    static bool parse(const QString& filePath,
                      RawPortfolio&  outPortfolio,
                      QString&       errorMessage);

private:
    static RawShare      parseShare(QXmlStreamReader& xml);
    static RawSale        parseSale(QXmlStreamReader& xml);
    static RawDividend    parseDividend(QXmlStreamReader& xml);
    static RawBuy          parseBuy(const QXmlStreamAttributes& attrs);
    static RawBrokerage    parseBrokerage(const QXmlStreamAttributes& attrs);
    static RawDailyValue   parseDailyValueEntry(const QXmlStreamAttributes& attrs);
    static RawUsedBuy      parseUsedBuy(const QXmlStreamAttributes& attrs);
};
