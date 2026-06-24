// MIT License
// Copyright (c) 2021 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QObject>
#include <QMap>
#include <QList>
#include <QString>
#include <QDate>
#include <QVariant>
#include <QRegularExpression>

/**
 * @namespace ParserLib
 * @brief Contains all classes and types for the Parser library.
 *
 * The Parser library provides asynchronous content retrieval and parsing.
 * It supports three parsing strategies:
 * - **Regex**: apply named regex rules to a text or downloaded URL content
 * - **OnVista JSON**: parse OnVista Finance API responses (real-time + history)
 * - **Yahoo JSON**: parse Yahoo Finance API responses (real-time + history)
 *
 * ### Typical usage
 * @code
 * using namespace ParserLib;
 *
 * // Build a regex rule set
 * RegExList rules;
 * rules["Price"] = RegExElement{ R"((\d+[.,]\d+))", 0, false, {} };
 *
 * // Configure and start
 * Parser parser;
 * connect(&parser, &Parser::parserUpdated, this, &MyClass::onUpdate);
 * parser.setParsingValues(ParsingValues(url, "UTF-8", rules));
 * parser.startParsing();
 * @endcode
 */
namespace ParserLib {

/**
 * @brief Daily OHLCV (Open/High/Low/Close/Volume) values for one trading day.
 */
struct DailyValues
{
    QDate   date;                ///< Trading date
    double  openingPrice = 0.0; ///< Opening price
    double  closingPrice = 0.0; ///< Closing price
    double  top          = 0.0; ///< Day high
    double  bottom       = 0.0; ///< Day low
    double  volume       = 0.0; ///< Trading volume
};

/**
 * @brief A single named regex rule used by the Regex parsing strategy.
 *
 * Each RegExElement defines:
 * - The regex pattern to apply
 * - Which match index to use (-1 = all matches)
 * - Whether an empty result is acceptable
 * - Optional Qt regex pattern options (e.g. CaseInsensitiveOption)
 */
struct RegExElement
{
    QString regexExpression;    ///< Qt regular expression pattern
    int     regexFoundPosition = 0; ///< Match index to use; -1 collects all matches
    bool    resultEmpty        = false; ///< If true, no match is not treated as an error
    QList<QRegularExpression::PatternOption> regexOptions; ///< Pattern options
};

/**
 * @brief Named collection of regex rules.
 *
 * Key: result name (e.g. "Price", "Currency", "LastDate")
 * Value: the corresponding RegExElement
 */
using RegExList = QMap<QString, RegExElement>;

/**
 * @brief Specifies where the content to parse comes from.
 */
enum class LoadType {
    Web,    ///< Download content from a URL
    Text    ///< Use a directly provided text string
};

/**
 * @brief Specifies how the downloaded or given content is parsed.
 */
enum class ParsingType {
    Regex,                ///< Apply RegExList rules to the text content
    OnVistaRealTime,      ///< Deserialize OnVista real-time JSON response
    OnVistaHistoryData,   ///< Deserialize OnVista daily OHLCV JSON response
    YahooRealTime,        ///< Deserialize Yahoo Finance real-time JSON response
    YahooHistoryData      ///< Deserialize Yahoo Finance daily OHLCV JSON response
};

/**
 * @brief Current lifecycle state of the parser.
 */
enum class ParserState {
    Idle,     ///< No operation running
    Started,  ///< Operation just started
    Loading,  ///< Downloading content
    Parsing   ///< Parsing content
};

/**
 * @brief Result and progress codes emitted with every parserUpdated() signal.
 *
 * Positive values indicate progress; zero means no error; negative values indicate errors.
 *
 * | Code | Value | Meaning |
 * |------|-------|---------|
 * | Finished | 8 | Parsing complete — results available |
 * | SearchFinished | 7 | Search phase complete |
 * | SearchRunning | 6 | Search in progress |
 * | SearchStarted | 5 | Search phase started |
 * | ContentLoadFinished | 4 | Download complete |
 * | ContentLoadStarted | 3 | Download started |
 * | Started | 2 | Operation started |
 * | Starting | 1 | Operation initializing |
 * | NoError | 0 | No error |
 * | StartFailed | -1 | Could not start |
 * | BusyFailed | -2 | Already running |
 * | InvalidWebSiteGiven | -3 | URL is empty or invalid |
 * | NoRegexListGiven | -4 | Regex mode but no rules provided |
 * | NoWebContentLoaded | -5 | Download returned empty content |
 * | ParsingFailed | -6 | A required regex had no match |
 * | CancelOperation | -7 | Cancelled by caller |
 * | NetworkError | -8 | Qt network error |
 * | FileError | -9 | File I/O error |
 * | JsonError | -10 | JSON deserialization error |
 * | UnknownError | -11 | Unexpected exception |
 */
enum class ParserErrorCode : int {
    Finished              =  8,
    SearchFinished        =  7,
    SearchRunning         =  6,
    SearchStarted         =  5,
    ContentLoadFinished   =  4,
    ContentLoadStarted    =  3,
    Started               =  2,
    Starting              =  1,
    NoError               =  0,
    StartFailed           = -1,
    BusyFailed            = -2,
    InvalidWebSiteGiven   = -3,
    NoRegexListGiven      = -4,
    NoWebContentLoaded    = -5,
    ParsingFailed         = -6,
    CancelOperation       = -7,
    NetworkError          = -8,
    FileError             = -9,
    JsonError             = -10,
    UnknownError          = -11
};

/**
 * @brief Complete snapshot of the parser state at a given point in time.
 *
 * A `ParserInfoState` is passed with every parserUpdated() signal emission.
 * When `lastErrorCode == ParserErrorCode::Finished`, the results are available in:
 * - `searchResult` — for Regex parsing
 * - `dailyValuesList` — for history parsing (OnVista / Yahoo)
 *
 * For real-time parsing (OnVista / Yahoo), results are also placed in `searchResult`
 * with keys: `"Currency"`, `"Price"`, `"PriceBefore"`, `"LastDate"`, `"LastTime"`.
 */
struct ParserInfoState
{
    QString                         webSite;                  ///< URL being processed
    QString                         userAgentIdentifier;      ///< HTTP User-Agent used
    ParserState                     state             = ParserState::Idle; ///< Current state
    int                             percentage        = 0;    ///< Overall progress (0–100)
    int                             percentageDownload = 0;   ///< Download progress (0–100)
    QByteArray                      webSiteContentAsByteArray; ///< Raw downloaded bytes
    QString                         webSiteContentAsString;   ///< Downloaded content as string
    ParserErrorCode                 lastErrorCode     = ParserErrorCode::NoError; ///< Last code
    RegExList                       regexList;                ///< Active regex rules
    QString                         lastRegexListKey;         ///< Last processed regex key
    QMap<QString, QList<QString>>   searchResult;             ///< Regex / real-time results
    QList<DailyValues>              dailyValuesList;          ///< History results
    QString                         exceptionMessage;         ///< Error message if any
};

} // namespace ParserLib
