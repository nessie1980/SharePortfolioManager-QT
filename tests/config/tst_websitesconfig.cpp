// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "../../app/config/WebSitesConfig.h"

class TestWebSitesConfig : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // Write a WebSites.xml with the given content into m_tempDir
    QString writeXml(const QString& content) const
    {
        const QString path = m_tempDir.path() + QStringLiteral("/WebSites.xml");
        QFile file(path);
        // Treat a failed open as a test precondition failure
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return QString(); // caller will get FileNotFound from load()
        QTextStream stream(&file);
        stream << content;
        return path;
    }

    // Minimal valid XML with one entry and all five required regex elements
    QString validXml() const
    {
        return QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<WebSites>\n"
            "  <WebSite Id=\"www.test.de\" Encoding=\"UTF-8\">\n"
            "    <WebSiteLastDate  Name=\"LastDate\"   FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">([0-9]{4})</WebSiteLastDate>\n"
            "    <WebSiteLastTime  Name=\"LastTime\"   FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">([0-9]{2})</WebSiteLastTime>\n"
            "    <WebSitePrice     Name=\"Price\"      FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"Multiline\">([0-9.]+)</WebSitePrice>\n"
            "    <WebSitePriceBefore Name=\"PriceBefore\" FoundIndex=\"1\" ResultEmpty=\"false\" RegexOptions=\"None\">([0-9.]+)</WebSitePriceBefore>\n"
            "    <WebSiteCurrency  Name=\"Currency\"   FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">([A-Z]{3})</WebSiteCurrency>\n"
            "  </WebSite>\n"
            "</WebSites>\n");
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
    }

    // ── load() — error cases ──────────────────────────────────────────────

    void test_load_fileNotFound()
    {
        WebSitesConfig config;
        const auto result = config.load(
            m_tempDir.path() + QStringLiteral("/nonexistent.xml"));

        QCOMPARE(result, WebSitesConfig::LoadResult::FileNotFound);
        QVERIFY(!config.isValid());
        QVERIFY(!config.lastError().isEmpty());
    }

    void test_load_emptyConfig()
    {
        const QString path = writeXml(
            QStringLiteral("<?xml version=\"1.0\"?><WebSites></WebSites>"));

        WebSitesConfig config;
        QCOMPARE(config.load(path), WebSitesConfig::LoadResult::EmptyConfig);
        QVERIFY(!config.isValid());
    }

    void test_load_malformedXml()
    {
        const QString path = writeXml(
            QStringLiteral("<?xml version=\"1.0\"?><WebSites><WebSite unclosed"));

        WebSitesConfig config;
        QCOMPARE(config.load(path), WebSitesConfig::LoadResult::XmlParseError);
        QVERIFY(!config.isValid());
    }

    void test_load_missingId_returnsAttributeError()
    {
        const QString path = writeXml(
            QStringLiteral(
                "<?xml version=\"1.0\"?><WebSites>"
                "<WebSite Encoding=\"UTF-8\">"
                "<WebSitePrice Name=\"Price\" FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">x</WebSitePrice>"
                "</WebSite></WebSites>"));

        WebSitesConfig config;
        QCOMPARE(config.load(path), WebSitesConfig::LoadResult::AttributeError);
    }

    void test_load_missingRegexName_returnsAttributeError()
    {
        const QString path = writeXml(
            QStringLiteral(
                "<?xml version=\"1.0\"?><WebSites>"
                "<WebSite Id=\"test\" Encoding=\"UTF-8\">"
                "<WebSitePrice FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">x</WebSitePrice>"
                "</WebSite></WebSites>"));

        WebSitesConfig config;
        QCOMPARE(config.load(path), WebSitesConfig::LoadResult::AttributeError);
    }

    // ── load() — success ──────────────────────────────────────────────────

    void test_load_success()
    {
        WebSitesConfig config;
        QCOMPARE(config.load(writeXml(validXml())),
                 WebSitesConfig::LoadResult::Success);
        QVERIFY(config.isValid());
        QCOMPARE(config.count(), 1);
    }

    void test_load_multipleEntries()
    {
        const QString path = writeXml(
            QStringLiteral(
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<WebSites>\n"
                "  <WebSite Id=\"site1\" Encoding=\"UTF-8\">\n"
                "    <WebSitePrice Name=\"Price\" FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">x</WebSitePrice>\n"
                "  </WebSite>\n"
                "  <WebSite Id=\"site2\" Encoding=\"ISO-8859-1\">\n"
                "    <WebSitePrice Name=\"Price\" FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">y</WebSitePrice>\n"
                "  </WebSite>\n"
                "</WebSites>\n"));

        WebSitesConfig config;
        QCOMPARE(config.load(path), WebSitesConfig::LoadResult::Success);
        QCOMPARE(config.count(), 2);
    }

    // ── entry content ─────────────────────────────────────────────────────

    void test_entry_id_and_encoding()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));

        // Copy the entry to avoid a dangling reference to a temporary
        const WebSiteEntry entry = config.entries().first();
        QCOMPARE(entry.id,       QStringLiteral("www.test.de"));
        QCOMPARE(entry.encoding, QStringLiteral("UTF-8"));
    }

    void test_entry_regexList_keys()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));

        const auto& regexList = config.entries().first().regexList;
        QVERIFY(regexList.contains(QStringLiteral("LastDate")));
        QVERIFY(regexList.contains(QStringLiteral("LastTime")));
        QVERIFY(regexList.contains(QStringLiteral("Price")));
        QVERIFY(regexList.contains(QStringLiteral("PriceBefore")));
        QVERIFY(regexList.contains(QStringLiteral("Currency")));
    }

    void test_entry_regexList_values()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));

        const auto& priceRule = config.entries().first().regexList
                                    .value(QStringLiteral("Price"));
        QCOMPARE(priceRule.regexExpression, QStringLiteral("([0-9.]+)"));
        QCOMPARE(priceRule.regexFoundPosition, 0);
        QVERIFY(!priceRule.resultEmpty);
    }

    void test_entry_foundIndex_preserved()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));

        // PriceBefore has FoundIndex="1" in the test XML
        const auto& rule = config.entries().first().regexList
                               .value(QStringLiteral("PriceBefore"));
        QCOMPARE(rule.regexFoundPosition, 1);
    }

    // ── findById ──────────────────────────────────────────────────────────

    void test_findById_found()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));

        const auto* entry = config.findById(QStringLiteral("www.test.de"));
        QVERIFY(entry != nullptr);
        QCOMPARE(entry->id, QStringLiteral("www.test.de"));
    }

    void test_findById_notFound()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));

        QVERIFY(config.findById(QStringLiteral("www.nonexistent.de")) == nullptr);
    }

    // ── parseRegexOptions ─────────────────────────────────────────────────

    void test_regexOptions_none()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));

        // LastDate has RegexOptions="None" → no special options
        const auto& rule = config.entries().first().regexList
                               .value(QStringLiteral("LastDate"));
        QVERIFY(rule.regexOptions.isEmpty() ||
                !rule.regexOptions.contains(QRegularExpression::MultilineOption));
    }

    void test_regexOptions_multiline()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));

        // Price has RegexOptions="Multiline"
        const auto& rule = config.entries().first().regexList
                               .value(QStringLiteral("Price"));
        QVERIFY(rule.regexOptions.contains(QRegularExpression::MultilineOption));
    }

    // ── reload ────────────────────────────────────────────────────────────

    void test_reload_clearsOldEntries()
    {
        WebSitesConfig config;
        config.load(writeXml(validXml()));
        QCOMPARE(config.count(), 1);

        // Load a file with two entries
        const QString path2 = m_tempDir.path() + QStringLiteral("/WebSites2.xml");
        QFile file2(path2);
        QVERIFY(file2.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file2);
        stream << "<?xml version=\"1.0\"?><WebSites>"
               << "<WebSite Id=\"a\" Encoding=\"UTF-8\">"
               << "<WebSitePrice Name=\"Price\" FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">x</WebSitePrice>"
               << "</WebSite>"
               << "<WebSite Id=\"b\" Encoding=\"UTF-8\">"
               << "<WebSitePrice Name=\"Price\" FoundIndex=\"0\" ResultEmpty=\"false\" RegexOptions=\"None\">y</WebSitePrice>"
               << "</WebSite>"
               << "</WebSites>";
        file2.close();

        config.load(path2);
        // Old entries must be gone, only the new two remain
        QCOMPARE(config.count(), 2);
        QVERIFY(config.findById(QStringLiteral("www.test.de")) == nullptr);
    }
};

QTEST_MAIN(TestWebSitesConfig)
#include "tst_websitesconfig.moc"
