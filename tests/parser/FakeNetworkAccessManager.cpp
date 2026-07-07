// MIT License
// Copyright (c) 2026 nessie1980 (nessie1980@gmx.de)
#include "FakeNetworkAccessManager.h"

#include <QTimer>
#include <cstring>

namespace ParserTestUtils {

// ── FakeNetworkReply ──────────────────────────────────────────────────────────
//
// Minimal QNetworkReply subclass that immediately (via a 0ms singleShot
// QTimer, so tests still need one event-loop turn) delivers a canned body or
// error, without touching the network. Follows the standard Qt pattern for
// faking QNetworkReply: buffer the whole body up front, serve it from
// readData(), and use the protected setError()/setFinished() setters that
// QNetworkReply provides for exactly this purpose.
class FakeNetworkReply : public QNetworkReply
{
public:
    FakeNetworkReply(const QUrl& url, const FakeResponse& response, QObject* parent)
        : QNetworkReply(parent)
        , m_body(response.body)
    {
        setUrl(url);
        setOperation(QNetworkAccessManager::GetOperation);
        setRequest(QNetworkRequest(url));
        open(QIODevice::ReadOnly);

        if (response.error != QNetworkReply::NoError) {
            setError(response.error,
                     response.errorString.isEmpty()
                         ? QStringLiteral("Fake network error")
                         : response.errorString);
            QTimer::singleShot(0, this, [this, response]() {
                emit errorOccurred(response.error);
                setFinished(true);
                emit finished();
            });
            return;
        }

        setHeader(QNetworkRequest::ContentLengthHeader, QVariant(m_body.size()));

        QTimer::singleShot(0, this, [this]() {
            emit downloadProgress(m_body.size(), m_body.size());
            emit readyRead();
            setFinished(true);
            emit finished();
        });
    }

    void abort() override
    {
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("aborted"));
        setFinished(true);
        emit finished();
    }

    qint64 bytesAvailable() const override
    {
        return (m_body.size() - m_readPos) + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        const qint64 remaining = m_body.size() - m_readPos;
        if (remaining <= 0)
            return -1; // EOF

        const qint64 toCopy = std::min(maxSize, remaining);
        std::memcpy(data, m_body.constData() + m_readPos, static_cast<size_t>(toCopy));
        m_readPos += toCopy;
        return toCopy;
    }

private:
    QByteArray m_body;
    qint64     m_readPos = 0;
};

// ── FakeNetworkAccessManager ──────────────────────────────────────────────────
FakeNetworkAccessManager::FakeNetworkAccessManager(QObject* parent)
    : QNetworkAccessManager(parent)
{}

void FakeNetworkAccessManager::setResponse(const QUrl& url, const QByteArray& body)
{
    m_responses.insert(url, FakeResponse{ body, QNetworkReply::NoError, {} });
}

void FakeNetworkAccessManager::setError(const QUrl& url, QNetworkReply::NetworkError error,
                                        const QString& errorString)
{
    m_responses.insert(url, FakeResponse{ QByteArray(), error, errorString });
}

QNetworkReply* FakeNetworkAccessManager::createRequest(Operation /*op*/,
                                                        const QNetworkRequest& request,
                                                        QIODevice* /*outgoingData*/)
{
    ++m_requestCount;
    m_lastUrl = request.url();

    const FakeResponse response = m_responses.value(request.url(), FakeResponse{});
    return new FakeNetworkReply(request.url(), response, this);
}

} // namespace ParserTestUtils
