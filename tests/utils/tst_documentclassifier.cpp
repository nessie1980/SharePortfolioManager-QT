// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "../../app/utils/DocumentClassifier.h"
#include "../../app/config/DocumentsConfig.h"

/**
 * @brief Unit tests for DocumentClassifier.
 *
 * Pure logic tests — no GUI, no pdftotext, no database. Builds a small
 * Documents.xml fixture per test via QTemporaryDir, following the exact
 * same pattern as tests/config/tst_documentsconfig.cpp (see TESTING.md).
 */
class TestDocumentClassifier : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    QString writeXml(const QString& fileName, const QString& content)
    {
        const QString path = m_tempDir.path() + QLatin1Char('/') + fileName;
        QFile file(path);
        const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Text);
        Q_ASSERT(opened);
        QTextStream(&file) << content;
        file.close();
        return path;
    }

    /**
     * @brief One bank ("TestBank") with Buy/Sale/Dividend/Brokerage
     * identifiers and a Buy document entry containing Wkn/Isin/Date rules.
     *
     * - BankIdentifier matches "Depotnummer 123456"
     * - BuyIdentifier matches "Wertpapier Abrechnung Kauf"
     * - SaleIdentifier matches "Wertpapier Abrechnung Verkauf"
     * - DividendIdentifier matches "Dividendengutschrift"
     * - BrokerageIdentifier matches "Kostenausweis"
     */
    QString validXml() const
    {
        return QStringLiteral(
            "<?xml version=\"1.0\"?><Documents>"
            "<Bank Name=\"TestBank\" BankIdentifierValue=\"123456\" Encoding=\"UTF-8\">"
            "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Depotnummer\\s+(\\d+)</BankIdentifier>"
            "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Wertpapier Abrechnung Kauf</BuyIdentifier>"
            "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Wertpapier Abrechnung Verkauf</SaleIdentifier>"
            "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Dividendengutschrift</DividendIdentifier>"
            "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Kostenausweis</BrokerageIdentifier>"
            "<Document Type=\"Buy\" TypeIdentifierValue=\"Wertpapier Abrechnung Kauf\" Encoding=\"UTF-8\">"
            "<Wkn Name=\"Wkn\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">WKN:\\s+([A-Z0-9]{6})</Wkn>"
            "<Isin Name=\"Isin\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">ISIN:\\s+([A-Z0-9]{12})</Isin>"
            "<Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" "
                "RegexOptions=\"None\">Datum:\\s+(\\d{2}.\\d{2}.\\d{4})</Date>"
            "</Document>"
            "<Document Type=\"Sale\" TypeIdentifierValue=\"Wertpapier Abrechnung Verkauf\" Encoding=\"UTF-8\">"
            "<Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" "
                "RegexOptions=\"None\">Datum:\\s+(\\d{2}.\\d{2}.\\d{4})</Date>"
            "</Document>"
            "<Document Type=\"Dividend\" TypeIdentifierValue=\"Dividendengutschrift\" Encoding=\"UTF-8\">"
            "<Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" "
                "RegexOptions=\"None\">Datum:\\s+(\\d{2}.\\d{2}.\\d{4})</Date>"
            "</Document>"
            "</Bank>"
            "</Documents>");
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
    }

    // ── classify() ──────────────────────────────────────────────────────

    void test_classify_buyDocument_matched()
    {
        DocumentsConfig config;
        QCOMPARE(config.load(writeXml(QStringLiteral("buy.xml"), validXml())),
                 DocumentsConfig::LoadResult::Success);

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Kauf\nDatum: 01.07.2026\n"
            "WKN: BASF11\nISIN: DE000BASF111");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Buy);
        QCOMPARE(result.bank.name, QStringLiteral("TestBank"));
        QCOMPARE(result.docEntry.type, DocumentType::Buy);
    }

    void test_classify_saleDocument_matched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("sale.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Verkauf\nDatum: 02.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Sale);
    }

    void test_classify_dividendDocument_matched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("dividend.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nDividendengutschrift\nDatum: 03.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Dividend);
    }

    void test_classify_unknownBank_notMatched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("unknownbank.xml"), validXml()));

        const QString text = QStringLiteral("Irgendein Dokument ohne Bank-Bezug.");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(!result.matched);
    }

    void test_classify_knownBank_unknownType_notMatched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("unknowntype.xml"), validXml()));

        // Bank identifier matches, but none of the four document identifiers do.
        const QString text = QStringLiteral("Depotnummer 123456\nIrgendein anderer Text.");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(!result.matched);
    }

    void test_classify_emptyConfig_notMatched()
    {
        DocumentsConfig config; // never loaded — isValid() == false, entries() empty
        const auto result = DocumentClassifier::classify(QStringLiteral("egal"), config);
        QVERIFY(!result.matched);
    }

    // ── extractFieldValue() / extractWkn() / extractIsin() ────────────────

    void test_extractWkn_found()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("wkn.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Kauf\nWKN: BASF11\nISIN: DE000BASF111");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);

        const QString wkn = DocumentClassifier::extractWkn(text, result.docEntry);
        QCOMPARE(wkn, QStringLiteral("BASF11"));
    }

    void test_extractIsin_found()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("isin.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Kauf\nWKN: BASF11\nISIN: DE000BASF111");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);

        const QString isin = DocumentClassifier::extractIsin(text, result.docEntry);
        QCOMPARE(isin, QStringLiteral("DE000BASF111"));
    }

    void test_extractWkn_notPresentInDocType_returnsEmpty()
    {
        // The "Sale" document entry in validXml() has no Wkn rule at all.
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("nowkn.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Verkauf\nDatum: 02.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Sale);

        QVERIFY(DocumentClassifier::extractWkn(text, result.docEntry).isEmpty());
    }

    void test_extractFieldValue_noMatch_returnsEmpty()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("nomatch.xml"), validXml()));

        const auto result = DocumentClassifier::classify(
            QStringLiteral("Depotnummer 123456\nWertpapier Abrechnung Kauf"), config);
        QVERIFY(result.matched);

        // Text has no "WKN: ..." at all.
        QVERIFY(DocumentClassifier::extractWkn(QStringLiteral("kein WKN hier"),
                                               result.docEntry).isEmpty());
    }

    // ── matchBankIndex() / detectDocumentType() ────────────────────────────
    // (used by the refactored PresenterBuyEdit/PresenterSaleEdit/
    // PresenterDividendEdit/PresenterShareAdd — see ARCHITECTURE.md)

    void test_matchBankIndex_found()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("bankidx.xml"), validXml()));

        int index = -1;
        const bool found = DocumentClassifier::matchBankIndex(
            QStringLiteral("Depotnummer 123456"), config, index);
        QVERIFY(found);
        QCOMPARE(index, 0);
    }

    void test_matchBankIndex_notFound_leavesIndexUnchanged()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("bankidx2.xml"), validXml()));

        int index = -7;
        const bool found = DocumentClassifier::matchBankIndex(
            QStringLiteral("kein Depot hier"), config, index);
        QVERIFY(!found);
        QCOMPARE(index, -7); // unchanged
    }

    void test_detectDocumentType_matches_buyIdentifier()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("detecttype.xml"), validXml()));

        int index = -1;
        QVERIFY(DocumentClassifier::matchBankIndex(
            QStringLiteral("Depotnummer 123456\nWertpapier Abrechnung Kauf"), config, index));
        const BankEntry bank = config.entries().at(index);

        const DocumentType type = DocumentClassifier::detectDocumentType(
            QStringLiteral("Depotnummer 123456\nWertpapier Abrechnung Kauf"),
            bank, DocumentType::Dividend /* deliberately "wrong" fallback */);
        QCOMPARE(type, DocumentType::Buy); // identifier match wins over fallback
    }

    void test_detectDocumentType_noIdentifierMatch_returnsFallback()
    {
        // Mirrors e.g. PresenterSaleEdit::startParserForText(), which defaults
        // to DocumentType::Sale when the bank matched but no explicit
        // Buy-/Sale-/Dividend-/BrokerageIdentifier does — the user already
        // chose the "Verkäufe hinzufügen" dialog, so guessing Sale is correct
        // there (unlike DocumentClassifier::classify(), which never guesses).
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("detecttype2.xml"), validXml()));

        int index = -1;
        QVERIFY(DocumentClassifier::matchBankIndex(
            QStringLiteral("Depotnummer 123456\nirgendein anderer Text"), config, index));
        const BankEntry bank = config.entries().at(index);

        const DocumentType type = DocumentClassifier::detectDocumentType(
            QStringLiteral("Depotnummer 123456\nirgendein anderer Text"),
            bank, DocumentType::Sale);
        QCOMPARE(type, DocumentType::Sale); // fallback, since nothing matched
    }
};

QTEST_MAIN(TestDocumentClassifier)
#include "tst_documentclassifier.moc"
