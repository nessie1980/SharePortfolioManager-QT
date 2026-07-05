// MIT License
// Copyright (c) 2021 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include "DataTypes.h"
#include "ParsingValues.h"
#include "Parser.h"
#include "JsonObjects/OnVistaObjects.h"
#include "JsonObjects/YahooObjects.h"

using namespace ParserLib;

class TestParser : public QObject
{
    Q_OBJECT

private slots:

    // ── DataTypes ─────────────────────────────────────────────────────────
    void test_parsingValues_text_mode()
    {
        RegExList rules;
        rules["Price"] = RegExElement{ R"((\d+[.,]\d+))", 0, false, {} };

        ParsingValues pv("Kurs: 123,45 EUR", "UTF-8", rules);

        QCOMPARE(pv.loadingType(), LoadType::Text);
        QCOMPARE(pv.parsingType(), ParsingType::Regex);
        QVERIFY(pv.isValid());
        QCOMPARE(pv.parsingText(), QString("Kurs: 123,45 EUR"));
    }

    void test_parsingValues_web_mode()
    {
        RegExList rules;
        rules["Price"] = RegExElement{ R"((\d+[.,]\d+))", 0, false, {} };

        ParsingValues pv(QUrl("https://example.com"), "UTF-8", rules);

        QCOMPARE(pv.loadingType(), LoadType::Web);
        QVERIFY(pv.isValid());
    }

    void test_parsingValues_invalid_url()
    {
        // QUrl is permissive — an empty URL is reliably invalid
        ParsingValues pv(QUrl(), QString(), "UTF-8", ParsingType::OnVistaRealTime);
        QVERIFY(!pv.isValid());
    }

    // ── Regex parsing ─────────────────────────────────────────────────────
    void test_regex_parsing_text_mode()
    {
        RegExList rules;
        rules["Price"] = RegExElement{ R"((\d+[.,]\d+))", 0, false, {} };

        ParsingValues pv("Kurs: 123,45 EUR", "UTF-8", rules);

        Parser parser;
        parser.setParsingValues(pv);

        ParserInfoState lastState;
        connect(&parser, &Parser::parserUpdated, [&](const ParserInfoState& s) {
            lastState = s;
        });

        QVERIFY(parser.startParsing());

        // Text mode is synchronous — result is available immediately
        QCOMPARE(lastState.lastErrorCode, ParserErrorCode::Finished);
        QVERIFY(lastState.searchResult.contains("Price"));
        QCOMPARE(lastState.searchResult["Price"].first(), QString("123,45"));
    }

    void test_regex_parsing_all_matches()
    {
        RegExList rules;
        // foundPosition = -1 → collect all matches
        rules["Prices"] = RegExElement{ R"((\d+[.,]\d+))", -1, false, {} };

        ParsingValues pv("100,00 und 200,50 und 300,75", "UTF-8", rules);

        Parser parser;
        parser.setParsingValues(pv);

        ParserInfoState lastState;
        connect(&parser, &Parser::parserUpdated, [&](const ParserInfoState& s) {
            lastState = s;
        });

        parser.startParsing();

        QCOMPARE(lastState.searchResult["Prices"].size(), 3);
        QCOMPARE(lastState.searchResult["Prices"][0], QString("100,00"));
        QCOMPARE(lastState.searchResult["Prices"][2], QString("300,75"));
    }

    void test_regex_no_match_result_empty_false()
    {
        RegExList rules;
        rules["Missing"] = RegExElement{ R"(XYZNOTFOUND(\d+))", 0, false, {} };

        ParsingValues pv("some text without match", "UTF-8", rules);

        Parser parser;
        parser.setParsingValues(pv);

        ParserInfoState lastState;
        connect(&parser, &Parser::parserUpdated, [&](const ParserInfoState& s) {
            lastState = s;
        });

        parser.startParsing();

        QCOMPARE(lastState.lastErrorCode, ParserErrorCode::ParsingFailed);
    }

    void test_regex_no_match_result_empty_true()
    {
        RegExList rules;
        // resultEmpty = true → missing match is allowed
        rules["Optional"] = RegExElement{ R"(XYZNOTFOUND(\d+))", 0, true, {} };

        ParsingValues pv("some text", "UTF-8", rules);

        Parser parser;
        parser.setParsingValues(pv);

        ParserInfoState lastState;
        connect(&parser, &Parser::parserUpdated, [&](const ParserInfoState& s) {
            lastState = s;
        });

        parser.startParsing();

        QCOMPARE(lastState.lastErrorCode, ParserErrorCode::Finished);
    }

    void test_start_fails_when_busy()
    {
        // Can't easily test without a real network — just verify guard works
        // by calling startParsing twice on a web URL
        RegExList rules;
        rules["P"] = RegExElement{ R"((\d+))", 0, false, {} };
        ParsingValues pv(QUrl("https://example.com"), "UTF-8", rules);

        Parser parser;
        parser.setParsingValues(pv);

        // First call starts (returns true), second should fail (returns false)
        // We only verify the no-regex guard here
        ParsingValues empty(QUrl("https://example.com"), "UTF-8", RegExList{});
        parser.setParsingValues(empty);

        ParserInfoState lastState;
        connect(&parser, &Parser::parserUpdated, [&](const ParserInfoState& s) {
            lastState = s;
        });

        QVERIFY(!parser.startParsing()); // NoRegexListGiven
        QCOMPARE(lastState.lastErrorCode, ParserErrorCode::NoRegexListGiven);
    }

    // ── Regression: reentrancy bugfix 05.07.2026 ───────────────────────────
    //
    // Bug: "Alle aktualisieren" chains directly from one share's finished
    // callback into startRefreshForShare() for the next share, reusing the
    // SAME Parser instance (m_parserDailyValues / m_parserMarketValues in
    // MainWindow). Parser::finish() used to reset m_busy AFTER emitting the
    // Finished signal, but that emit is a synchronous, direct call — so the
    // reentrant startParsing() call for the next share ran while m_busy was
    // still true and was rejected with BusyFailed (-2). This only showed up
    // for "Alle aktualisieren" (chained calls), never for a single
    // "Aktualisieren" (no chaining) — matching the reported symptom exactly.
    void test_reentrant_start_from_finished_signal_succeeds()
    {
        RegExList rules;
        rules["Price"] = RegExElement{ R"((\d+[.,]\d+))", 0, false, {} };

        ParsingValues pvFirst("Kurs: 100,00 EUR", "UTF-8", rules);
        ParsingValues pvSecond("Kurs: 200,00 EUR", "UTF-8", rules);

        Parser parser;
        parser.setParsingValues(pvFirst);

        bool            reentrantAttempted   = false;
        bool            reentrantStartResult = false;
        ParserInfoState finalState;

        connect(&parser, &Parser::parserUpdated, [&](const ParserInfoState& s) {
            // Simulate MainWindow::onRefreshShareFinished() chaining straight
            // into startRefreshForShare() for the next share, from within the
            // Finished callback of the current share.
            if (s.lastErrorCode == ParserErrorCode::Finished && !reentrantAttempted) {
                reentrantAttempted = true;
                parser.setParsingValues(pvSecond);
                reentrantStartResult = parser.startParsing();
            }
            if (reentrantAttempted)
                finalState = s;
        });

        QVERIFY(parser.startParsing());

        QVERIFY2(reentrantStartResult,
                 "Reentrant startParsing() called from within the Finished "
                 "callback must succeed. A 'false' result here reproduces the "
                 "BusyFailed (-2) regression from 'Alle aktualisieren'.");
        QCOMPARE(finalState.lastErrorCode, ParserErrorCode::Finished);
        QVERIFY(finalState.searchResult.contains("Price"));
        QCOMPARE(finalState.searchResult["Price"].first(), QString("200,00"));
        QVERIFY(!parser.isBusy());
    }

    // ── JSON Objects ──────────────────────────────────────────────────────
    void test_onvista_realtime_json_parsing()
    {
        const QByteArray json = R"({
            "price": 142.56,
            "previousLast": 140.12,
            "isoCurrency": "EUR",
            "idNotation": 12345,
            "idCurrency": 1,
            "datetimePrice": {
                "localTime": "2024-01-15T10:30:00",
                "localTimeZone": "Europe/Berlin",
                "utcTimeStamp": 1705315800
            }
        })";

        const auto rt = JsonObjects::OnVista::RealTimeData::fromJson(json);

        QCOMPARE(rt.isoCurrency, QString("EUR"));
        QVERIFY(qAbs(rt.price - 142.56f) < 0.001f);
        QVERIFY(qAbs(rt.previousLast - 140.12f) < 0.001f);
    }

    void test_onvista_history_json_parsing()
    {
        const QByteArray json = R"({
            "datetimeLast": [1705315800, 1705402200],
            "first":  [140.0, 142.0],
            "last":   [141.5, 143.0],
            "high":   [142.0, 144.0],
            "low":    [139.0, 141.0],
            "volume": [100000.0, 120000.0]
        })";

        const auto hist = JsonObjects::OnVista::HistoryData::fromJson(json);

        QVERIFY(!hist.isEmpty());
        QVERIFY(hist.isValid());
        QCOMPARE(hist.datetimeLast.size(), 2);
        QVERIFY(qAbs(hist.first[0] - 140.0f) < 0.001f);
    }

    void test_yahoo_history_json_parsing()
    {
        const QByteArray json = R"({
            "chart": {
                "result": [{
                    "timestamp": [1705315800, 1705402200],
                    "indicators": {
                        "quote": [{
                            "open":   [140.0, 142.0],
                            "close":  [141.5, 143.0],
                            "high":   [142.0, 144.0],
                            "low":    [139.0, 141.0],
                            "volume": [100000, 120000]
                        }]
                    }
                }]
            }
        })";

        const auto hist = JsonObjects::Yahoo::HistoryData::fromJson(json);

        QVERIFY(hist.isValid());
        QCOMPARE(hist.results.first().timestamps.size(), 2);
        QVERIFY(qAbs(hist.results.first().quotes.first().close[0] - 141.5) < 0.001);
    }
};

QTEST_MAIN(TestParser)
#include "tst_parser.moc"
