// MIT License
// Copyright (c) 2026 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QUrl>
#include <QMap>
#include <QString>

namespace ParserTestUtils {

/**
 * @brief Canned response registered for a specific request URL.
 */
struct FakeResponse
{
    QByteArray             body;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    QString                 errorString;
};

/**
 * @brief QNetworkAccessManager subclass for tests.
 *
 * Intercepts createRequest() and returns a FakeNetworkReply carrying a
 * pre-registered canned response instead of performing a real HTTP request.
 * This is the test seam that the ParserLib::Parser(QNetworkAccessManager*, ...)
 * constructor was added for — it lets tests exercise the full Parser state
 * machine (busy/reentrancy handling, regex parsing, OnVista/Yahoo JSON
 * mapping, error codes) without any network access.
 *
 * ### Usage
 * @code
 * ParserTestUtils::FakeNetworkAccessManager nam;
 * nam.setResponse(QUrl("https://api.example.com/price"),
 *                 QByteArrayLiteral(R"({"price": 42.5})"));
 *
 * ParserLib::Parser parser(&nam);
 * parser.setParsingValues(ParserLib::ParsingValues(
 *     QUrl("https://api.example.com/price"), "", "UTF-8",
 *     ParserLib::ParsingType::OnVistaRealTime));
 * parser.startParsing();
 * // parserUpdated() fires synchronously-ish via a queued 0ms timer — pump
 * // the event loop once with QSignalSpy::wait() or QTest::qWait(0) in tests.
 * @endcode
 *
 * If no response was registered for a requested URL, an empty successful
 * body is returned. Parser then reports NoWebContentLoaded, same as a real
 * empty HTTP response — this makes an unregistered URL fail loudly in test
 * output instead of hanging on a real network call.
 *
 * Ownership: the FakeNetworkAccessManager is NOT owned by Parser (see the
 * injecting constructor's docs in Parser.h) — the test fixture owns it, e.g.
 * as a member or stack variable that outlives the Parser instance under test.
 */
class FakeNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    explicit FakeNetworkAccessManager(QObject* parent = nullptr);

    /// Register a canned successful response for an exact URL match.
    void setResponse(const QUrl& url, const QByteArray& body);

    /// Register a canned network error for an exact URL match.
    void setError(const QUrl& url, QNetworkReply::NetworkError error,
                 const QString& errorString = {});

    /// Number of requests observed so far (for assertions).
    int requestCount() const { return m_requestCount; }

    /// Most recently requested URL — useful to assert on URLs built by
    /// production code (e.g. MainWindow::buildDailyValuesUrl()) without
    /// needing to make that function itself network-aware.
    QUrl lastRequestedUrl() const { return m_lastUrl; }

protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request,
                                 QIODevice* outgoingData = nullptr) override;

private:
    QMap<QUrl, FakeResponse> m_responses;
    int                       m_requestCount = 0;
    QUrl                      m_lastUrl;
};

} // namespace ParserTestUtils
