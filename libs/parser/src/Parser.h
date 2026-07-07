// MIT License
// Copyright (c) 2021 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "DataTypes.h"
#include "ParsingValues.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

namespace ParserLib {

/**
 * @brief Asynchronous content parser.
 *
 * Supports three parsing modes:
 *  - **Regex**: downloads or uses given text, applies RegExList rules
 *  - **OnVista JSON**: downloads OnVista API response, maps to ParserInfoState
 *  - **Yahoo JSON**: downloads Yahoo Finance API response, maps to ParserInfoState
 *
 * All operations are non-blocking. Connect to parserUpdated() to receive
 * progress and result notifications.
 *
 * Usage:
 * @code
 *   Parser parser;
 *   connect(&parser, &Parser::parserUpdated,
 *           this, &MyClass::onParserUpdate);
 *
 *   RegExList rules;
 *   rules["Price"] = { .regexExpression = R"((\d+[.,]\d+))", .regexFoundPosition = 0 };
 *
 *   parser.setParsingValues(ParsingValues(url, "UTF-8", rules));
 *   parser.startParsing();
 * @endcode
 */
class Parser : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Returns the Parser library version string.
     * @return Version string (e.g. "1.0.0").
     */
    static QString version() { return QStringLiteral("1.0.0"); }

    /**
     * @brief Construct a Parser with its own internally owned QNetworkAccessManager.
     *
     * This is the constructor used in production code (e.g. MainWindow's
     * m_parserMarketValues / m_parserDailyValues). The QNetworkAccessManager
     * is created with `this` as parent and destroyed automatically together
     * with the Parser.
     */
    explicit Parser(QObject* parent = nullptr);

    /**
     * @brief Construct a Parser using an externally provided QNetworkAccessManager.
     *
     * Ownership of @p networkManager stays with the caller — Parser neither
     * deletes nor reparents it.
     *
     * This constructor exists as a test seam: it lets unit tests inject a
     * QNetworkAccessManager subclass (see ParserTestUtils::FakeNetworkAccessManager
     * in tests/parser/) that intercepts createRequest() and returns a canned
     * QNetworkReply instead of performing a real HTTP request. That way the
     * full Parser state machine — URL building call sites, busy/reentrancy
     * handling, regex parsing, OnVista/Yahoo JSON mapping, error codes — can
     * be exercised end-to-end without any network access.
     *
     * @param networkManager  Externally owned QNetworkAccessManager to use for all requests.
     * @param parent          Optional QObject parent for the Parser instance itself.
     */
    explicit Parser(QNetworkAccessManager* networkManager, QObject* parent = nullptr);

    /// Set the parsing configuration before calling startParsing().
    void setParsingValues(const ParsingValues& values);

    /// Returns current parsing configuration.
    ParsingValues parsingValues() const { return m_parsingValues; }

    /// Start the parse operation. Returns false if already running or invalid.
    bool startParsing();

    /// Cancel a running parse operation.
    void cancelParsing();

    /// User-Agent header sent with HTTP requests.
    void setUserAgentIdentifier(const QString& agent) { m_userAgentIdentifier = agent; }
    QString userAgentIdentifier() const { return m_userAgentIdentifier; }

    /// Returns the current state snapshot.
    ParserInfoState parserInfoState() const { return m_infoState; }

    /// Returns true if a parse operation is currently running.
    bool isBusy() const { return m_busy; }

signals:
    /**
     * @brief Emitted on every state change during parsing.
     *
     * Check ParserInfoState::lastErrorCode:
     *  - >= Starting (1): progress
     *  - Finished (8):    results are ready in searchResult / dailyValuesList
     *  - < 0:             error occurred
     */
    void parserUpdated(const ParserLib::ParserInfoState& state);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    void doRegexParsing(const QString& text);
    void doOnVistaRealTimeParsing(const QByteArray& data);
    void doOnVistaHistoryParsing(const QByteArray& data);
    void doYahooRealTimeParsing(const QByteArray& data);
    void doYahooHistoryParsing(const QByteArray& data);

    void setState(ParserErrorCode code, int percentage, const QString& exception = {});
    void finish(ParserErrorCode code = ParserErrorCode::Finished);

    ParsingValues          m_parsingValues;
    ParserInfoState        m_infoState;
    QNetworkAccessManager* m_networkManager = nullptr;
    QNetworkReply*         m_reply          = nullptr;
    bool                   m_busy           = false;
    bool                   m_cancelled      = false;

    QString m_userAgentIdentifier =
        QStringLiteral("Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0");
};

} // namespace ParserLib
