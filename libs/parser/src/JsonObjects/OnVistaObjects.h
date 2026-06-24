// MIT License
// Copyright (c) 2021 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

/**
 * @namespace ParserLib::JsonObjects::OnVista
 * @brief Data structures for the OnVista Finance REST API.
 *
 * OnVista provides two endpoints relevant to this application:
 * - **Real-time**: current price, currency, previous close
 * - **History**: daily OHLCV data for a given date range
 *
 * Both structs provide a static `fromJson()` factory method that
 * deserializes the raw API response directly from a `QByteArray`.
 */
namespace ParserLib::JsonObjects::OnVista {

/**
 * @brief Timestamp information attached to a real-time price quote.
 */
struct DateTimePrice
{
    QString localTime;      ///< Local time string (ISO 8601)
    QString localTimeZone;  ///< Time zone identifier (e.g. "Europe/Berlin")
    int     utcTimeStamp = 0; ///< Unix timestamp (UTC seconds since epoch)

    /**
     * @brief Deserialize from a JSON object.
     * @param obj  JSON object containing "LocalTime", "LocalTimeZone", "UtcTimeStamp"
     */
    static DateTimePrice fromJson(const QJsonObject& obj) {
        DateTimePrice d;
        d.localTime     = obj[QStringLiteral("localTime")].toString();
        d.localTimeZone = obj[QStringLiteral("localTimeZone")].toString();
        d.utcTimeStamp  = obj[QStringLiteral("utcTimeStamp")].toInt();
        return d;
    }
};

/**
 * @brief OnVista real-time price data.
 *
 * Maps to the OnVista `/api/v1/instruments/.../quotes` JSON response.
 *
 * Result keys placed in ParserInfoState::searchResult after parsing:
 * | Key | Content |
 * |-----|---------|
 * | `"Currency"` | ISO currency code (e.g. "EUR") |
 * | `"Price"` | Current price, 4 decimal places |
 * | `"PriceBefore"` | Previous close, 4 decimal places |
 * | `"LastDate"` | Date of last quote (locale short format) |
 * | `"LastTime"` | Time of last quote (locale short format) |
 */
struct RealTimeData
{
    float         price         = 0.0f; ///< Current market price
    DateTimePrice datetimePrice;        ///< Timestamp of the quote
    float         previousLast  = 0.0f; ///< Previous closing price
    int           idNotation    = 0;    ///< OnVista instrument notation ID
    int           idCurrency    = 0;    ///< OnVista currency ID
    QString       isoCurrency;          ///< ISO currency code (e.g. "EUR")

    /**
     * @brief Deserialize from raw JSON response bytes.
     * @param data  Raw JSON bytes from the OnVista API
     * @return Populated RealTimeData; fields are zero/empty on parse error
     */
    static RealTimeData fromJson(const QByteArray& data) {
        RealTimeData r;
        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) return r;
        const auto obj = doc.object();
        r.price         = static_cast<float>(obj[QStringLiteral("price")].toDouble());
        r.previousLast  = static_cast<float>(obj[QStringLiteral("previousLast")].toDouble());
        r.idNotation    = obj[QStringLiteral("idNotation")].toInt();
        r.idCurrency    = obj[QStringLiteral("idCurrency")].toInt();
        r.isoCurrency   = obj[QStringLiteral("isoCurrency")].toString();
        r.datetimePrice = DateTimePrice::fromJson(obj[QStringLiteral("datetimePrice")].toObject());
        return r;
    }
};

/**
 * @brief OnVista daily OHLCV history data.
 *
 * Maps to the OnVista `/api/v1/instruments/.../eodHistory` JSON response.
 * All price lists are parallel arrays of equal length indexed by trading day.
 *
 * After parsing, results are available in ParserInfoState::dailyValuesList
 * as a list of DailyValues structs.
 */
struct HistoryData
{
    QList<qint64> datetimeLast; ///< Unix timestamps (UTC) for each trading day
    QList<float>  first;        ///< Opening prices
    QList<float>  last;         ///< Closing prices
    QList<float>  high;         ///< Day highs
    QList<float>  low;          ///< Day lows
    QList<float>  volume;       ///< Trading volumes

    /// Returns true if no data was loaded.
    bool isEmpty() const { return first.isEmpty(); }

    /// Returns true if all parallel arrays have the same non-zero length.
    bool isValid() const {
        return !datetimeLast.isEmpty() &&
               datetimeLast.size() == first.size() &&
               datetimeLast.size() == last.size();
    }

    /**
     * @brief Deserialize from raw JSON response bytes.
     * @param data  Raw JSON bytes from the OnVista API
     * @return Populated HistoryData; isEmpty() returns true on parse error
     */
    static HistoryData fromJson(const QByteArray& data) {
        HistoryData historyData;
        const auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) return historyData;
        const auto object = doc.object();

        auto toFloatList = [](const QJsonArray& array) {
            QList<float> listFloat;
            for (const auto& value : array) listFloat.append(static_cast<float>(value.toDouble()));
            return listFloat;
        };
        auto toInt64List = [](const QJsonArray& arr) {
            QList<qint64> listInt64;
            for (const auto& value : arr) listInt64.append(static_cast<qint64>(value.toDouble()));
            return listInt64;
        };

        historyData.datetimeLast = toInt64List(object[QStringLiteral("datetimeLast")].toArray());
        historyData.first        = toFloatList(object[QStringLiteral("first")].toArray());
        historyData.last         = toFloatList(object[QStringLiteral("last")].toArray());
        historyData.high         = toFloatList(object[QStringLiteral("high")].toArray());
        historyData.low          = toFloatList(object[QStringLiteral("low")].toArray());
        historyData.volume       = toFloatList(object[QStringLiteral("volume")].toArray());
        return historyData;
    }
};

} // namespace ParserLib::JsonObjects::OnVista
