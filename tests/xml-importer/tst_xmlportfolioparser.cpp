// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QTemporaryFile>
#include <QDir>

#include "XmlPortfolioParser.h"

class TestXmlPortfolioParser : public QObject
{
    Q_OBJECT

private:
    // Schreibt den übergebenen XML-Inhalt in eine temporäre Datei (Lebensdauer
    // an dieses Testobjekt gebunden) und gibt deren Pfad zurück.
    QString writeTempXml(const QString& content)
    {
        auto* file = new QTemporaryFile(QDir::temp().filePath("tst_xmlparser_XXXXXX.xml"), this);
        file->setAutoRemove(true);
        if (!file->open()) {
            qWarning("Temp-Datei konnte nicht erstellt werden");
            return QString();
        }
        file->write(content.toUtf8());
        file->close();
        return file->fileName();
    }

    // Minimales, aber vollständiges <Share>-Gerüst mit austauschbarem Body.
    QString wrapShare(const QString& attrs, const QString& body) const
    {
        return QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Portfolio>\n"
            "  <Share %1>\n"
            "    <DetailsWebSite></DetailsWebSite>\n"
            "    <StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>\n"
            "    <LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>\n"
            "    <LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>\n"
            "    <SharePrice>10,00</SharePrice>\n"
            "    <SharePriceBefore>9,50</SharePriceBefore>\n"
            "    <Culture></Culture>\n"
            "    <ShareType>0</ShareType>\n"
            "    <MarketValue WebSite=\"\" Parsing=\"\" />\n"
            "    <DailyValues WebSite=\"\" Parsing=\"\"></DailyValues>\n"
            "    <Brokerages></Brokerages>\n"
            "    <Buys></Buys>\n"
            "    <Sales></Sales>\n"
            "    <Dividends></Dividends>\n"
            "    %2\n"
            "  </Share>\n"
            "</Portfolio>\n").arg(attrs, body);
    }

private slots:

    void test_parse_minimalShare_readsBasicAttributesAndFields()
    {
        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<Portfolio>\n"
            "  <Share WKN=\"850663\" ISIN=\"US1912161007\" Name=\"THE COCA-COLA CO.\" Update=\"Both\">\n"
            "    <DetailsWebSite>https://example.com</DetailsWebSite>\n"
            "    <StockMarketLaunchDate>05.09.1919</StockMarketLaunchDate>\n"
            "    <LastUpdateInternet>06.03.2026 14:01</LastUpdateInternet>\n"
            "    <LastUpdateShareDate>06.03.2026 13:38</LastUpdateShareDate>\n"
            "    <SharePrice>66,52</SharePrice>\n"
            "    <SharePriceBefore>66,80</SharePriceBefore>\n"
            "    <Culture>de-DE</Culture>\n"
            "    <ShareType>0</ShareType>\n"
            "    <MarketValue WebSite=\"https://market.example.com\" Parsing=\"ApiYahoo\" />\n"
            "    <DailyValues WebSite=\"https://daily.example.com\" Parsing=\"ApiOnvista\"></DailyValues>\n"
            "    <Brokerages></Brokerages>\n"
            "    <Buys></Buys>\n"
            "    <Sales></Sales>\n"
            "    <Dividends></Dividends>\n"
            "  </Share>\n"
            "</Portfolio>\n");

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        QCOMPARE(portfolio.shares.size(), 1);

        const RawShare& s = portfolio.shares.first();
        QCOMPARE(s.wkn,  QStringLiteral("850663"));
        QCOMPARE(s.isin, QStringLiteral("US1912161007"));
        QCOMPARE(s.name, QStringLiteral("THE COCA-COLA CO."));
        QCOMPARE(s.updateStr, QStringLiteral("Both"));
        QCOMPARE(s.detailsWebSite, QStringLiteral("https://example.com"));
        QCOMPARE(s.stockMarketLaunchDate, QStringLiteral("05.09.1919"));
        QCOMPARE(s.sharePrice, QStringLiteral("66,52"));
        QCOMPARE(s.culture, QStringLiteral("de-DE"));
        QCOMPARE(s.marketValueWebSite, QStringLiteral("https://market.example.com"));
        QCOMPARE(s.marketValueParsing, QStringLiteral("ApiYahoo"));
        QCOMPARE(s.dailyValuesWebSite, QStringLiteral("https://daily.example.com"));
        QCOMPARE(s.dailyValuesParsing, QStringLiteral("ApiOnvista"));
        QVERIFY(s.parseWarnings.isEmpty());
        QVERIFY(s.parseErrors.isEmpty());
    }

    // ── Regressionstests: doppelt-XML-escapte WebSite-URLs (gemeldet 05.07.2026) ─
    // Quell-XML enthielt bei Nvidia/Wacker Chemie "&amp;amp;" statt "&amp;". Ein
    // XML-Parser löst Entities nur einmal auf, daher landet ein literales
    // "&amp;" (statt "&") im geparsten Wert — wird jetzt erkannt und korrigiert.

    void test_parse_marketValueWebSite_doubleEscapedAmpersand_isAutoCorrected()
    {
        const QString body = QStringLiteral(
            "<MarketValue WebSite=\"https://yfapi.net/v6/finance/quote?region=DE&amp;amp;lang=de&amp;amp;symbols=NVD.DE\" "
            "Parsing=\"ApiYahoo\" />\n"
            "<DailyValues WebSite=\"\" Parsing=\"\"></DailyValues>\n"
            "<Brokerages></Brokerages><Buys></Buys><Sales></Sales><Dividends></Dividends>");

        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio><Share WKN=\"NVDA\" ISIN=\"X\" "
            "Name=\"Nvidia\" Update=\"Both\"><DetailsWebSite></DetailsWebSite>"
            "<StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>10,00</SharePrice><SharePriceBefore>9,50</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "%1</Share></Portfolio>").arg(body);

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();

        // Nach dem einmaligen XML-Unescape landet "&amp;lang=de&amp;symbols=NVD.DE"
        // (literal) im Rohwert — das wird hier zu "&" korrigiert.
        QCOMPARE(s.marketValueWebSite,
                 QStringLiteral("https://yfapi.net/v6/finance/quote?region=DE&lang=de&symbols=NVD.DE"));
        QCOMPARE(s.parseWarnings.size(), 1);
        QVERIFY(s.parseWarnings.first().contains(QStringLiteral("MarketValue.WebSite")));
    }

    void test_parse_dailyValuesWebSite_doubleEscapedAmpersand_isAutoCorrected()
    {
        const QString body = QStringLiteral(
            "<MarketValue WebSite=\"\" Parsing=\"\" />\n"
            "<DailyValues WebSite=\"https://api.onvista.de/api/v1/instruments/FUND/114917893/"
            "eod_history?idNotation=9386126&amp;amp;startDate={0}&amp;amp;range={1}\" "
            "Parsing=\"ApiOnvista\"></DailyValues>\n"
            "<Brokerages></Brokerages><Buys></Buys><Sales></Sales><Dividends></Dividends>");

        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio><Share WKN=\"WCH\" ISIN=\"X\" "
            "Name=\"Wacker Chemie\" Update=\"DailyValues\"><DetailsWebSite></DetailsWebSite>"
            "<StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>10,00</SharePrice><SharePriceBefore>9,50</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "%1</Share></Portfolio>").arg(body);

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();

        QCOMPARE(s.dailyValuesWebSite,
                 QStringLiteral("https://api.onvista.de/api/v1/instruments/FUND/114917893/"
                                "eod_history?idNotation=9386126&startDate={0}&range={1}"));
        QCOMPARE(s.parseWarnings.size(), 1);
        QVERIFY(s.parseWarnings.first().contains(QStringLiteral("DailyValues.WebSite")));
    }

    void test_parse_detailsWebSite_doubleEscapedAmpersand_isAutoCorrected()
    {
        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio><Share WKN=\"TESTDW\" ISIN=\"X\" "
            "Name=\"Test\" Update=\"None\">"
            "<DetailsWebSite>https://example.com/page?a=1&amp;amp;b=2</DetailsWebSite>"
            "<StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>1,00</SharePrice><SharePriceBefore>1,00</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "<MarketValue WebSite=\"\" Parsing=\"\" /><DailyValues WebSite=\"\" Parsing=\"\"></DailyValues>"
            "<Brokerages></Brokerages><Buys></Buys><Sales></Sales><Dividends></Dividends></Share>"
            "</Portfolio>");

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();

        QCOMPARE(s.detailsWebSite, QStringLiteral("https://example.com/page?a=1&b=2"));
        QCOMPARE(s.parseWarnings.size(), 1);
        QVERIFY(s.parseWarnings.first().contains(QStringLiteral("DetailsWebSite")));
    }

    void test_parse_singleEscapedAmpersand_isLeftUnchanged_noWarning()
    {
        // Kontrollfall (z.B. BMW.DE aus dem Report vom 05.07.2026): korrekt
        // einfach escapte URL ("&amp;" im Quell-XML -> "&" nach dem Parsen)
        // darf NICHT als Fehler erkannt oder verändert werden.
        const QString body = QStringLiteral(
            "<MarketValue WebSite=\"https://yfapi.net/v6/finance/quote?region=DE&amp;lang=de&amp;symbols=BMW.DE\" "
            "Parsing=\"ApiYahoo\" />\n"
            "<DailyValues WebSite=\"\" Parsing=\"\"></DailyValues>\n"
            "<Brokerages></Brokerages><Buys></Buys><Sales></Sales><Dividends></Dividends>");

        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio><Share WKN=\"BMW\" ISIN=\"X\" "
            "Name=\"BMW\" Update=\"Both\"><DetailsWebSite></DetailsWebSite>"
            "<StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>10,00</SharePrice><SharePriceBefore>9,50</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "%1</Share></Portfolio>").arg(body);

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();

        QCOMPARE(s.marketValueWebSite,
                 QStringLiteral("https://yfapi.net/v6/finance/quote?region=DE&lang=de&symbols=BMW.DE"));
        QVERIFY(s.parseWarnings.isEmpty());
    }

    void test_parse_marketValuePluralTag_isReportedAsErrorNotImported()
    {
        // Regressionstest: In der realen Quell-XML heißt das Element bei
        // Nvidia/Wacker Chemie "<MarketValues>" (Plural) statt "<MarketValue>"
        // (Singular) wie beim Rest des Bestands (32x Singular, 2x Plural laut
        // grep vom 05.07.2026). Das ist ein struktureller Datenfehler in der
        // Quelle, KEIN Synonym — der Parser darf das nicht raten/übernehmen,
        // sondern muss es als Fehler melden und das Feld leer lassen.
        const QString body = QStringLiteral(
            "<MarketValues WebSite=\"https://yfapi.net/v6/finance/quote?region=DE&amp;lang=de&amp;symbols=WCH.DE\" "
            "Parsing=\"ApiYahoo\" />\n"
            "<DailyValues WebSite=\"\" Parsing=\"\"></DailyValues>\n"
            "<Brokerages></Brokerages><Buys></Buys><Sales></Sales><Dividends></Dividends>");

        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio><Share WKN=\"WCH\" ISIN=\"X\" "
            "Name=\"Wacker Chemie\" Update=\"Both\"><DetailsWebSite></DetailsWebSite>"
            "<StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>10,00</SharePrice><SharePriceBefore>9,50</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "%1</Share></Portfolio>").arg(body);

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();

        // NICHT übernommen — bewusst leer, statt geraten/automatisch korrigiert.
        QVERIFY(s.marketValueWebSite.isEmpty());
        QVERIFY(s.marketValueParsing.isEmpty());
        QVERIFY(s.parseWarnings.isEmpty());
        QCOMPARE(s.parseErrors.size(), 1);
        QVERIFY(s.parseErrors.first().contains(QStringLiteral("MarketValues")));
    }

    void test_parse_marketValuePluralTag_doesNotSuppressOtherFieldWarnings()
    {
        // Kombinierter Fall mit den echten Werten aus dem Report vom
        // 05.07.2026 (Nvidia): Plural-Tag bei MarketValue (-> parseErrors)
        // UND doppelt-escaptes Ampersand bei DailyValues (-> parseWarnings)
        // gleichzeitig, aber an unterschiedlichen Feldern — beide müssen
        // unabhängig voneinander erkannt werden.
        const QString body = QStringLiteral(
            "<MarketValues WebSite=\"https://yfapi.net/v6/finance/quote?region=DE&amp;amp;lang=de&amp;amp;symbols=NVD.DE\" "
            "Parsing=\"ApiYahoo\" />\n"
            "<DailyValues WebSite=\"https://api.onvista.de/api/v1/instruments/FUND/114917893/"
            "eod_history?idNotation=9386126&amp;amp;startDate={0}&amp;amp;range={1}\" "
            "Parsing=\"ApiOnvista\"></DailyValues>\n"
            "<Brokerages></Brokerages><Buys></Buys><Sales></Sales><Dividends></Dividends>");

        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio><Share WKN=\"NVDA\" ISIN=\"X\" "
            "Name=\"Nvidia\" Update=\"Both\"><DetailsWebSite></DetailsWebSite>"
            "<StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>10,00</SharePrice><SharePriceBefore>9,50</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "%1</Share></Portfolio>").arg(body);

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();

        // MarketValue: falscher Elementname -> nicht übernommen, ERROR.
        QVERIFY(s.marketValueWebSite.isEmpty());
        QCOMPARE(s.parseErrors.size(), 1);
        QVERIFY(s.parseErrors.first().contains(QStringLiteral("MarketValues")));

        // DailyValues: korrektes Element, nur Ampersand-Problem -> korrigiert, INFO.
        QCOMPARE(s.dailyValuesWebSite,
                 QStringLiteral("https://api.onvista.de/api/v1/instruments/FUND/114917893/"
                                "eod_history?idNotation=9386126&startDate={0}&range={1}"));
        QCOMPARE(s.parseWarnings.size(), 1);
        QVERIFY(s.parseWarnings.first().contains(QStringLiteral("DailyValues.WebSite")));
    }

    void test_parse_dailyValuesEntries_mapsAttributesCorrectly()
    {
        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio><Share WKN=\"TEST03\" ISIN=\"X\" "
            "Name=\"Test\" Update=\"DailyValues\"><DetailsWebSite></DetailsWebSite>"
            "<StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>10,00</SharePrice><SharePriceBefore>9,50</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "<MarketValue WebSite=\"\" Parsing=\"\" />"
            "<DailyValues WebSite=\"https://d.example.com\" Parsing=\"ApiOnvista\">"
            "<Entry D=\"18.08.2015\" C=\"10,50\" O=\"10,00\" T=\"10,80\" B=\"9,90\" V=\"1000\" />"
            "<Entry D=\"19.08.2015\" C=\"10,60\" O=\"10,50\" T=\"10,90\" B=\"10,20\" V=\"1200\" />"
            "</DailyValues><Brokerages></Brokerages><Buys></Buys><Sales></Sales><Dividends></Dividends>"
            "</Share></Portfolio>");

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();
        QCOMPARE(s.dailyValues.size(), 2);
        QCOMPARE(s.dailyValues.first().date,   QStringLiteral("18.08.2015"));
        QCOMPARE(s.dailyValues.first().close,  QStringLiteral("10,50"));
        QCOMPARE(s.dailyValues.first().open,   QStringLiteral("10,00"));
        QCOMPARE(s.dailyValues.first().top,    QStringLiteral("10,80"));
        QCOMPARE(s.dailyValues.first().bottom, QStringLiteral("9,90"));
        QCOMPARE(s.dailyValues.first().volume, QStringLiteral("1000"));
    }

    void test_parse_multipleShares_areAllCollected()
    {
        const QString oneShare = QStringLiteral(
            "<Share WKN=\"%1\" ISIN=\"X\" Name=\"Share %1\" Update=\"None\">"
            "<DetailsWebSite></DetailsWebSite><StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>1,00</SharePrice><SharePriceBefore>1,00</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "<MarketValue WebSite=\"\" Parsing=\"\" /><DailyValues WebSite=\"\" Parsing=\"\"></DailyValues>"
            "<Brokerages></Brokerages><Buys></Buys><Sales></Sales><Dividends></Dividends></Share>");

        const QString xml = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio>%1%2</Portfolio>")
                                .arg(oneShare.arg(QStringLiteral("AAA")), oneShare.arg(QStringLiteral("BBB")));

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        QCOMPARE(portfolio.shares.size(), 2);
        QCOMPARE(portfolio.shares.at(0).wkn, QStringLiteral("AAA"));
        QCOMPARE(portfolio.shares.at(1).wkn, QStringLiteral("BBB"));
    }

    void test_parse_dividendWithForeignCurrency_setsFcFields()
    {
        const QString body = QStringLiteral(
            "<Dividends>"
            "<Dividend Guid=\"div-1\" Date=\"01.07.2024\" Rate=\"0,485\" Volume=\"125\" "
            "TaxAtSource=\"0,00\" CapitalGainsTax=\"12,00\" SolidarityTax=\"0,66\" Price=\"65,00\" Doc=\"\">"
            "<ForeignCurrency Flag=\"Checked\" ExchangeRatio=\"1,08\" FCName=\"en-US\" />"
            "</Dividend>"
            "<Dividend Guid=\"div-2\" Date=\"01.10.2024\" Rate=\"0,50\" Volume=\"100\" "
            "TaxAtSource=\"0,00\" CapitalGainsTax=\"10,00\" SolidarityTax=\"0,55\" Price=\"66,00\" Doc=\"\" />"
            "</Dividends>");

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(wrapShare(
                     QStringLiteral("WKN=\"TEST02\" ISIN=\"X\" Name=\"Test\" Update=\"None\""), body)),
                 portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();
        QCOMPARE(s.dividends.size(), 2);

        const RawDividend& d1 = s.dividends.at(0);
        QVERIFY(d1.hasForeignCurrency);
        QVERIFY(d1.fc.enabled);
        QCOMPARE(d1.fc.exchangeRatio, QStringLiteral("1,08"));
        QCOMPARE(d1.fc.currency, QStringLiteral("en-US"));

        const RawDividend& d2 = s.dividends.at(1);
        QVERIFY(!d2.hasForeignCurrency);
    }

    void test_parse_buysSalesBrokerages_mapsAttributesCorrectly()
    {
        const QString body = QStringLiteral(
            "<Brokerages>"
            "<Brokerage Guid=\"brok-buy-1\" BuyPart=\"True\" SalePart=\"False\" GuidBuySale=\"buy-1\" "
            "Date=\"01.01.2024\" Provision=\"9,90\" BrokerFee=\"0,00\" TraderPlaceFee=\"0,00\" "
            "Reduction=\"0,00\" Doc=\"\" />"
            "<Brokerage Guid=\"brok-sale-1\" BuyPart=\"False\" SalePart=\"True\" GuidBuySale=\"sale-1\" "
            "Date=\"02.01.2024\" Provision=\"9,90\" BrokerFee=\"0,00\" TraderPlaceFee=\"0,00\" "
            "Reduction=\"0,00\" Doc=\"\" />"
            "</Brokerages>"
            "<Buys>"
            "<Buy Guid=\"buy-1\" DepotNumber=\"D1\" OrderNumber=\"ORD-1\" Date=\"01.01.2024\" "
            "Volume=\"10,00\" VolumeSold=\"0,00\" "
            "Price=\"10,00\" BrokerageGuid=\"brok-buy-1\" Doc=\"\" />"
            "</Buys>"
            "<Sales>"
            "<Sale Guid=\"sale-1\" Date=\"02.01.2024\" DepotNumber=\"D1\" OrderNumber=\"ORD-2\" "
            "Volume=\"10,00\" SalePrice=\"11,00\" TaxAtSource=\"0,00\" CapitalGainsTax=\"2,00\" "
            "SolidarityTax=\"0,11\" Reduction=\"0,00\" BrokerageGuid=\"brok-sale-1\" Doc=\"\">"
            "<UsedBuys><UsedBuy BuyDate=\"01.01.2024\" BuyVolume=\"10,00\" BuyPrice=\"10,00\" "
            "Reduction=\"0,00\" Brokerage=\"1,50\" BuyGuid=\"buy-1\" /></UsedBuys>"
            "</Sale>"
            "</Sales>");

        const QString xml = QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Portfolio><Share WKN=\"TEST01\" ISIN=\"X\" "
            "Name=\"Test\" Update=\"Both\"><DetailsWebSite></DetailsWebSite>"
            "<StockMarketLaunchDate>01.01.2000</StockMarketLaunchDate>"
            "<LastUpdateInternet>01.01.2024 10:00</LastUpdateInternet>"
            "<LastUpdateShareDate>01.01.2024 10:00</LastUpdateShareDate>"
            "<SharePrice>10,00</SharePrice><SharePriceBefore>9,50</SharePriceBefore>"
            "<Culture></Culture><ShareType>0</ShareType>"
            "<MarketValue WebSite=\"\" Parsing=\"\" /><DailyValues WebSite=\"\" Parsing=\"\"></DailyValues>"
            "%1<Dividends></Dividends></Share></Portfolio>").arg(body);

        RawPortfolio portfolio;
        QString error;
        QVERIFY2(XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error), qPrintable(error));
        const RawShare& s = portfolio.shares.first();

        QCOMPARE(s.brokerages.size(), 2);
        QCOMPARE(s.buys.size(), 1);
        QCOMPARE(s.sales.size(), 1);

        QCOMPARE(s.buys.first().guid, QStringLiteral("buy-1"));
        QCOMPARE(s.buys.first().orderNumber, QStringLiteral("ORD-1"));
        QCOMPARE(s.buys.first().volume, QStringLiteral("10,00"));

        const RawSale& sale = s.sales.first();
        QCOMPARE(sale.guid, QStringLiteral("sale-1"));
        QCOMPARE(sale.usedBuys.size(), 1);
        QCOMPARE(sale.usedBuys.first().buyGuid, QStringLiteral("buy-1"));
        QCOMPARE(sale.usedBuys.first().brokerage, QStringLiteral("1,50"));

        const RawBrokerage& brokBuy = s.brokerages.at(0);
        QVERIFY(brokBuy.buyPart);
        QVERIFY(!brokBuy.salePart);
        QCOMPARE(brokBuy.guidBuySale, QStringLiteral("buy-1"));

        const RawBrokerage& brokSale = s.brokerages.at(1);
        QVERIFY(!brokSale.buyPart);
        QVERIFY(brokSale.salePart);
        QCOMPARE(brokSale.guidBuySale, QStringLiteral("sale-1"));
    }

    void test_parse_missingRootElement_fails()
    {
        const QString xml = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?><NotAPortfolio></NotAPortfolio>");
        RawPortfolio portfolio;
        QString error;
        QVERIFY(!XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error));
        QVERIFY(!error.isEmpty());
    }

    void test_parse_fileNotFound_fails()
    {
        RawPortfolio portfolio;
        QString error;
        QVERIFY(!XmlPortfolioParser::parse(QStringLiteral("/nonexistent/path/does-not-exist.xml"), portfolio, error));
        QVERIFY(!error.isEmpty());
    }

    void test_parse_malformedXml_fails()
    {
        const QString xml = QStringLiteral("<Portfolio><Share WKN=\"X\"></Portfolio>"); // Share nicht geschlossen
        RawPortfolio portfolio;
        QString error;
        QVERIFY(!XmlPortfolioParser::parse(writeTempXml(xml), portfolio, error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(TestXmlPortfolioParser)
#include "tst_xmlportfolioparser.moc"
