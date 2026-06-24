// MIT License
// Copyright (c) 2021 nessie1980 (nessie1980@gmx.de)
#include "Parser.h"
#include "JsonObjects/OnVistaObjects.h"
#include "JsonObjects/YahooObjects.h"

#include <QNetworkRequest>
#include <QRegularExpression>
#include <QDateTime>
#include <QLocale>
#include <QDebug>

namespace ParserLib {

// ── Construction ──────────────────────────────────────────────────────────────
Parser::Parser(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{}

// ── Public API ────────────────────────────────────────────────────────────────
void Parser::setParsingValues(const ParsingValues& values)
{
    m_parsingValues = values;
}

bool Parser::startParsing()
{
    if (m_busy) {
        setState(ParserErrorCode::BusyFailed, 0);
        return false;
    }

    if (m_parsingValues.loadingType() == LoadType::Web && !m_parsingValues.isValid()) {
        setState(ParserErrorCode::InvalidWebSiteGiven, 0);
        return false;
    }

    if (m_parsingValues.parsingType() == ParsingType::Regex &&
        m_parsingValues.regexList().isEmpty()) {
        setState(ParserErrorCode::NoRegexListGiven, 0);
        return false;
    }

    m_busy      = true;
    m_cancelled = false;
    m_infoState = ParserInfoState{};
    m_infoState.webSite             = m_parsingValues.webSiteUrl().toString();
    m_infoState.userAgentIdentifier = m_userAgentIdentifier;

    setState(ParserErrorCode::Starting, 0);
    setState(ParserErrorCode::Started,  0);

    // ── Text mode: parse directly ─────────────────────────────────────────
    if (m_parsingValues.loadingType() == LoadType::Text) {
        doRegexParsing(m_parsingValues.parsingText());
        return true;
    }

    // ── Web mode: download first ──────────────────────────────────────────
    setState(ParserErrorCode::ContentLoadStarted, 5);

    QNetworkRequest request(m_parsingValues.webSiteUrl());
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgentIdentifier);

    if (!m_parsingValues.apiKey().isEmpty())
        request.setRawHeader("X-API-KEY", m_parsingValues.apiKey().toUtf8());

    m_reply = m_networkManager->get(request);

    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &Parser::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished,
            this, &Parser::onDownloadFinished);
    connect(m_reply, &QNetworkReply::errorOccurred,
            this, &Parser::onNetworkError);

    return true;
}

void Parser::cancelParsing()
{
    m_cancelled = true;
    if (m_reply && m_reply->isRunning())
        m_reply->abort();
    setState(ParserErrorCode::CancelOperation, 0);
    m_busy = false;
}

// ── Network slots ─────────────────────────────────────────────────────────────
void Parser::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0)
        m_infoState.percentageDownload =
            static_cast<int>((bytesReceived * 100) / bytesTotal);
    emit parserUpdated(m_infoState);
}

void Parser::onDownloadFinished()
{
    if (!m_reply) return;

    m_reply->deleteLater();

    if (m_cancelled) {
        m_busy = false;
        return;
    }

    if (m_reply->error() != QNetworkReply::NoError) {
        // handled by onNetworkError
        return;
    }

    const QByteArray rawData = m_reply->readAll();

    if (rawData.isEmpty()) {
        setState(ParserErrorCode::NoWebContentLoaded, 0);
        m_busy = false;
        return;
    }

    m_infoState.webSiteContentAsByteArray = rawData;
    m_infoState.webSiteContentAsString    =
        QString::fromUtf8(rawData); // encoding handling can be extended

    setState(ParserErrorCode::ContentLoadFinished, 10);

    // ── Dispatch to correct parser ────────────────────────────────────────
    switch (m_parsingValues.parsingType()) {
    case ParsingType::Regex:
        doRegexParsing(m_infoState.webSiteContentAsString);
        break;
    case ParsingType::OnVistaRealTime:
        doOnVistaRealTimeParsing(rawData);
        break;
    case ParsingType::OnVistaHistoryData:
        doOnVistaHistoryParsing(rawData);
        break;
    case ParsingType::YahooRealTime:
        doYahooRealTimeParsing(rawData);
        break;
    case ParsingType::YahooHistoryData:
        doYahooHistoryParsing(rawData);
        break;
    }
}

void Parser::onNetworkError(QNetworkReply::NetworkError /*error*/)
{
    if (!m_reply) return;
    const QString msg = m_reply->errorString();
    qWarning() << "[Parser] Network error:" << msg;
    setState(ParserErrorCode::NetworkError, 0, msg);
    m_busy = false;
}

// ── Regex parsing ─────────────────────────────────────────────────────────────
void Parser::doRegexParsing(const QString& text)
{
    setState(ParserErrorCode::SearchStarted, 15);

    const auto& rules       = m_parsingValues.regexList();
    const int   stepSize    = rules.isEmpty() ? 0 : (100 - 15) / rules.size();
    int         progressPercent = 15;

    for (auto it = rules.cbegin(); it != rules.cend(); ++it) {
        if (m_cancelled) {
            setState(ParserErrorCode::CancelOperation, 0);
            m_busy = false;
            return;
        }

        const QString&    key     = it.key();
        const RegExElement& element = it.value();

        m_infoState.lastRegexListKey = key;

        // Build QRegularExpression options
        QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
        for (auto opt : element.regexOptions)
            opts |= opt;

        QRegularExpression regularExpression(element.regexExpression, opts);
        if (!regularExpression.isValid()) {
            qWarning() << "[Parser] Invalid regex for key:" << key << regularExpression.errorString();
            if (!element.resultEmpty) {
                setState(ParserErrorCode::ParsingFailed, 0);
                m_busy = false;
                return;
            }
            continue;
        }

        QList<QString> results;

        if (element.regexFoundPosition >= 0) {
            // Specific match index
            auto matchIt = regularExpression.globalMatch(text);
            int  currentMatchIndex = 0;
            while (matchIt.hasNext()) {
                auto match = matchIt.next();
                if (currentMatchIndex == element.regexFoundPosition) {
                    // Collect first non-empty capture group
                    for (int captureIndex = 1; captureIndex <= match.lastCapturedIndex(); ++captureIndex) {
                        if (!match.captured(captureIndex).isEmpty()) {
                            results.append(match.captured(captureIndex));
                            break;
                        }
                    }
                    break;
                }
                ++currentMatchIndex;
            }
        } else {
            // All matches
            auto matchIt = regularExpression.globalMatch(text);
            while (matchIt.hasNext()) {
                auto match = matchIt.next();
                for (int captureIndex = 1; captureIndex <= match.lastCapturedIndex(); ++captureIndex) {
                    if (!match.captured(captureIndex).isEmpty())
                        results.append(match.captured(captureIndex));
                }
            }
        }

        if (results.isEmpty() && !element.resultEmpty) {
            setState(ParserErrorCode::ParsingFailed, 0);
            m_busy = false;
            return;
        }

        m_infoState.searchResult.insert(key, results);

        progressPercent += stepSize;
        if (progressPercent < 100)
            setState(ParserErrorCode::SearchRunning, progressPercent);
    }

    finish(ParserErrorCode::Finished);
}

// ── OnVista JSON ──────────────────────────────────────────────────────────────
void Parser::doOnVistaRealTimeParsing(const QByteArray& data)
{
    const auto realTimeData = JsonObjects::OnVista::RealTimeData::fromJson(data);

    m_infoState.searchResult["Currency"]    = { realTimeData.isoCurrency };
    m_infoState.searchResult["Price"]       = { QString::number(static_cast<double>(realTimeData.price), 'f', 4) };
    m_infoState.searchResult["PriceBefore"] = { QString::number(static_cast<double>(realTimeData.previousLast), 'f', 4) };

    const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(realTimeData.datetimePrice.utcTimeStamp);
    m_infoState.searchResult["LastDate"] = { QLocale().toString(dateTime.date(), QLocale::ShortFormat) };
    m_infoState.searchResult["LastTime"] = { QLocale().toString(dateTime.time(), QLocale::ShortFormat) };

    finish();
}

// ── OnVista History ───────────────────────────────────────────────────────────
void Parser::doOnVistaHistoryParsing(const QByteArray& data)
{
    const auto historyData = JsonObjects::OnVista::HistoryData::fromJson(data);

    if (historyData.isEmpty() || !historyData.isValid()) {
        setState(ParserErrorCode::NoWebContentLoaded, 0);
        m_busy = false;
        return;
    }

    m_infoState.dailyValuesList.clear();
    const double progressStep = (100.0 - 10.0) / historyData.datetimeLast.size();
    double       progressPercent = 10.0;

    for (int dayIndex = 0; dayIndex < historyData.datetimeLast.size(); ++dayIndex) {
        DailyValues dailyValues;
        dailyValues.date         = QDateTime::fromSecsSinceEpoch(historyData.datetimeLast[dayIndex]).date();
        dailyValues.openingPrice = qRound(static_cast<double>(historyData.first[dayIndex]) * 100.0) / 100.0;
        dailyValues.closingPrice = qRound(static_cast<double>(historyData.last[dayIndex])  * 100.0) / 100.0;
        dailyValues.top          = qRound(static_cast<double>(historyData.high[dayIndex])  * 100.0) / 100.0;
        dailyValues.bottom       = qRound(static_cast<double>(historyData.low[dayIndex])   * 100.0) / 100.0;
        dailyValues.volume       = qRound(static_cast<double>(historyData.volume[dayIndex])* 100.0) / 100.0;
        m_infoState.dailyValuesList.append(dailyValues);

        progressPercent += progressStep;
        if (progressPercent < 100)
            setState(ParserErrorCode::SearchRunning, static_cast<int>(progressPercent));
    }

    finish();
}

// ── Yahoo RealTime ────────────────────────────────────────────────────────────
void Parser::doYahooRealTimeParsing(const QByteArray& data)
{
    const auto realTimeData = JsonObjects::Yahoo::RealTimeData::fromJson(data);

    if (!realTimeData.isValid()) {
        setState(ParserErrorCode::ParsingFailed, 0);
        m_busy = false;
        return;
    }

    const auto& realTimeDataElement = realTimeData.results.first();
    m_infoState.searchResult["Currency"]    = { realTimeDataElement.currency };
    m_infoState.searchResult["Price"]       = { QString::number(realTimeDataElement.regularMarketPrice, 'f', 4) };
    m_infoState.searchResult["PriceBefore"] = { QString::number(realTimeDataElement.regularMarketPreviousClose, 'f', 4) };

    const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(realTimeDataElement.regularMarketTime);
    m_infoState.searchResult["LastDate"] = { QLocale().toString(dateTime.date(), QLocale::ShortFormat) };
    m_infoState.searchResult["LastTime"] = { QLocale().toString(dateTime.time(), QLocale::ShortFormat) };

    finish();
}

// ── Yahoo History ─────────────────────────────────────────────────────────────
void Parser::doYahooHistoryParsing(const QByteArray& data)
{
    const auto historyData = JsonObjects::Yahoo::HistoryData::fromJson(data);

    if (!historyData.isValid()) {
        setState(ParserErrorCode::NoWebContentLoaded, 0);
        m_busy = false;
        return;
    }

    const auto& result = historyData.results.first();
    const auto& quote  = result.quotes.first();

    m_infoState.dailyValuesList.clear();
    const double progressStep = (100.0 - 10.0) / result.timestamps.size();
    double       progressPercent = 10.0;

    for (int dayIndex = 0; dayIndex < result.timestamps.size(); ++dayIndex) {
        DailyValues dailyValues;
        dailyValues.date         = QDateTime::fromSecsSinceEpoch(result.timestamps[dayIndex]).date();
        dailyValues.openingPrice = qRound(quote.open[dayIndex]  * 100.0) / 100.0;
        dailyValues.closingPrice = qRound(quote.close[dayIndex] * 100.0) / 100.0;
        dailyValues.top          = qRound(quote.high[dayIndex]  * 100.0) / 100.0;
        dailyValues.bottom       = qRound(quote.low[dayIndex]   * 100.0) / 100.0;
        dailyValues.volume       = static_cast<double>(quote.volume[dayIndex]);
        m_infoState.dailyValuesList.append(dailyValues);

        progressPercent += progressStep;
        if (progressPercent < 100)
            setState(ParserErrorCode::SearchRunning, static_cast<int>(progressPercent));
    }

    finish();
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void Parser::setState(ParserErrorCode code, int percentage, const QString& exception)
{
    m_infoState.lastErrorCode = code;
    m_infoState.percentage    = percentage;
    if (!exception.isEmpty())
        m_infoState.exceptionMessage = exception;
    emit parserUpdated(m_infoState);
}

void Parser::finish(ParserErrorCode code)
{
    setState(ParserErrorCode::SearchFinished, 100);
    setState(code, 100);
    m_busy = false;
}

} // namespace ParserLib
