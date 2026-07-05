// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QStringList>
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

    /// Human-readable notes about auto-corrected data quality issues found
    /// while parsing this share (e.g. double-XML-escaped WebSite URLs, see
    /// XmlPortfolioParser::normalizeWebSiteUrl()). Populated by the parser,
    /// logged by PortfolioImporter — kept separate from the fatal
    /// XML-structure errors handled via XmlPortfolioParser::parse()'s
    /// errorMessage out-parameter.
    QStringList parseWarnings;

    /// Human-readable notes about data quality issues found while parsing
    /// this share that were deliberately NOT auto-corrected (e.g. an
    /// unexpected element name such as "<MarketValues>" instead of
    /// "<MarketValue>" — a structural error in the source, not a safely
    /// normalizable formatting detail). The affected field(s) are left empty
    /// rather than guessed at. Populated by the parser, logged by
    /// PortfolioImporter as ImportLogger::Action::Error so the source data
    /// gets corrected and re-imported instead of silently accepted.
    QStringList parseErrors;
};

struct RawPortfolio
{
    QList<RawShare> shares;
};

/**
 * @brief Streaming reader for the legacy C# Portfolio.xml export format.
 *
 * Pure parsing only — no DB access, no business logic, no logging beyond
 * fatal XML structure errors (returned via the error out-parameter). Data
 * quality issues that can be safely auto-corrected (see normalizeWebSiteUrl())
 * are collected as human-readable notes in RawShare::parseWarnings instead,
 * for the caller to log as it sees fit.
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

    /**
     * @brief Detects and repairs double-XML-escaped WebSite/URL values.
     *
     * The legacy C# source XML has been observed to contain doubly-escaped
     * ampersands in WebSite attributes/elements (`DetailsWebSite`,
     * `MarketValue@WebSite`, `DailyValues@WebSite`), e.g. `&amp;amp;` instead
     * of `&amp;`. A conformant XML parser (QXmlStreamReader included) only
     * unescapes entities once, so such a value survives parsing as a literal
     * `&amp;` substring in the resulting QString — a broken URL, but not an
     * empty one (reported 05.07.2026 for Nvidia/Wacker Chemie).
     *
     * If the given raw value contains a literal `&amp;` substring, this is a
     * strong signal of exactly this data quality issue (a legitimate URL
     * query string never contains that literal text), so it is auto-corrected
     * to `&` and a note is appended to @p warnings for the caller to log.
     *
     * Note: the same two shares (Nvidia/Wacker Chemie) also use the element
     * name `<MarketValues>` (plural) instead of `<MarketValue>` (singular) —
     * a separate, structural data error handled directly in parseShare() by
     * logging it to RawShare::parseErrors and deliberately NOT parsing that
     * element's data (not a case for this method, which only handles safely
     * auto-correctable formatting details).
     *
     * @param raw        The already-XML-unescaped attribute/element value.
     * @param fieldLabel Human-readable field identifier for the warning text,
     *                   e.g. "MarketValue.WebSite".
     * @param warnings   Warning list to append a note to, if a correction was made.
     * @return           The (possibly corrected) value.
     */
    static QString normalizeWebSiteUrl(const QString& raw,
                                       const QString& fieldLabel,
                                       QStringList&   warnings);
};
