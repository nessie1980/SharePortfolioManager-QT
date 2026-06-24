// MIT License
// Copyright (c) 2021 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

/**
 * @namespace ParserLib::JsonObjects::Yahoo
 * @brief Data structures for the Yahoo Finance REST API.
 *
 * Yahoo Finance provides two endpoints relevant to this application:
 * - **Real-time** (`/v7/finance/quote`): current price and metadata
 * - **History** (`/v8/finance/chart`): daily OHLCV data for a date range
 *
 * Authentication is via an API key passed as `X-API-KEY` request header.
 * Both structs provide a static `fromJson()` factory method.
 */
namespace ParserLib::JsonObjects::Yahoo {

/**
 * @brief A single real-time quote result from the Yahoo Finance API.
 *
 * Result keys placed in ParserInfoState::searchResult after parsing:
 * | Key | Content |
 * |-----|---------|
 * | `"Currency"` | ISO currency code (e.g. "USD") |
 * | `"Price"` | Current market price, 4 decimal places |
 * | `"PriceBefore"` | Previous close, 4 decimal places |
 * | `"LastDate"` | Date of last quote (locale short format) |
 * | `"LastTime"` | Time of last quote (locale short format) |
 */
struct QuoteResult
{
    QString currency;                           ///< ISO currency code
    QString exchange;                           ///< Exchange identifier
    QString shortName;                          ///< Short instrument name
    double  regularMarketPrice          = 0.0; ///< Current market price
    double  regularMarketPreviousClose  = 0.0; ///< Previous closing price
    qint64  regularMarketTime           = 0;   ///< Unix timestamp of last quote

    /**
     * @brief Deserialize from a single JSON quote object.
     * @param obj  JSON object from the `quoteResponse.result` array
     */
    static QuoteResult fromJson(const QJsonObject& obj) {
        QuoteResult result;
        result.currency                  = obj["currency"].toString();
        result.exchange                  = obj["exchange"].toString();
        result.shortName                 = obj["shortName"].toString();
        result.regularMarketPrice        = obj["regularMarketPrice"].toDouble();
        result.regularMarketPreviousClose= obj["regularMarketPreviousClose"].toDouble();
        result.regularMarketTime         = static_cast<qint64>(obj["regularMarketTime"].toDouble());
        return result;
    }
};

/**
 * @brief Yahoo Finance real-time data — wraps the full `quoteResponse` JSON.
 */
struct RealTimeData
{
    QList<QuoteResult> results; ///< One entry per requested symbol

    /// Returns true if at least one quote result was parsed successfully.
    bool isValid() const { return !results.isEmpty(); }

    /**
     * @brief Deserialize from raw JSON response bytes.
     * @param data  Raw JSON bytes from the Yahoo Finance quote endpoint
     * @return Populated RealTimeData; isValid() returns false on parse error
     */
    static RealTimeData fromJson(const QByteArray& data) {
        RealTimeData realTimeData;
        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) return realTimeData;
        const auto root = doc.object();
        const auto resultArr = root.value(QStringLiteral("quoteResponse")).toObject()
                                   .value(QStringLiteral("result")).toArray();
        for (const auto& item : resultArr)
            realTimeData.results.append(QuoteResult::fromJson(item.toObject()));
        return realTimeData;
    }
};

/**
 * @brief OHLCV indicator data for one result in a history response.
 *
 * All lists are parallel arrays indexed by trading day,
 * corresponding to the `timestamps` list in HistoryResult.
 */
struct QuoteIndicator
{
    QList<double> open;   ///< Opening prices
    QList<double> close;  ///< Closing prices
    QList<double> high;   ///< Day highs
    QList<double> low;    ///< Day lows
    QList<qint64> volume; ///< Trading volumes
};

/**
 * @brief One history result entry (one symbol) from the Yahoo Finance chart API.
 */
struct HistoryResult
{
    QList<qint64>        timestamps; ///< Unix timestamps (UTC) for each trading day
    QList<QuoteIndicator> quotes;   ///< OHLCV data (one QuoteIndicator per symbol)
};

/**
 * @brief Yahoo Finance daily OHLCV history — wraps the full `chart` JSON response.
 *
 * After parsing, results are available in ParserInfoState::dailyValuesList
 * as a list of DailyValues structs.
 */
struct HistoryData
{
    QList<HistoryResult> results; ///< One entry per requested symbol

    /// Returns true if at least one result with timestamps and quotes was parsed.
    bool isValid() const {
        return !results.isEmpty() &&
               !results.first().timestamps.isEmpty() &&
               !results.first().quotes.isEmpty();
    }

    /**
     * @brief Deserialize from raw JSON response bytes.
     * @param data  Raw JSON bytes from the Yahoo Finance chart endpoint
     * @return Populated HistoryData; isValid() returns false on parse error
     */
    static HistoryData fromJson(const QByteArray& data) {
        HistoryData historyData;
        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) return historyData;
        const auto root = doc.object();
        const auto resultArray = root.value(QStringLiteral("chart")).toObject()
                                     .value(QStringLiteral("result")).toArray();

        for (const auto& result : resultArray) {
            HistoryResult historyResult;
            const auto resultObject = result.toObject();

            for (const auto& timeStamp : resultObject["timestamp"].toArray())
                historyResult.timestamps.append(static_cast<qint64>(timeStamp.toDouble()));

            const auto quoteArr = resultObject["indicators"].toObject()["quote"].toArray();
            for (const auto& quote : quoteArr) {
                QuoteIndicator quoteIndicator;
                const auto quoteObject = quote.toObject();

                auto toDoubleList = [](const QJsonArray& a) {
                    QList<double> listDouble;
                    for (const auto& v : a) listDouble.append(v.toDouble());
                    return listDouble;
                };
                auto toInt64List = [](const QJsonArray& array) {
                    QList<qint64> listInt64;
                    for (const auto& value : array) listInt64.append(static_cast<qint64>(value.toDouble()));
                    return listInt64;
                };

                quoteIndicator.open   = toDoubleList(quoteObject["open"].toArray());
                quoteIndicator.close  = toDoubleList(quoteObject["close"].toArray());
                quoteIndicator.high   = toDoubleList(quoteObject["high"].toArray());
                quoteIndicator.low    = toDoubleList(quoteObject["low"].toArray());
                quoteIndicator.volume = toInt64List(quoteObject["volume"].toArray());
                historyResult.quotes.append(quoteIndicator);
            }
            historyData.results.append(historyResult);
        }
        return historyData;
    }
};

} // namespace ParserLib::JsonObjects::Yahoo
