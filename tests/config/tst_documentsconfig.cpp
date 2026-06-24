// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "../../app/config/DocumentsConfig.h"

class TestDocumentsConfig : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    QString writeXml(const QString& filename, const QString& content) const
    {
        const QString path = m_tempDir.path() + QStringLiteral("/") + filename;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return QString();
        QTextStream stream(&file);
        stream << content;
        return path;
    }

    // Minimal valid XML with one bank, all identifiers and one Buy document
    QString validXml() const
    {
        return QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Documents>\n"
            "  <Bank Name=\"TestBank\" BankIdentifierValue=\"123456\" Encoding=\"UTF-8\">\n"
            "    <BankIdentifier     Name=\"BankIdentifier\"     FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">Depot([0-9]+)</BankIdentifier>\n"
            "    <BuyIdentifier      Name=\"BuyIdentifier\"      FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">(Kauf)</BuyIdentifier>\n"
            "    <SaleIdentifier     Name=\"SaleIdentifier\"     FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">(Verkauf)</SaleIdentifier>\n"
            "    <DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">(Dividende)</DividendIdentifier>\n"
            "    <BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">(Kosten)</BrokerageIdentifier>\n"
            "    <Document Type=\"Buy\" TypeIdentifierValue=\"Kauf\" Encoding=\"UTF-8\">\n"
            "      <Wkn  Name=\"Wkn\"  FoundIndex=\"0\" ResultEmpty=\"true\"  RegexOptions=\"None\">([A-Z0-9]{6})</Wkn>\n"
            "      <Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">([0-9]{2}[.][0-9]{2}[.][0-9]{4})</Date>\n"
            "      <Price Name=\"Price\" FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"Multiline\">([0-9.,]+)</Price>\n"
            "    </Document>\n"
            "  </Bank>\n"
            "</Documents>\n");
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
    }

    // ── load() — error cases ──────────────────────────────────────────────

    void test_load_fileNotFound()
    {
        DocumentsConfig config;
        QCOMPARE(config.load(m_tempDir.path() + QStringLiteral("/nonexistent.xml")),
                 DocumentsConfig::LoadResult::FileNotFound);
        QVERIFY(!config.isValid());
        QVERIFY(!config.lastError().isEmpty());
    }

    void test_load_emptyConfig()
    {
        const QString path = writeXml(QStringLiteral("empty.xml"),
            QStringLiteral("<?xml version=\"1.0\"?><Documents></Documents>"));
        DocumentsConfig config;
        QCOMPARE(config.load(path), DocumentsConfig::LoadResult::EmptyConfig);
        QVERIFY(!config.isValid());
    }

    void test_load_malformedXml()
    {
        const QString path = writeXml(QStringLiteral("bad.xml"),
            QStringLiteral("<?xml version=\"1.0\"?><Documents><Bank unclosed"));
        DocumentsConfig config;
        QCOMPARE(config.load(path), DocumentsConfig::LoadResult::XmlParseError);
    }

    void test_load_missingBankName_returnsAttributeError()
    {
        const QString path = writeXml(QStringLiteral("nobankname.xml"),
            QStringLiteral(
                "<?xml version=\"1.0\"?><Documents>"
                "<Bank BankIdentifierValue=\"123\" Encoding=\"UTF-8\">"
                "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</BankIdentifier>"
                "</Bank></Documents>"));
        DocumentsConfig config;
        QCOMPARE(config.load(path), DocumentsConfig::LoadResult::BankAttributeError);
    }

    void test_load_missingIdentifierName_returnsAttributeError()
    {
        const QString path = writeXml(QStringLiteral("noidname.xml"),
            QStringLiteral(
                "<?xml version=\"1.0\"?><Documents>"
                "<Bank Name=\"B\" BankIdentifierValue=\"1\" Encoding=\"UTF-8\">"
                "<BankIdentifier FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</BankIdentifier>"
                "</Bank></Documents>"));
        DocumentsConfig config;
        QCOMPARE(config.load(path), DocumentsConfig::LoadResult::IdentifierAttributeError);
    }

    void test_load_missingDocumentType_returnsAttributeError()
    {
        const QString path = writeXml(QStringLiteral("nodoctype.xml"),
            QStringLiteral(
                "<?xml version=\"1.0\"?><Documents>"
                "<Bank Name=\"B\" BankIdentifierValue=\"1\" Encoding=\"UTF-8\">"
                "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</BankIdentifier>"
                "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</BuyIdentifier>"
                "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</SaleIdentifier>"
                "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</DividendIdentifier>"
                "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</BrokerageIdentifier>"
                "<Document TypeIdentifierValue=\"Kauf\" Encoding=\"UTF-8\">"
                "<Wkn Name=\"Wkn\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</Wkn>"
                "</Document>"
                "</Bank></Documents>"));
        DocumentsConfig config;
        QCOMPARE(config.load(path), DocumentsConfig::LoadResult::DocumentAttributeError);
    }

    void test_load_emptyDocument_returnsDocumentElementError()
    {
        const QString path = writeXml(QStringLiteral("emptydoc.xml"),
            QStringLiteral(
                "<?xml version=\"1.0\"?><Documents>"
                "<Bank Name=\"B\" BankIdentifierValue=\"1\" Encoding=\"UTF-8\">"
                "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</BankIdentifier>"
                "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</BuyIdentifier>"
                "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</SaleIdentifier>"
                "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</DividendIdentifier>"
                "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</BrokerageIdentifier>"
                "<Document Type=\"Buy\" TypeIdentifierValue=\"Kauf\" Encoding=\"UTF-8\">"
                "</Document>"
                "</Bank></Documents>"));
        DocumentsConfig config;
        QCOMPARE(config.load(path), DocumentsConfig::LoadResult::DocumentElementError);
    }

    // ── load() — success ──────────────────────────────────────────────────

    void test_load_success()
    {
        DocumentsConfig config;
        QCOMPARE(config.load(writeXml(QStringLiteral("valid.xml"), validXml())),
                 DocumentsConfig::LoadResult::Success);
        QVERIFY(config.isValid());
        QCOMPARE(config.count(), 1);
    }

    void test_load_multipleBanks()
    {
        const QString path = writeXml(QStringLiteral("multi.xml"),
            QStringLiteral(
                "<?xml version=\"1.0\"?><Documents>"
                "<Bank Name=\"BankA\" BankIdentifierValue=\"1\" Encoding=\"UTF-8\">"
                "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</BankIdentifier>"
                "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</BuyIdentifier>"
                "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</SaleIdentifier>"
                "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</DividendIdentifier>"
                "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</BrokerageIdentifier>"
                "<Document Type=\"Buy\" TypeIdentifierValue=\"Kauf\" Encoding=\"UTF-8\">"
                "<Wkn Name=\"Wkn\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</Wkn>"
                "</Document>"
                "</Bank>"
                "<Bank Name=\"BankB\" BankIdentifierValue=\"2\" Encoding=\"UTF-8\">"
                "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</BankIdentifier>"
                "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</BuyIdentifier>"
                "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</SaleIdentifier>"
                "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</DividendIdentifier>"
                "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</BrokerageIdentifier>"
                "<Document Type=\"Buy\" TypeIdentifierValue=\"Kauf\" Encoding=\"UTF-8\">"
                "<Wkn Name=\"Wkn\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">y</Wkn>"
                "</Document>"
                "</Bank>"
                "</Documents>"));
        DocumentsConfig config;
        QCOMPARE(config.load(path), DocumentsConfig::LoadResult::Success);
        QCOMPARE(config.count(), 2);
    }

    // ── Bank entry content ────────────────────────────────────────────────

    void test_bank_attributes()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid2.xml"), validXml()));

        const BankEntry bank = config.entries().first();
        QCOMPARE(bank.name,       QStringLiteral("TestBank"));
        QCOMPARE(bank.identifier, QStringLiteral("123456"));
        QCOMPARE(bank.encoding,   QStringLiteral("UTF-8"));
    }

    void test_bank_identifierRegexList()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid3.xml"), validXml()));

        const BankEntry bank = config.entries().first();
        QVERIFY(bank.identifierRegexList.contains(QStringLiteral("BankIdentifier")));
        QVERIFY(bank.identifierRegexList.contains(QStringLiteral("BuyIdentifier")));
        QVERIFY(bank.identifierRegexList.contains(QStringLiteral("SaleIdentifier")));
        QVERIFY(bank.identifierRegexList.contains(QStringLiteral("DividendIdentifier")));
        QVERIFY(bank.identifierRegexList.contains(QStringLiteral("BrokerageIdentifier")));
    }

    void test_document_loaded()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid4.xml"), validXml()));

        const BankEntry bank = config.entries().first();
        QVERIFY(bank.documents.contains(DocumentType::Buy));
    }

    void test_document_typeIdentifier()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid5.xml"), validXml()));

        const BankEntry bank = config.entries().first();
        const DocumentEntry* doc = DocumentsConfig::findDocument(bank, DocumentType::Buy);
        QVERIFY(doc != nullptr);
        QCOMPARE(doc->typeIdentifier, QStringLiteral("Kauf"));
        QCOMPARE(doc->encoding,       QStringLiteral("UTF-8"));
    }

    void test_document_regexList()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid6.xml"), validXml()));

        const BankEntry bank = config.entries().first();
        const DocumentEntry* doc = DocumentsConfig::findDocument(bank, DocumentType::Buy);
        QVERIFY(doc != nullptr);
        QVERIFY(doc->regexList.contains(QStringLiteral("Wkn")));
        QVERIFY(doc->regexList.contains(QStringLiteral("Date")));
        QVERIFY(doc->regexList.contains(QStringLiteral("Price")));
    }

    void test_document_regexOptions_multiline()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid7.xml"), validXml()));

        const BankEntry bank = config.entries().first();
        const DocumentEntry* doc = DocumentsConfig::findDocument(bank, DocumentType::Buy);
        QVERIFY(doc != nullptr);
        // Price has RegexOptions="Multiline"
        const auto& priceRule = doc->regexList.value(QStringLiteral("Price"));
        QVERIFY(priceRule.regexOptions.contains(QRegularExpression::MultilineOption));
    }

    // ── findByName ────────────────────────────────────────────────────────

    void test_findByName_found()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid8.xml"), validXml()));

        const BankEntry* bank = config.findByName(QStringLiteral("TestBank"));
        QVERIFY(bank != nullptr);
        QCOMPARE(bank->name, QStringLiteral("TestBank"));
    }

    void test_findByName_notFound()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid9.xml"), validXml()));

        QVERIFY(config.findByName(QStringLiteral("NonExistentBank")) == nullptr);
    }

    // ── findDocument ──────────────────────────────────────────────────────

    void test_findDocument_found()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid10.xml"), validXml()));

        const BankEntry bank = config.entries().first();
        QVERIFY(DocumentsConfig::findDocument(bank, DocumentType::Buy) != nullptr);
    }

    void test_findDocument_notFound()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("valid11.xml"), validXml()));

        // Only Buy document was defined in validXml()
        const BankEntry bank = config.entries().first();
        QVERIFY(DocumentsConfig::findDocument(bank, DocumentType::Sale) == nullptr);
    }

    // ── documentTypeFromString ────────────────────────────────────────────

    void test_documentTypeFromString()
    {
        QCOMPARE(DocumentsConfig::documentTypeFromString(QStringLiteral("Buy")),       DocumentType::Buy);
        QCOMPARE(DocumentsConfig::documentTypeFromString(QStringLiteral("Sale")),      DocumentType::Sale);
        QCOMPARE(DocumentsConfig::documentTypeFromString(QStringLiteral("Dividend")),  DocumentType::Dividend);
        QCOMPARE(DocumentsConfig::documentTypeFromString(QStringLiteral("Brokerage")), DocumentType::Brokerage);
        // Unknown → fallback to Buy
        QCOMPARE(DocumentsConfig::documentTypeFromString(QStringLiteral("Unknown")),   DocumentType::Buy);
    }

    // ── reload ────────────────────────────────────────────────────────────

    void test_reload_clearsOldEntries()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("r1.xml"), validXml()));
        QCOMPARE(config.count(), 1);

        // Load a file with two banks
        const QString path2 = writeXml(QStringLiteral("r2.xml"),
            QStringLiteral(
                "<?xml version=\"1.0\"?><Documents>"
                "<Bank Name=\"A\" BankIdentifierValue=\"1\" Encoding=\"UTF-8\">"
                "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</BankIdentifier>"
                "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</BuyIdentifier>"
                "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</SaleIdentifier>"
                "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</DividendIdentifier>"
                "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">a</BrokerageIdentifier>"
                "<Document Type=\"Buy\" TypeIdentifierValue=\"X\" Encoding=\"UTF-8\">"
                "<Wkn Name=\"Wkn\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">x</Wkn>"
                "</Document></Bank>"
                "<Bank Name=\"B\" BankIdentifierValue=\"2\" Encoding=\"UTF-8\">"
                "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</BankIdentifier>"
                "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</BuyIdentifier>"
                "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</SaleIdentifier>"
                "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</DividendIdentifier>"
                "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">b</BrokerageIdentifier>"
                "<Document Type=\"Buy\" TypeIdentifierValue=\"Y\" Encoding=\"UTF-8\">"
                "<Wkn Name=\"Wkn\" FoundIndex=\"0\" ResultEmpty=\"true\" RegexOptions=\"None\">y</Wkn>"
                "</Document></Bank>"
                "</Documents>"));
        config.load(path2);
        QCOMPARE(config.count(), 2);
        QVERIFY(config.findByName(QStringLiteral("TestBank")) == nullptr);
    }
};

QTEST_MAIN(TestDocumentsConfig)
#include "tst_documentsconfig.moc"
